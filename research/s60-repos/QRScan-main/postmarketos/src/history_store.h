#pragma once
#include <glib.h>

typedef struct {
    gint64   id;
    gchar   *content;
    gchar   *format;
    gint64   timestamp;
    gboolean favorite;
    gchar   *label;
} QRScanEntry;

void       qrscan_history_store_add               (const gchar *content, const gchar *format);
GPtrArray *qrscan_history_store_get_all           (void);
void       qrscan_history_store_set_favorite      (gint64 id, gboolean fav);
void       qrscan_history_store_delete            (gint64 id);
void       qrscan_history_store_clear_non_favorites (void);
void       qrscan_scan_entry_free                 (QRScanEntry *entry);
