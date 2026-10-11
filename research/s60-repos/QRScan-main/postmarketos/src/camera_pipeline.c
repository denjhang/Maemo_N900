#include "camera_pipeline.h"
#include "decoder.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

enum {
    SIGNAL_SCAN_RESULT,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _QRScanCameraPipeline {
    GObject      parent_instance;

    GstElement  *pipeline;
    GstElement  *src;
    GstElement  *tee;
    GstElement  *display_queue;
    GstElement  *display_conv;
    GstElement  *gtksink;
    GstElement  *decode_queue;
    GstElement  *decode_conv;
    GstElement  *appsink;

    GThread     *decode_thread;
    gboolean     running;
};

G_DEFINE_TYPE(QRScanCameraPipeline, qrscan_camera_pipeline, G_TYPE_OBJECT)

static gpointer decode_thread_func(gpointer data) {
    QRScanCameraPipeline *self = QRSCAN_CAMERA_PIPELINE(data);

    while (self->running) {
        GstSample *sample = gst_app_sink_try_pull_sample(
            GST_APP_SINK(self->appsink), GST_SECOND / 4);

        if (!sample) continue;

        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstCaps   *caps   = gst_sample_get_caps(sample);

        GstVideoInfo vinfo;
        if (!gst_video_info_from_caps(&vinfo, caps)) {
            gst_sample_unref(sample);
            continue;
        }

        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            gchar *result = qrscan_decoder_decode_rgb(
                map.data,
                GST_VIDEO_INFO_WIDTH(&vinfo),
                GST_VIDEO_INFO_HEIGHT(&vinfo),
                GST_VIDEO_INFO_COMP_STRIDE(&vinfo, 0)
            );

            gst_buffer_unmap(buffer, &map);

            if (result) {
                g_signal_emit(self, signals[SIGNAL_SCAN_RESULT], 0, result, "QR_CODE");
                g_free(result);
            }
        }

        gst_sample_unref(sample);
    }

    return NULL;
}

void qrscan_camera_pipeline_start(QRScanCameraPipeline *self, GtkWidget *sink_widget) {
    self->pipeline    = gst_pipeline_new("qrscan");
    self->src         = gst_element_factory_make("v4l2src",         "src");
    self->tee         = gst_element_factory_make("tee",             "tee");
    self->display_queue = gst_element_factory_make("queue",         "disp_queue");
    self->display_conv  = gst_element_factory_make("videoconvert",  "disp_conv");
    self->gtksink       = gst_element_factory_make("gtk4paintablesink", "gtksink");
    self->decode_queue  = gst_element_factory_make("queue",         "dec_queue");
    self->decode_conv   = gst_element_factory_make("videoconvert",  "dec_conv");
    self->appsink       = gst_element_factory_make("appsink",       "appsink");

    if (!self->gtksink) {
        self->gtksink = gst_element_factory_make("gtksink", "gtksink");
    }

    g_object_set(self->appsink,
        "emit-signals", FALSE,
        "sync",         FALSE,
        "drop",         TRUE,
        "max-buffers",  1,
        NULL);

    GstCaps *decode_caps = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "RGB",
        NULL);
    gst_app_sink_set_caps(GST_APP_SINK(self->appsink), decode_caps);
    gst_caps_unref(decode_caps);

    gst_bin_add_many(GST_BIN(self->pipeline),
        self->src, self->tee,
        self->display_queue, self->display_conv, self->gtksink,
        self->decode_queue, self->decode_conv, self->appsink,
        NULL);

    gst_element_link(self->src, self->tee);
    gst_element_link_many(self->display_queue, self->display_conv, self->gtksink, NULL);
    gst_element_link_many(self->decode_queue, self->decode_conv, self->appsink, NULL);

    GstPad *tee_src1 = gst_element_request_pad_simple(self->tee, "src_%u");
    GstPad *q1_sink  = gst_element_get_static_pad(self->display_queue, "sink");
    gst_pad_link(tee_src1, q1_sink);
    gst_object_unref(tee_src1);
    gst_object_unref(q1_sink);

    GstPad *tee_src2 = gst_element_request_pad_simple(self->tee, "src_%u");
    GstPad *q2_sink  = gst_element_get_static_pad(self->decode_queue, "sink");
    gst_pad_link(tee_src2, q2_sink);
    gst_object_unref(tee_src2);
    gst_object_unref(q2_sink);

    if (GTK_IS_WIDGET(sink_widget)) {
        GstPaintable *paintable = NULL;
        g_object_get(self->gtksink, "paintable", &paintable, NULL);
        if (paintable) {
            gtk_picture_set_paintable(GTK_PICTURE(sink_widget),
                                      GDK_PAINTABLE(paintable));
            g_object_unref(paintable);
        }
    }

    self->running = TRUE;
    self->decode_thread = g_thread_new("qrscan-decode", decode_thread_func, self);
    gst_element_set_state(self->pipeline, GST_STATE_PLAYING);
}

void qrscan_camera_pipeline_stop(QRScanCameraPipeline *self) {
    self->running = FALSE;
    if (self->pipeline) {
        gst_element_set_state(self->pipeline, GST_STATE_NULL);
        gst_object_unref(self->pipeline);
        self->pipeline = NULL;
    }
    if (self->decode_thread) {
        g_thread_join(self->decode_thread);
        self->decode_thread = NULL;
    }
}

void qrscan_camera_pipeline_set_torch(QRScanCameraPipeline *self, gboolean on) {
    if (!self->src) return;
    g_object_set(self->src, "extra-controls",
        gst_structure_new("extra-controls",
            "torch", G_TYPE_BOOLEAN, on, NULL),
        NULL);
}

void qrscan_camera_pipeline_set_zoom(QRScanCameraPipeline *self, gdouble zoom) {
    if (!self->src) return;
    g_object_set(self->src, "zoom", zoom, NULL);
}

static void qrscan_camera_pipeline_finalize(GObject *obj) {
    qrscan_camera_pipeline_stop(QRSCAN_CAMERA_PIPELINE(obj));
    G_OBJECT_CLASS(qrscan_camera_pipeline_parent_class)->finalize(obj);
}

static void qrscan_camera_pipeline_class_init(QRScanCameraPipelineClass *klass) {
    G_OBJECT_CLASS(klass)->finalize = qrscan_camera_pipeline_finalize;

    signals[SIGNAL_SCAN_RESULT] = g_signal_new(
        "scan-result",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, NULL, NULL,
        NULL,
        G_TYPE_NONE, 2,
        G_TYPE_STRING,
        G_TYPE_STRING
    );
}

static void qrscan_camera_pipeline_init(QRScanCameraPipeline *self) {
    self->running = FALSE;
}

QRScanCameraPipeline *qrscan_camera_pipeline_new(void) {
    return g_object_new(QRSCAN_TYPE_CAMERA_PIPELINE, NULL);
}
