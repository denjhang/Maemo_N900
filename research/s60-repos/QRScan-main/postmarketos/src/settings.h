#pragma once
#include <glib.h>

gboolean qrscan_settings_get_haptics     (void);
void     qrscan_settings_set_haptics     (gboolean val);
gboolean qrscan_settings_get_save_history(void);
void     qrscan_settings_set_save_history(gboolean val);
