#include "scanner_window.h"
#include "camera_pipeline.h"
#include "decoder.h"
#include "result_dialog.h"
#include "history_store.h"

#include <gtk/gtk.h>
#include <gst/gst.h>

struct _QRScanScannerWindow {
    GtkApplicationWindow  parent_instance;

    GtkWidget            *stack;
    GtkWidget            *video_widget;
    GtkWidget            *flash_btn;
    GtkWidget            *flip_btn;
    GtkWidget            *pick_btn;
    GtkWidget            *history_btn;
    GtkWidget            *create_btn;
    GtkWidget            *zoom_scale;
    GtkWidget            *header_bar;
    GtkWidget            *toast_overlay;

    QRScanCameraPipeline *pipeline;
    gboolean              flash_on;
    gboolean              scanning;
};

G_DEFINE_TYPE(QRScanScannerWindow, qrscan_scanner_window, GTK_TYPE_APPLICATION_WINDOW)

static void on_scan_result(QRScanCameraPipeline *pipeline,
                            const gchar          *text,
                            const gchar          *format,
                            gpointer              user_data) {
    QRScanScannerWindow *self = QRSCAN_SCANNER_WINDOW(user_data);
    if (!self->scanning) return;
    self->scanning = FALSE;

    qrscan_history_store_add(text, format);
    qrscan_result_dialog_show(GTK_WINDOW(self), text, format);

    self->scanning = TRUE;
}

static void on_flash_clicked(GtkButton *btn, gpointer user_data) {
    QRScanScannerWindow *self = QRSCAN_SCANNER_WINDOW(user_data);
    self->flash_on = !self->flash_on;
    qrscan_camera_pipeline_set_torch(self->pipeline, self->flash_on);

    const gchar *icon = self->flash_on
        ? "weather-clear-symbolic"
        : "weather-clear-night-symbolic";
    gtk_button_set_icon_name(btn, icon);
}

static void on_zoom_changed(GtkRange *range, gpointer user_data) {
    QRScanScannerWindow *self = QRSCAN_SCANNER_WINDOW(user_data);
    gdouble val = gtk_range_get_value(range);
    qrscan_camera_pipeline_set_zoom(self->pipeline, val);
}

static void on_pick_clicked(GtkButton *btn, gpointer user_data) {
    QRScanScannerWindow *self = QRSCAN_SCANNER_WINDOW(user_data);

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open image");

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/webp");
    gtk_file_filter_add_mime_type(filter, "image/bmp");

    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

    gtk_file_dialog_open(dialog, GTK_WINDOW(self), NULL,
        (GAsyncReadyCallback) on_file_dialog_done, self);

    g_object_unref(filter);
    g_object_unref(filters);
    g_object_unref(dialog);
}

static void on_file_dialog_done(GtkFileDialog *dialog,
                                 GAsyncResult  *res,
                                 gpointer       user_data) {
    QRScanScannerWindow *self = QRSCAN_SCANNER_WINDOW(user_data);
    GError *err = NULL;
    GFile  *file = gtk_file_dialog_open_finish(dialog, res, &err);
    if (!file) { g_clear_error(&err); return; }

    gchar *path = g_file_get_path(file);
    g_object_unref(file);

    gchar *result = qrscan_decoder_decode_file(path);
    g_free(path);

    if (result) {
        qrscan_history_store_add(result, "QR_CODE");
        qrscan_result_dialog_show(GTK_WINDOW(self), result, "QR_CODE");
        g_free(result);
    } else {
        GtkAlertDialog *alert = gtk_alert_dialog_new("No QR code found in this image.");
        gtk_alert_dialog_show(alert, GTK_WINDOW(self));
        g_object_unref(alert);
    }
}

static void qrscan_scanner_window_dispose(GObject *obj) {
    QRScanScannerWindow *self = QRSCAN_SCANNER_WINDOW(obj);
    if (self->pipeline) {
        qrscan_camera_pipeline_stop(self->pipeline);
        g_clear_object(&self->pipeline);
    }
    G_OBJECT_CLASS(qrscan_scanner_window_parent_class)->dispose(obj);
}

static void qrscan_scanner_window_class_init(QRScanScannerWindowClass *klass) {
    G_OBJECT_CLASS(klass)->dispose = qrscan_scanner_window_dispose;
}

static void qrscan_scanner_window_init(QRScanScannerWindow *self) {
    self->flash_on = FALSE;
    self->scanning = TRUE;

    gtk_window_set_title(GTK_WINDOW(self), "QRScan");
    gtk_window_set_default_size(GTK_WINDOW(self), 390, 780);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(self), box);

    self->header_bar = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(self), self->header_bar);

    self->history_btn = gtk_button_new_from_icon_name("document-open-recent-symbolic");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(self->header_bar), self->history_btn);

    self->create_btn = gtk_button_new_from_icon_name("qrscanner-symbolic");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(self->header_bar), self->create_btn);

    self->video_widget = gtk_picture_new();
    gtk_widget_set_vexpand(self->video_widget, TRUE);
    gtk_box_append(GTK_BOX(box), self->video_widget);

    self->zoom_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1.0, 5.0, 0.1);
    gtk_scale_set_draw_value(GTK_SCALE(self->zoom_scale), FALSE);
    gtk_box_append(GTK_BOX(box), self->zoom_scale);

    GtkWidget *action_bar = gtk_action_bar_new();
    gtk_box_append(GTK_BOX(box), action_bar);

    self->flash_btn = gtk_button_new_from_icon_name("weather-clear-night-symbolic");
    gtk_action_bar_pack_start(GTK_ACTION_BAR(action_bar), self->flash_btn);

    self->pick_btn = gtk_button_new_from_icon_name("document-open-symbolic");
    gtk_action_bar_pack_end(GTK_ACTION_BAR(action_bar), self->pick_btn);

    self->flip_btn = gtk_button_new_from_icon_name("object-flip-horizontal-symbolic");
    gtk_action_bar_pack_end(GTK_ACTION_BAR(action_bar), self->flip_btn);

    g_signal_connect(self->flash_btn, "clicked", G_CALLBACK(on_flash_clicked), self);
    g_signal_connect(self->pick_btn,  "clicked", G_CALLBACK(on_pick_clicked),  self);
    g_signal_connect(self->zoom_scale, "value-changed", G_CALLBACK(on_zoom_changed), self);

    self->pipeline = qrscan_camera_pipeline_new();
    g_signal_connect(self->pipeline, "scan-result", G_CALLBACK(on_scan_result), self);
    qrscan_camera_pipeline_start(self->pipeline, self->video_widget);
}

GtkWidget *qrscan_scanner_window_new(GtkApplication *app) {
    return g_object_new(QRSCAN_TYPE_SCANNER_WINDOW,
                        "application", app, NULL);
}
