#pragma once
#include <glib-object.h>
#include <gtk/gtk.h>

#define QRSCAN_TYPE_CAMERA_PIPELINE (qrscan_camera_pipeline_get_type())
G_DECLARE_FINAL_TYPE(QRScanCameraPipeline, qrscan_camera_pipeline,
                     QRSCAN, CAMERA_PIPELINE, GObject)

QRScanCameraPipeline *qrscan_camera_pipeline_new      (void);
void                  qrscan_camera_pipeline_start     (QRScanCameraPipeline *self, GtkWidget *sink_widget);
void                  qrscan_camera_pipeline_stop      (QRScanCameraPipeline *self);
void                  qrscan_camera_pipeline_set_torch (QRScanCameraPipeline *self, gboolean on);
void                  qrscan_camera_pipeline_set_zoom  (QRScanCameraPipeline *self, gdouble zoom);
