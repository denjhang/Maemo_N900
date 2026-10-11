#include "result_dialog.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>

typedef struct {
    gchar       *text;
    gchar       *format;
    GtkWindow   *parent;
    GtkWidget   *dialog;
} ResultCtx;

static void ctx_free(ResultCtx *ctx) {
    g_free(ctx->text);
    g_free(ctx->format);
    g_free(ctx);
}

static void on_copy_clicked(GtkButton *btn, gpointer user_data) {
    ResultCtx     *ctx      = user_data;
    GdkDisplay    *display  = gdk_display_get_default();
    GdkClipboard  *clipboard = gdk_display_get_clipboard(display);
    gdk_clipboard_set_text(clipboard, ctx->text);
}

static void on_open_clicked(GtkButton *btn, gpointer user_data) {
    ResultCtx *ctx = user_data;
    GError    *err = NULL;
    gtk_show_uri(ctx->parent, ctx->text, GDK_CURRENT_TIME);
}

static void on_dialog_response(GtkDialog *dialog, int response, gpointer user_data) {
    ResultCtx *ctx = user_data;
    gtk_window_destroy(GTK_WINDOW(dialog));
    ctx_free(ctx);
}

void qrscan_result_dialog_show(GtkWindow   *parent,
                                const gchar *text,
                                const gchar *format) {
    ResultCtx *ctx  = g_new0(ResultCtx, 1);
    ctx->text       = g_strdup(text);
    ctx->format     = g_strdup(format);
    ctx->parent     = parent;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        format, parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE,
        NULL
    );
    ctx->dialog = dialog;
    gtk_window_set_default_size(GTK_WINDOW(dialog), 360, 300);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_top(content,    16);
    gtk_widget_set_margin_bottom(content, 16);
    gtk_widget_set_margin_start(content,  16);
    gtk_widget_set_margin_end(content,    16);
    gtk_box_set_spacing(GTK_BOX(content), 12);

    GtkWidget *label_fmt = gtk_label_new(format);
    gtk_widget_add_css_class(label_fmt, "caption");
    gtk_widget_set_halign(label_fmt, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), label_fmt);

    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view)),
        text, -1);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 100);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), text_view);
    gtk_box_append(GTK_BOX(content), scroll);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(content), btn_row);

    GtkWidget *copy_btn = gtk_button_new_with_label("Copy");
    g_signal_connect(copy_btn, "clicked", G_CALLBACK(on_copy_clicked), ctx);
    gtk_box_append(GTK_BOX(btn_row), copy_btn);

    gboolean is_url = g_str_has_prefix(text, "http://") ||
                      g_str_has_prefix(text, "https://");
    if (is_url) {
        GtkWidget *open_btn = gtk_button_new_with_label("Open URL");
        g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_clicked), ctx);
        gtk_box_append(GTK_BOX(btn_row), open_btn);
    }

    g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), ctx);
    gtk_widget_show(dialog);
}
