#include <glib.h>
#include <gst/gst.h>
#include <libsoup/soup.h>
#include <gst/app/gstappsink.h>

#define HTTP_LISTEN_ADDRESS "127.0.0.1"
#define HTTP_LISTEN_PORT 4000
#define HTTP_SERVER_NAME "camerasave"
#define HTTP_MULTIPART_BOUNDARY "SurelyJPEGDoesntIncludeThis"

typedef struct {
  GstElement *pipeline;
  GMainLoop  *loop;
  GstBus     *bus;
  gulong      bus_watch_id;

  GstElement *videotestsrc;
  GstElement *jpegenc;
  GstElement *multipartmux;
  GstElement *appsink;

  GHashTable *httpclients;
} StreamInfo;

static GstFlowReturn new_jpeg (GstAppSink *sink, gpointer data);

static void send_chunk (gpointer key, gpointer value, gpointer user_data) {
  GstBuffer *buffer = (GstBuffer *) user_data;
  SoupMessage *msg = SOUP_MESSAGE(key);
  SoupServer *server = SOUP_SERVER(value);
  GstMapInfo map;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  g_print ("writing chunk of %lu bytes\n", (unsigned long)map.size);
  soup_message_body_append (msg->response_body, SOUP_MEMORY_COPY, map.data, map.size);
  gst_buffer_unmap (buffer, &map);

  soup_server_unpause_message (server, msg);
}

static GstFlowReturn new_jpeg (GstAppSink *sink, gpointer data) {
  GstBuffer *buffer;
  StreamInfo *si = (StreamInfo *) data;

  buffer = gst_sample_get_buffer(gst_app_sink_pull_sample(sink));
  g_hash_table_foreach(si->httpclients, (GHFunc) send_chunk, buffer);

  return GST_FLOW_OK;
}

void end_connection (SoupMessage *msg, gpointer user_data) {
  StreamInfo *si = (StreamInfo *) user_data;

  g_print("Got end of connection\n");

  g_hash_table_remove(si->httpclients, msg);
}

void wrote_chunk (SoupMessage *msg, gpointer user_data) {
  g_print("Wrote chunk!\n");
}

void wrote_body_data (SoupMessage *msg, SoupBuffer *chunk, gpointer user_data) {
  g_print("Wrote data from chunk [%p]!\n", chunk);
}

static void
new_connection (SoupServer        *server,
                SoupMessage       *msg, 
                const char        *path,
                GHashTable        *query,
                SoupClientContext *client,
                gpointer           user_data)
{
  StreamInfo *si = (StreamInfo *) user_data;

  g_print ("Got request: %s %s HTTP/1.%d\n", msg->method, path,
                                             soup_message_get_http_version (msg));
  if (msg->method != SOUP_METHOD_GET) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
    return;
  }

  soup_message_headers_set_encoding (msg->response_headers, SOUP_ENCODING_CHUNKED);
  soup_message_headers_append(msg->response_headers, "Content-Type", "multipart/x-mixed-replace;boundary=" HTTP_MULTIPART_BOUNDARY);
  soup_message_set_status (msg, SOUP_STATUS_OK);
  soup_message_body_set_accumulate (msg->response_body, FALSE);

  //g_print("Pausing message [%p] in server [%p]\n", msg, server);
  soup_server_pause_message(server, msg);

  g_hash_table_replace(si->httpclients, msg, server);
  g_signal_connect (msg, "finished", G_CALLBACK(end_connection), si);
  g_signal_connect (msg, "wrote-chunk", G_CALLBACK(wrote_chunk), si);
  g_signal_connect (msg, "wrote-body-data", G_CALLBACK(wrote_body_data), si);
}

void usage () {
  g_printerr ("Usage: camerasave <uri> <filedir>\n");
}

int
main (int   argc,
      char *argv[])
{
  StreamInfo si;
  SoupServer *httpserver;
  SoupAddress *httpaddress;

  /* Initialisation */
  gst_init (&argc, &argv);
  si.loop = g_main_loop_new (NULL, FALSE);

  /* Create elements */
  si.pipeline = gst_pipeline_new ("savefile");
  si.videotestsrc   = gst_element_factory_make ("videotestsrc", "videotestsrc");
  si.jpegenc = gst_element_factory_make ("jpegenc", "jpegenc");
  si.multipartmux  = gst_element_factory_make ("multipartmux", "multipartmux");
  si.appsink  = gst_element_factory_make ("appsink", "appsink");

  g_object_set (G_OBJECT (si.multipartmux), "boundary", HTTP_MULTIPART_BOUNDARY, NULL);
  g_signal_connect (si.appsink, "new-sample", G_CALLBACK(new_jpeg), &si);
  g_object_set (G_OBJECT (si.appsink), "emit-signals", TRUE, NULL);

  /* we add all elements into the pipeline */
  gst_bin_add_many (GST_BIN (si.pipeline), si.videotestsrc, si.jpegenc, 
                                           si.multipartmux, si.appsink, NULL);

  gst_element_link_many (si.videotestsrc, si.jpegenc, si.multipartmux, si.appsink, NULL);

  /* Setup the data probe on queue element */
  /* Add a probe to react to I-frames at the output of the queue blocking it
 and letting frames pile up if needed */

  si.httpclients = g_hash_table_new(NULL, NULL);
  httpaddress = soup_address_new(HTTP_LISTEN_ADDRESS, HTTP_LISTEN_PORT);
  if (SOUP_STATUS_OK != soup_address_resolve_sync(httpaddress, NULL)) {
    g_printerr("FATAL: Can't resolve %s:%d\n", HTTP_LISTEN_ADDRESS, HTTP_LISTEN_PORT);
  }
  httpserver = soup_server_new("server-header", HTTP_SERVER_NAME,
                               "interface", httpaddress,
                               NULL);
  soup_server_add_handler (httpserver, "/mjpeg", new_connection, &si, NULL);
  soup_server_run_async (httpserver);
  g_print("Listening on http://%s:%d/\n", HTTP_LISTEN_ADDRESS, HTTP_LISTEN_PORT);

  /* Set the pipeline to "playing" state*/
  gst_element_set_state (si.pipeline, GST_STATE_PLAYING);

  /* Iterate */
  g_print ("Running...\n");
  g_main_loop_run (si.loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (si.pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (si.pipeline));
  g_main_loop_unref (si.loop);

  return 0;
}
