#pragma once
#include <gtk/gtk.h>

#define QRSCAN_TYPE_APP (qrscan_app_get_type())
G_DECLARE_FINAL_TYPE(QRScanApp, qrscan_app, QRSCAN, APP, GtkApplication)

QRScanApp *qrscan_app_new(void);
