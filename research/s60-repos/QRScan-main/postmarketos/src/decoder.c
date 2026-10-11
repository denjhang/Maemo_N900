#include "decoder.h"
#include <zbar.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>

gchar *qrscan_decoder_decode_rgb(const uint8_t *data,
                                  int            width,
                                  int            height,
                                  int            stride) {
    if (!data || width <= 0 || height <= 0) return NULL;

    zbar_image_scanner_t *scanner = zbar_image_scanner_create();
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);

    uint8_t *grey = g_malloc(width * height);
    for (int y = 0; y < height; y++) {
        const uint8_t *row = data + y * stride;
        uint8_t *dst = grey + y * width;
        for (int x = 0; x < width; x++) {
            uint8_t r = row[x * 3 + 0];
            uint8_t g = row[x * 3 + 1];
            uint8_t b = row[x * 3 + 2];
            dst[x] = (uint8_t)((r * 77 + g * 151 + b * 28) >> 8);
        }
    }

    zbar_image_t *image = zbar_image_create();
    zbar_image_set_format(image, zbar_fourcc('Y', '8', '0', '0'));
    zbar_image_set_size(image, (unsigned)width, (unsigned)height);
    zbar_image_set_data(image, grey, (unsigned long)(width * height), zbar_image_free_data);

    gchar *result = NULL;
    int n = zbar_scan_image(scanner, image);
    if (n > 0) {
        const zbar_symbol_t *sym = zbar_image_first_symbol(image);
        if (sym) {
            const char *data_str = zbar_symbol_get_data(sym);
            if (data_str) result = g_strdup(data_str);
        }
    }

    zbar_image_destroy(image);
    zbar_image_scanner_destroy(scanner);
    return result;
}

gchar *qrscan_decoder_decode_file(const gchar *path) {
    if (!path) return NULL;

    GError    *err    = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &err);
    if (!pixbuf) { g_clear_error(&err); return NULL; }

    int      width      = gdk_pixbuf_get_width(pixbuf);
    int      height     = gdk_pixbuf_get_height(pixbuf);
    int      rowstride  = gdk_pixbuf_get_rowstride(pixbuf);
    int      channels   = gdk_pixbuf_get_n_channels(pixbuf);
    uint8_t *pixels     = gdk_pixbuf_get_pixels(pixbuf);

    uint8_t *rgb = NULL;
    int      rgb_stride = width * 3;

    if (channels == 3) {
        rgb = g_malloc(height * rgb_stride);
        for (int y = 0; y < height; y++) {
            memcpy(rgb + y * rgb_stride, pixels + y * rowstride, (size_t)(width * 3));
        }
    } else if (channels == 4) {
        rgb = g_malloc(height * rgb_stride);
        for (int y = 0; y < height; y++) {
            const uint8_t *src = pixels + y * rowstride;
            uint8_t       *dst = rgb    + y * rgb_stride;
            for (int x = 0; x < width; x++) {
                dst[x * 3 + 0] = src[x * 4 + 0];
                dst[x * 3 + 1] = src[x * 4 + 1];
                dst[x * 3 + 2] = src[x * 4 + 2];
            }
        }
    }

    gchar *result = NULL;
    if (rgb) {
        result = qrscan_decoder_decode_rgb(rgb, width, height, rgb_stride);
        g_free(rgb);
    }

    g_object_unref(pixbuf);
    return result;
}
