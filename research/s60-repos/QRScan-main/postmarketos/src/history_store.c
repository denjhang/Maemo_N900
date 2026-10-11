#include "history_store.h"
#include <glib.h>
#include <sqlite3.h>
#include <string.h>
#include <time.h>

static sqlite3 *db = NULL;

static void ensure_db(void) {
    if (db) return;
    gchar *dir = g_build_filename(g_get_user_data_dir(), "qrscan", NULL);
    g_mkdir_with_parents(dir, 0700);
    gchar *path = g_build_filename(dir, "history.db", NULL);
    g_free(dir);
    sqlite3_open(path, &db);
    g_free(path);
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS scans ("
        "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  content   TEXT NOT NULL,"
        "  format    TEXT NOT NULL,"
        "  timestamp INTEGER NOT NULL,"
        "  favorite  INTEGER NOT NULL DEFAULT 0,"
        "  label     TEXT NOT NULL DEFAULT ''"
        ");",
        NULL, NULL, NULL);
}

void qrscan_history_store_add(const gchar *content, const gchar *format) {
    ensure_db();
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
        "INSERT INTO scans (content, format, timestamp) VALUES (?, ?, ?);",
        -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, format,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)time(NULL));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

GPtrArray *qrscan_history_store_get_all(void) {
    ensure_db();
    GPtrArray    *arr  = g_ptr_array_new_with_free_func((GDestroyNotify)qrscan_scan_entry_free);
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
        "SELECT id, content, format, timestamp, favorite, label "
        "FROM scans ORDER BY timestamp DESC LIMIT 500;",
        -1, &stmt, NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QRScanEntry *e = g_new0(QRScanEntry, 1);
        e->id        = (gint64) sqlite3_column_int64(stmt, 0);
        e->content   = g_strdup((const gchar *) sqlite3_column_text(stmt, 1));
        e->format    = g_strdup((const gchar *) sqlite3_column_text(stmt, 2));
        e->timestamp = (gint64) sqlite3_column_int64(stmt, 3);
        e->favorite  = (gboolean) sqlite3_column_int(stmt, 4);
        e->label     = g_strdup((const gchar *) sqlite3_column_text(stmt, 5));
        g_ptr_array_add(arr, e);
    }
    sqlite3_finalize(stmt);
    return arr;
}

void qrscan_history_store_set_favorite(gint64 id, gboolean fav) {
    ensure_db();
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
        "UPDATE scans SET favorite = ? WHERE id = ?;",
        -1, &stmt, NULL);
    sqlite3_bind_int(stmt,   1, fav ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void qrscan_history_store_delete(gint64 id) {
    ensure_db();
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "DELETE FROM scans WHERE id = ?;", -1, &stmt, NULL);
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void qrscan_history_store_clear_non_favorites(void) {
    ensure_db();
    sqlite3_exec(db, "DELETE FROM scans WHERE favorite = 0;", NULL, NULL, NULL);
}

void qrscan_scan_entry_free(QRScanEntry *entry) {
    if (!entry) return;
    g_free(entry->content);
    g_free(entry->format);
    g_free(entry->label);
    g_free(entry);
}
