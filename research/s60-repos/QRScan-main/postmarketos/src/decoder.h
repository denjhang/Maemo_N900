#pragma once
#include <glib.h>
#include <stdint.h>

gchar *qrscan_decoder_decode_rgb  (const uint8_t *data, int width, int height, int stride);
gchar *qrscan_decoder_decode_file (const gchar *path);
