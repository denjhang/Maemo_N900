#pragma once
#include <gtk/gtk.h>

void qrscan_result_dialog_show(GtkWindow   *parent,
                                const gchar *text,
                                const gchar *format);
