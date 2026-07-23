#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------
// PS1 Bare Metal Hardware Ports
// ---------------------------------------------------------
#define GP0 (*(volatile uint32_t *)0x1F801810)
#define GP1 (*(volatile uint32_t *)0x1F801814)

extern const uint32_t diss_00_width;
extern const uint32_t diss_00_height;
extern const uint32_t diss_00_pixels[];

// Load the 16-bit texture directly into VRAM
static void load_texture() {
    GP0 = 0xA0000000;                       // GPU Command: Copy CPU to VRAM
    GP0 = (0 << 16) | 640;                  // VRAM Dest: X=640, Y=0
    GP0 = (diss_00_height << 16) | diss_00_width;
    
    uint32_t num_words = (diss_00_width * diss_00_height) / 2;
    for(uint32_t i = 0; i < num_words; i++) {
        GP0 = diss_00_pixels[i];
    }
}

// Blocks execution until vertical retrace finishes (Prevents screen tearing)
static void wait_vblank() {
    while ((GP1 & (1 << 11)) != 0); // Wait until out of VBlank
    while ((GP1 & (1 << 11)) == 0); // Wait until next VBlank begins
}

// ---------------------------------------------------------
// Sine Table & 3D Math Logic 
// ---------------------------------------------------------
static const int16_t sintable[64] = {
    0, 100, 201, 301, 401, 501, 601, 700, 799, 897, 994, 1089, 1183, 1275, 1365, 1454,
    1540, 1624, 1706, 1785, 1861, 1934, 2004, 2071, 2134, 2194, 2250, 2303, 2351, 2395, 2435, 2470,
    2500, 2525, 2545, 2560, 2570, 2575, 2575, 2570, 2560, 2545, 2525, 2500, 2470, 2435, 2395, 2351,
    2303, 2250, 2194, 2134, 2071, 2004, 1934, 1861, 1785, 1706, 1624, 1540, 1454, 1365, 1275, 1183
};

static int16_t get_sin(int angle) {
    angle = angle & 255;
    if (angle < 64)  return sintable[angle];
    if (angle < 128) return sintable[128 - 1 - angle];
    if (angle < 192) return -sintable[angle - 128];
    return -sintable[256 - 1 - angle];
}

#define FOCAL_LENGTH 240
#define SCREEN_W 320
#define SCREEN_H 240
#define MESH_W 22
#define MESH_H 10

// ---------------------------------------------------------
// Main Executable
// ---------------------------------------------------------
int main() {
    GP1 = 0x00000000; // Reset GPU
    GP1 = 0x03000000; // Display Enable
    GP1 = 0x08000001; // 320x240, NTSC, 15bpp
    GP1 = 0x06C60260; // Horizontal range
    GP1 = 0x07040010; // Vertical range

    load_texture();

    int draw_buffer = 0;
    int frame = 0;

    int x_step = 420 / (MESH_W - 1);
    int z_step = 240 / (MESH_H - 1);
    int start_x = -210;
    int start_z = 220;

    while (1) {
        wait_vblank();
        
        // -------------------------------------------------
        // Hardware Double Buffering
        // -------------------------------------------------
        if (draw_buffer == 0) {
            GP1 = 0x05000000; // Display Top (Y=0)
            GP0 = 0xE30F0000; // Draw Area Top Left Y=240
            GP0 = 0xE4077D3F; // Draw Area Bot Right 319, 479
            GP0 = 0xE5078000; // Draw Offset Y=240
        } else {
            GP1 = 0x050F0000; // Display Bottom (Y=240)
            GP0 = 0xE3000000; // Draw Area Top Left Y=0
            GP0 = 0xE403BD3F; // Draw Area Bot Right 319, 239
            GP0 = 0xE5000000; // Draw Offset Y=0
        }
        
        // Clear Active Draw Area (Deep ambient blue XMB color)
        GP0 = 0x02081020; 
        GP0 = (draw_buffer == 0) ? 0x00F00000 : 0x00000000;
        GP0 = (240 << 16) | 320; 

        // Set Global Draw Mode: T-Page X=640 (10), Mode=16-bit, ABR=1 (Additive Blending)
        GP0 = 0xE100012A;

        uint32_t u_max = diss_00_width - 1;
        uint32_t v_max = diss_00_height - 1;

        // -------------------------------------------------
        // Wave Generation & Rendering
        // -------------------------------------------------
        for (int iz = 0; iz < MESH_H - 1; iz++) {
            for (int ix = 0; ix < MESH_W - 1; ix++) {
                
                // Calculate 3D undulations based on the PSP model curves
                #define CALC_Y(x, z) ( (get_sin((x)*12 + (z)*16 + frame*5) * 28) >> 12 ) + \
                                     ( (get_sin((x)*20 - frame*3) * 12) >> 12 )

                int x0 = start_x + ix * x_step;
                int z0 = start_z + iz * z_step;
                int y0 = CALC_Y(ix, iz);
                
                int x1 = start_x + (ix + 1) * x_step;
                int z1 = start_z + iz * z_step;
                int y1 = CALC_Y(ix+1, iz);
                
                int x2 = start_x + ix * x_step;
                int z2 = start_z + (iz + 1) * z_step;
                int y2 = CALC_Y(ix, iz+1);
                
                int x3 = start_x + (ix + 1) * x_step;
                int z3 = start_z + (iz + 1) * z_step;
                int y3 = CALC_Y(ix+1, iz+1);
                
                // Pure software projection
                int sx0 = (x0 * FOCAL_LENGTH) / z0 + (SCREEN_W / 2);
                int sy0 = (y0 * FOCAL_LENGTH) / z0 + (SCREEN_H / 2);
                int sx1 = (x1 * FOCAL_LENGTH) / z1 + (SCREEN_W / 2);
                int sy1 = (y1 * FOCAL_LENGTH) / z1 + (SCREEN_H / 2);
                int sx2 = (x2 * FOCAL_LENGTH) / z2 + (SCREEN_W / 2);
                int sy2 = (y2 * FOCAL_LENGTH) / z2 + (SCREEN_H / 2);
                int sx3 = (x3 * FOCAL_LENGTH) / z3 + (SCREEN_W / 2);
                int sy3 = (y3 * FOCAL_LENGTH) / z3 + (SCREEN_H / 2);
                
                // Apply Y offset for double buffer rendering
                sy0 += (draw_buffer == 0) ? 240 : 0;
                sy1 += (draw_buffer == 0) ? 240 : 0;
                sy2 += (draw_buffer == 0) ? 240 : 0;
                sy3 += (draw_buffer == 0) ? 240 : 0;
                
                // Write Textured, Semi-Transparent Quad to GPU
                GP0 = 0x2E000000 | 140 | (200 << 8) | (255 << 16); // Soft glowing blue tint
                GP0 = (sx0 & 0xffff) | (sy0 << 16);
                GP0 = 0 | (0 << 8);                                // u0, v0
                GP0 = (sx1 & 0xffff) | (sy1 << 16);
                GP0 = u_max | (0 << 8);                            // u1, v1
                GP0 = (sx2 & 0xffff) | (sy2 << 16);
                GP0 = 0 | (v_max << 8);                            // u2, v2
                GP0 = (sx3 & 0xffff) | (sy3 << 16);
                GP0 = u_max | (v_max << 8);                        // u3, v3
            }
        }
        
        draw_buffer = !draw_buffer;
        frame++;
    }
    return 0;
}