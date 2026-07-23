#include <sys/types.h>
#include <stdio.h>
#include <psxetc.h>
#include <psxgte.h>
#include <psxgpu.h>

#define OT_LEN 256
#define MESH_W 22
#define MESH_H 8

// Double buffering structure
typedef struct {
    DISPENV disp;
    DRAWENV draw;
    u_long  ot[OT_LEN];
    char    packetArea[4096 * 6];
} DB;

DB db[2];
int db_idx = 0;
u_long *cdb_ot;

// Simple fixed-point Sine Table (0-255 mapped to 0-360 deg, scaled by 4096)
static const short sintable[64] = {
    0, 100, 201, 301, 401, 501, 601, 700, 799, 897, 994, 1089, 1183, 1275, 1365, 1454,
    1540, 1624, 1706, 1785, 1861, 1934, 2004, 2071, 2134, 2194, 2250, 2303, 2351, 2355, 2435, 2470
};

// Quick sine lookup helper
short get_sin(int angle) {
    angle = (angle & 255);
    if (angle < 64)  return sintable[angle];
    if (angle < 128) return sintable[128 - 1 - angle];
    if (angle < 192) return -sintable[angle - 128];
    return -sintable[256 - 1 - angle];
}

// Upload texture data generated from diss_00.png using png_to_c
extern u_long diss_00_tim[];

TIM_IMAGE tim_wave;
u_short tpage_id;
u_short clut_id;

void load_texture(void) {
    GetTimInfo(diss_00_tim, &tim_wave);
    
    // Load pixel data to VRAM
    if (tim_wave.prect) {
        LoadImage(tim_wave.prect, tim_wave.paddr);
        DrawSync(0);
    }
    
    // Load CLUT if palette exists
    if (tim_wave.crect) {
        LoadImage(tim_wave.crect, tim_wave.caddr);
        DrawSync(0);
    }

    tpage_id = getTPage(tim_wave.mode & 0x3, 1, tim_wave.prect->x, tim_wave.prect->y);
    if (tim_wave.crect) {
        clut_id = getClut(tim_wave.crect->x, tim_wave.crect->y);
    }
}

void init_ps1(void) {
    ResetGraph(0);
    
    // Initialize double buffer framebuffers
    SetDefDispenv(&db[0].disp, 0, 0, 320, 240);
    SetDefDrawenv(&db[0].draw, 0, 240, 320, 240);
    SetDefDispenv(&db[1].disp, 0, 240, 320, 240);
    SetDefDrawenv(&db[1].draw, 0, 0, 320, 240);

    db[0].draw.isbg = 1;
    setRGB0(&db[0].draw, 10, 15, 35); // Dark blue XMB ambient background
    db[1].draw.isbg = 1;
    setRGB0(&db[1].draw, 10, 15, 35);

    PutDispenv(&db[0].disp);
    PutDrawenv(&db[0].draw);

    InitGeom();
    gte_SetGeomOffset(160, 120); // Center of 320x240 screen
    gte_SetGeomScreen(240);       // FOV
}

void draw_psp_wave(int frame) {
    SVECTOR v[4];
    DVECTOR s[4];
    long p, flag;
    POLY_FT4 *poly;
    
    char *next_packet = db[db_idx].packetArea;

    // Define mesh dimensions
    int x_step = 400 / (MESH_W - 1);
    int z_step = 200 / (MESH_H - 1);
    int start_x = -200;
    int start_z = 300;

    for (int iz = 0; iz < MESH_H - 1; iz++) {
        for (int ix = 0; ix < MESH_W - 1; ix++) {
            
            // Calculate 3D grid wave displacement (Y value) using sin function
            #define CALC_Y(x, z) ( (get_sin((x)*2 + (z)*3 + frame*4) * 35) >> 12 ) + \
                                 ( (get_sin((x)*4 - frame*2) * 15) >> 12 )

            int x0 = start_x + ix * x_step;
            int x1 = start_x + (ix + 1) * x_step;
            int z0 = start_z + iz * z_step;
            int z1 = start_z + (iz + 1) * z_step;

            // Define quad vertex world coordinates
            v[0].vx = x0; v[0].vy = CALC_Y(ix, iz);     v[0].vz = z0;
            v[1].vx = x1; v[1].vy = CALC_Y(ix+1, iz);   v[1].vz = z0;
            v[2].vx = x0; v[2].vy = CALC_Y(ix, iz+1);   v[2].vz = z1;
            v[3].vx = x1; v[3].vy = CALC_Y(ix+1, iz+1); v[3].vz = z1;

            // GTE Perspective transformation
            gte_ldv3(&v[0], &v[1], &v[2]);
            gte_rtpt();
            gte_stsxy3(&s[0], &s[1], &s[2]);

            gte_ldv0(&v[3]);
            gte_rtps();
            gte_stsxy(&s[3]);
            gte_avsz4();
            gte_stotz(&p);

            // Depth push to Ordering Table
            if (p > 0 && p < OT_LEN) {
                poly = (POLY_FT4 *)next_packet;
                next_packet += sizeof(POLY_FT4);

                setPolyFT4(poly);
                setSemiTrans(poly, 1); // Enable semi-transparency

                // Color tint for wave highlights
                setRGB0(poly, 120, 180, 255);

                // Screen coordinates from GTE
                poly->x0 = s[0].vx; poly->y0 = s[0].vy;
                poly->x1 = s[1].vx; poly->y1 = s[1].vy;
                poly->x2 = s[2].vx; poly->y2 = s[2].vy;
                poly->x3 = s[3].vx; poly->y3 = s[3].vy;

                // Map UV coordinates along texture gradient
                poly->u0 = 0;  poly->v0 = 0;
                poly->u1 = 255; poly->v1 = 0;
                poly->u2 = 0;  poly->v2 = 255;
                poly->u3 = 255; poly->v3 = 255;

                poly->tpage = tpage_id;
                poly->clut = clut_id;

                addPrim(&cdb_ot[p], poly);
            }
        }
    }
}

int main(void) {
    int frame = 0;

    init_ps1();
    load_texture();

    while (1) {
        db_idx = !db_idx;
        cdb_ot = db[db_idx].ot;

        ClearOTagR(cdb_ot, OT_LEN);

        // Draw animated XMB ribbon
        draw_psp_wave(frame++);

        DrawSync(0);
        VSync(0);

        PutDispenv(&db[db_idx].disp);
        PutDrawenv(&db[db_idx].draw);

        DrawOTag(&db[db_idx].ot[OT_LEN - 1]);
    }

    return 0;
}