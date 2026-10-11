#include "util.h"
#include <glib.h>
#include <time.h>

gchar *qrscan_util_format_timestamp(gint64 unix_time) {
    time_t t = (time_t)unix_time;
    struct tm *tm_info = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", tm_info);
    return g_strdup(buf);
}
