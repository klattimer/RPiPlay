/**
 * RPiPlay - An open-source AirPlay mirroring server for Raspberry Pi
 * Copyright (C) 2019 Florian Draschbacher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "video_renderer.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <gst/gst.h>

struct video_renderer_s {
    logger_t *logger;
    GstElement *appsrc, *tee;
    GstElement *pipeline;
    GstElement *video_queue, *video_convert, *video_sink, *h264parse, *h264dec;
    GstElement *app_sink, *app_queue;
    uint64_t pts_last, pts_first;
};

video_renderer_t *video_renderer_init(logger_t *logger, background_mode_t background_mode, bool low_latency) {
    video_renderer_t *renderer;
    renderer = calloc(1, sizeof(video_renderer_t));
    if (!renderer) {
        return NULL;
    }
    gst_init(NULL, NULL);

    renderer->logger = logger;
    renderer->pts_last = 0;
    renderer->appsrc = gst_element_factory_make ("appsrc", "video_source");
    renderer->video_queue = gst_element_factory_make ("queue", "video_queue");
    renderer->h264parse = gst_element_factory_make("h264parse", "video_parse");
    renderer->h264dec = gst_element_factory_make("avdec_h264", "video_decode");
    renderer->video_convert = gst_element_factory_make ("videoconvert", "video_convert");
    renderer->video_sink = gst_element_factory_make ("autovideosink", "video_sink");
    renderer->pipeline = gst_pipeline_new ("airplay-pipeline");
    renderer->app_sink = gst_element_factory_make ("appsink", "app_sink");
    renderer->app_queue = gst_element_factory_make ("queue", "app_queue");
    renderer->tee = gst_element_factory_make ("tee", "tee");

    gst_bin_add_many (GST_BIN (renderer->pipeline),
                      renderer->appsrc,
                      renderer->tee,
                      renderer->video_queue,
                      renderer->h264parse,
                      renderer->h264dec,
                      renderer->video_convert,
                      renderer->video_sink,
                      renderer->app_sink,
                      renderer->app_queue,
                      NULL);

    if (//gst_element_link_many (renderer->appsrc, renderer->tee, NULL) != TRUE ||
        gst_element_link_many (renderer->appsrc, renderer->video_queue, renderer->h264parse, renderer->h264dec, renderer->video_convert, renderer->video_sink, NULL) != TRUE ||
        gst_element_link_many (renderer->app_queue, renderer->app_sink, NULL) != TRUE
        ) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (renderer->pipeline);
        return -1;
    }

    return renderer;
}

void video_renderer_start(video_renderer_t *renderer) {
    gst_element_set_state (renderer->pipeline, GST_STATE_PLAYING);
}

void video_renderer_render_buffer(video_renderer_t *renderer, raop_ntp_t *ntp, unsigned char* data, int data_len, uint64_t pts, int type) {
    // TODO: Prepare the GstBuffer from buffer, ntp and pts
    GstBuffer *buffer;
    logger_log(renderer->logger, LOGGER_INFO, "Received buffer of size %d", data_len);
    if (data_len == 0) {
        logger_log(renderer->logger, LOGGER_INFO, "Empty data");
        return;
    }

    buffer = gst_buffer_new_and_alloc(data_len);
    // memcpy(GST_BUFFER_DATA(buffer),GST_BUFFER_DATA(data),data_len);

    if (buffer == NULL) {
        logger_log(renderer->logger, LOGGER_INFO, "Cannot allocate GstBuffer");
        return;
    }

    gst_buffer_fill(buffer, 0, data, data_len);
    if (renderer->pts_last == 0) {
        renderer->pts_last = pts;
        renderer->pts_first = pts;
    }
    uint64_t duration = pts - renderer->pts_last;
    uint64_t timestamp = pts - renderer->pts_first;

    /* Set its timestamp and duration */
    // GST_BUFFER_TIMESTAMP (buffer) = timestamp;
    // GST_BUFFER_DURATION (buffer) = duration;

    logger_log(renderer->logger, LOGGER_INFO, "Setting timestamp and duration %ul, %ul", timestamp, duration);

    renderer->pts_last = pts;
    gst_app_src_push_buffer (renderer->appsrc, buffer);
    gst_element_set_state (renderer->pipeline, GST_STATE_PLAYING);
}

void video_renderer_flush(video_renderer_t *renderer) {
}

void video_renderer_destroy(video_renderer_t *renderer) {
    gst_app_src_end_of_stream (renderer->appsrc);
    gst_element_set_state (renderer->pipeline, GST_STATE_NULL);
    gst_object_unref (renderer->pipeline);
    if (renderer) {
        free(renderer);
    }
}

void video_renderer_update_background(video_renderer_t *renderer, int type) {

}
