/*
 * Copyright 2015 Andrew Eikum for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef GST_CBS_H
#define GST_CBS_H

#include "wine/list.h"
#include "windef.h"
#include <pthread.h>

typedef enum {
  GST_AUTOPLUG_SELECT_TRY,
  GST_AUTOPLUG_SELECT_EXPOSE,
  GST_AUTOPLUG_SELECT_SKIP
} GstAutoplugSelectResult;

enum CB_TYPE {
    WATCH_BUS,
    EXISTING_NEW_PAD,
    QUERY_FUNCTION,
    ACTIVATE_MODE,
    NO_MORE_PADS,
    REQUEST_BUFFER_SRC,
    EVENT_SRC,
    EVENT_SINK,
    ACCEPT_CAPS_SINK,
    SETCAPS_SINK,
    GOT_DATA_SINK,
    GOT_DATA,
    REMOVED_DECODED_PAD,
    AUTOPLUG_BLACKLIST,
    UNKNOWN_TYPE,
    RELEASE_SAMPLE,
    TRANSFORM_PAD_ADDED,
    QUERY_SINK
};

struct cb_data {
    enum CB_TYPE type;
    union {
        struct watch_bus_data {
            GstBus *bus;
            GstMessage *msg;
            gpointer user;
            GstBusSyncReply ret;
        } watch_bus_data;
        struct existing_new_pad_data {
            GstElement *bin;
            GstPad *pad;
            gpointer user;
        } existing_new_pad_data;
        struct query_function_data {
            GstPad *pad;
            GstObject *parent;
            GstQuery *query;
            gboolean ret;
        } query_function_data;
        struct activate_mode_data {
            GstPad *pad;
            GstObject *parent;
            GstPadMode mode;
            gboolean activate;
            gboolean ret;
        } activate_mode_data;
        struct no_more_pads_data {
            GstElement *decodebin;
            gpointer user;
        } no_more_pads_data;
        struct request_buffer_src_data {
            GstPad *pad;
            GstObject *parent;
            guint64 ofs;
            guint len;
            GstBuffer **buf;
            GstFlowReturn ret;
        } request_buffer_src_data;
        struct event_src_data {
            GstPad *pad;
            GstObject *parent;
            GstEvent *event;
            gboolean ret;
        } event_src_data;
        struct event_sink_data {
            GstPad *pad;
            GstObject *parent;
            GstEvent *event;
            gboolean ret;
        } event_sink_data;
        struct accept_caps_sink_data {
            GstPad *pad;
            GstCaps *caps;
            gboolean ret;
        } accept_caps_sink_data;
        struct setcaps_sink_data {
            GstPad *pad;
            GstCaps *caps;
            gboolean ret;
        } setcaps_sink_data;
        struct got_data_sink_data {
            GstPad *pad;
            GstObject *parent;
            GstBuffer *buf;
            GstFlowReturn ret;
        } got_data_sink_data;
        struct got_data_data {
            GstPad *pad;
            GstObject *parent;
            GstBuffer *buf;
            GstFlowReturn ret;
        } got_data_data;
        struct removed_decoded_pad_data {
            GstElement *bin;
            GstPad *pad;
            gpointer user;
        } removed_decoded_pad_data;
        struct autoplug_blacklist_data {
            GstElement *bin;
            GstPad *pad;
            GstCaps *caps;
            GstElementFactory *fact;
            gpointer user;
            GstAutoplugSelectResult ret;
        } autoplug_blacklist_data;
        struct unknown_type_data {
            GstElement *bin;
            GstPad *pad;
            GstCaps *caps;
            gpointer user;
        } unknown_type_data;
        struct release_sample_data {
            gpointer data;
        } release_sample_data;
        struct transform_pad_added_data {
            GstElement *filter;
            GstPad *pad;
            gpointer user;
        } transform_pad_added_data;
        struct query_sink_data {
            GstPad *pad;
            GstObject *parent;
            GstQuery *query;
            gboolean ret;
        } query_sink_data;
    } u;

    int finished;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct list entry;
};

extern pthread_mutex_t cb_list_lock DECLSPEC_HIDDEN;
extern pthread_cond_t cb_list_cond DECLSPEC_HIDDEN;
extern struct list cb_list DECLSPEC_HIDDEN;
void CALLBACK perform_cb(TP_CALLBACK_INSTANCE *instance, void *user) DECLSPEC_HIDDEN;
BOOL is_wine_thread(void) DECLSPEC_HIDDEN;
void mark_wine_thread(void) DECLSPEC_HIDDEN;

GstBusSyncReply watch_bus_wrapper(GstBus *bus, GstMessage *msg, gpointer user) DECLSPEC_HIDDEN;
void existing_new_pad_wrapper(GstElement *bin, GstPad *pad, gpointer user) DECLSPEC_HIDDEN;
gboolean query_function_wrapper(GstPad *pad, GstObject *parent, GstQuery *query) DECLSPEC_HIDDEN;
gboolean activate_mode_wrapper(GstPad *pad, GstObject *parent, GstPadMode mode, gboolean activate) DECLSPEC_HIDDEN;
void no_more_pads_wrapper(GstElement *decodebin, gpointer user) DECLSPEC_HIDDEN;
GstFlowReturn request_buffer_src_wrapper(GstPad *pad, GstObject *parent, guint64 ofs, guint len, GstBuffer **buf) DECLSPEC_HIDDEN;
gboolean event_src_wrapper(GstPad *pad, GstObject *parent, GstEvent *event) DECLSPEC_HIDDEN;
gboolean event_sink_wrapper(GstPad *pad, GstObject *parent, GstEvent *event) DECLSPEC_HIDDEN;
gboolean accept_caps_sink_wrapper(GstPad *pad, GstCaps *caps) DECLSPEC_HIDDEN;
gboolean setcaps_sink_wrapper(GstPad *pad, GstCaps *caps) DECLSPEC_HIDDEN;
GstFlowReturn got_data_sink_wrapper(GstPad *pad, GstObject *parent, GstBuffer *buf) DECLSPEC_HIDDEN;
GstFlowReturn got_data_wrapper(GstPad *pad, GstObject *parent, GstBuffer *buf) DECLSPEC_HIDDEN;
void removed_decoded_pad_wrapper(GstElement *bin, GstPad *pad, gpointer user) DECLSPEC_HIDDEN;
GstAutoplugSelectResult autoplug_blacklist_wrapper(GstElement *bin, GstPad *pad, GstCaps *caps, GstElementFactory *fact, gpointer user) DECLSPEC_HIDDEN;
void unknown_type_wrapper(GstElement *bin, GstPad *pad, GstCaps *caps, gpointer user) DECLSPEC_HIDDEN;
void release_sample_wrapper(gpointer data) DECLSPEC_HIDDEN;
void Gstreamer_transform_pad_added_wrapper(GstElement *filter, GstPad *pad, gpointer user) DECLSPEC_HIDDEN;
gboolean query_sink_wrapper(GstPad *pad, GstObject *parent, GstQuery *query) DECLSPEC_HIDDEN;

#endif
