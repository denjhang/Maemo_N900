#include "settings.h"
#include <gio/gio.h>

static GSettings *get_settings(void) {
    static GSettings *s = NULL;
    if (!s) s = g_settings_new("com.hexadecinull.QRScan");
    return s;
}

gboolean qrscan_settings_get_haptics(void) {
    return g_settings_get_boolean(get_settings(), "haptics");
}

void qrscan_settings_set_haptics(gboolean val) {
    g_settings_set_boolean(get_settings(), "haptics", val);
}

gboolean qrscan_settings_get_save_history(void) {
    return g_settings_get_boolean(get_settings(), "save-history");
}

void qrscan_settings_set_save_history(gboolean val) {
    g_settings_set_boolean(get_settings(), "save-history", val);
}
