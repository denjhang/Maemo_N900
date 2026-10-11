#pragma once
#include <gtk/gtk.h>

#define QRSCAN_TYPE_SCANNER_WINDOW (qrscan_scanner_window_get_type())
G_DECLARE_FINAL_TYPE(QRScanScannerWindow, qrscan_scanner_window,
                     QRSCAN, SCANNER_WINDOW, GtkApplicationWindow)

GtkWidget *qrscan_scanner_window_new(GtkApplication *app);
