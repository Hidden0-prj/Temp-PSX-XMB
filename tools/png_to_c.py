import sys
from PIL import Image

def png_to_c(input_png, output_c):
    img = Image.open(input_png).convert("RGB")
    width, height = img.size

    words = []
    for y in range(height):
        for x in range(0, width, 2):
            # Process left pixel (15-bit BGR + STP Alpha bit)
            r1, g1, b1 = img.getpixel((x, y))
            stp1 = 1 if (r1 | g1 | b1) > 0 else 0
            p1 = (stp1 << 15) | ((b1 >> 3) << 10) | ((g1 >> 3) << 5) | (r1 >> 3)
            
            # Process right pixel
            if x + 1 < width:
                r2, g2, b2 = img.getpixel((x + 1, y))
                stp2 = 1 if (r2 | g2 | b2) > 0 else 0
                p2 = (stp2 << 15) | ((b2 >> 3) << 10) | ((g2 >> 3) << 5) | (r2 >> 3)
            else:
                p2 = 0
                
            # Pack two 16-bit pixels into one 32-bit hardware word
            words.append((p2 << 16) | p1)

    with open(output_c, "w") as f:
        f.write("#include <stdint.h>\n\n")
        f.write(f"const uint32_t diss_00_width = {width};\n")
        f.write(f"const uint32_t diss_00_height = {height};\n")
        f.write("const uint32_t diss_00_pixels[] = {\n")
        for w in words:
            f.write(f"    0x{w:08X},\n")
        f.write("};\n")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 png_to_c.py <input.png> <output.c>")
        sys.exit(1)
    png_to_c(sys.argv[1], sys.argv[2])