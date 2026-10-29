
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

/* Convert 8-bit RGB values into the framebuffer's pixel format. */
static uint32_t pixel_value(unsigned r, unsigned g, unsigned b,
                            struct fb_var_screeninfo *v)
{
    uint32_t pixel = 0;

    struct fb_bitfield fields[] = {v->red, v->green, v->blue};
    unsigned values[] = {r, g, b};

    for (int i = 0; i < 3; i++) {
        struct fb_bitfield f = fields[i];
        if (!f.length)
            continue;

        uint32_t max = (1u << f.length) - 1u;
        uint32_t value = values[i] * max / 255u;

        if (f.msb_right) {
            uint32_t reversed = 0;
            for (unsigned j = 0; j < f.length; j++)
                reversed = (reversed << 1) | ((value >> j) & 1u);
            value = reversed;
        }

        pixel |= value << f.offset;
    }

    if (v->transp.length) {
        uint32_t max = (1u << v->transp.length) - 1u;
        pixel |= max << v->transp.offset;
    }

    return pixel;
}

int main(int argc, char **argv)
{
    const char *device = argc > 1 ? argv[1] : "/dev/fb1";

    int fd = open(device, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", device, strerror(errno));
        return 1;
    }

    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0 ||
        ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) {
        perror("Framebuffer information");
        close(fd);
        return 1;
    }

    if (var.bits_per_pixel != 16 && var.bits_per_pixel != 24 &&
        var.bits_per_pixel != 32) {
        fprintf(stderr, "Unsupported pixel depth: %u bpp\n",
                var.bits_per_pixel);
        close(fd);
        return 1;
    }

    unsigned bytes = var.bits_per_pixel / 8;
    struct fb_bitfield fields[] = {var.red, var.green, var.blue, var.transp};
    for (unsigned i = 0; i < 4; i++) {
        if (fields[i].length > 8 || fields[i].offset > var.bits_per_pixel ||
            fields[i].length > var.bits_per_pixel - fields[i].offset) {
            fprintf(stderr, "Unsupported color bitfield\n");
            close(fd);
            return 1;
        }
    }
    if (!var.xres || !var.yres ||
        (uint64_t)var.xoffset + var.xres > var.xres_virtual ||
        (uint64_t)var.yoffset + var.yres > var.yres_virtual ||
        ((uint64_t)var.xoffset + var.xres) * bytes > fix.line_length ||
        ((uint64_t)var.yoffset + var.yres - 1) * fix.line_length +
        ((uint64_t)var.xoffset + var.xres) * bytes > fix.smem_len) {
        fprintf(stderr, "Framebuffer geometry exceeds mapped memory\n");
        close(fd);
        return 1;
    }

    size_t length = fix.smem_len;
    if (length == 0) {
        fprintf(stderr, "Framebuffer reports zero memory size\n");
        close(fd);
        return 1;
    }

    unsigned char *fb = mmap(NULL, length, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        perror("mmap framebuffer");
        close(fd);
        return 1;
    }

    printf("Device: %s\n", device);
    printf("Resolution: %u x %u (virtual %u x %u)\n",
           var.xres, var.yres, var.xres_virtual, var.yres_virtual);
    printf("Pixel depth: %u bpp; line stride: %u bytes\n",
           var.bits_per_pixel, fix.line_length);

    uint32_t blue = pixel_value(0, 0, 255, &var);
    for (unsigned y = 0; y < var.yres; y++) {
        unsigned char *row =
            fb + (size_t)(y + var.yoffset) * fix.line_length;

        for (unsigned x = 0; x < var.xres; x++) {
            unsigned char *p = row + (size_t)(x + var.xoffset) * bytes;

            for (unsigned i = 0; i < bytes; i++)
                p[i] = (blue >> (8 * i)) & 0xff;
        }
    }

    if (msync(fb, length, MS_SYNC) < 0)
        perror("msync (display may still have updated)");

    printf("Blue fill written.\n");
    printf("The display may retain the last frame after this process exits.\n");

    munmap(fb, length);
    close(fd);
    return 0;
}

