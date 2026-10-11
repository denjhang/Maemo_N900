#include "app.h"
#include "scanner_window.h"

struct _QRScanApp {
    GtkApplication parent_instance;
};

G_DEFINE_TYPE(QRScanApp, qrscan_app, GTK_TYPE_APPLICATION)

static void qrscan_app_activate(GApplication *app) {
    GtkWindow *win = gtk_application_get_active_window(GTK_APPLICATION(app));
    if (!win) {
        win = GTK_WINDOW(qrscan_scanner_window_new(GTK_APPLICATION(app)));
    }
    gtk_window_present(win);
}

static void qrscan_app_class_init(QRScanAppClass *klass) {
    G_APPLICATION_CLASS(klass)->activate = qrscan_app_activate;
}

static void qrscan_app_init(QRScanApp *self) {
    (void)self;
}

QRScanApp *qrscan_app_new(void) {
    return g_object_new(
        QRSCAN_TYPE_APP,
        "application-id", "com.hexadecinull.QRScan",
        "flags", G_APPLICATION_DEFAULT_FLAGS,
        NULL
    );
}
