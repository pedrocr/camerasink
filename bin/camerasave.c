#include <common.h>
#include <libsoup/soup.h>
#include <gst/app/gstappsink.h>

#define MIN_FRAMES_PER_FILE 200
#define HTTP_LISTEN_ADDRESS "127.0.0.1"
#define HTTP_LISTEN_PORT 4000
#define HTTP_SERVER_NAME "camerasave"
#define HTTP_MULTIPART_BOUNDARY "SurelyJPEGDoesntIncludeThis"

typedef struct {
  GstElement *pipeline;
  GMainLoop  *loop;
  GstBus     *bus;

  gulong      bus_watch_id;

  GstElement *source;
  GstElement *tee;
  GstElement *queue;
  GstElement *savebin;
  GstElement *queue2;
  GstElement *jpegbin;

  gulong      probeid;

  GstPad     *queuepad;

  gchar      *filedir;
  guint       numframes;
  gboolean    ignoreEOS;

  GstClockTime bufferoffset;
  GHashTable *httpclients;
} StreamInfo;

void change_file (StreamInfo *si);
void reset_probe (StreamInfo *si);
static GstPadProbeReturn probe_data (GstPad *pad, GstPadProbeInfo *info, gpointer data);
static GstFlowReturn new_jpeg (GstAppSink *sink, gpointer data);

void padadd (GstElement *bin, GstPad *newpad, gpointer data) {
  GstElement *sink = (GstElement *) data;
  GstPad *sink_pad;

  sink_pad = my_gst_element_get_static_pad (sink, "sink");
  gst_pad_link(newpad,sink_pad);

  gst_object_unref (GST_OBJECT (sink_pad));
}

gboolean uriplug (GstElement *bin, GstPad *pad, GstCaps *caps, gpointer ud) {
  return !(
    gst_caps_is_subset(caps, gst_caps_from_string ("video/x-h264, parsed=true")) ||
    gst_caps_is_subset(caps, gst_caps_from_string ("image/jpeg"))
  );
}

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  StreamInfo *si = (StreamInfo *) data;
  GstMessage *origmsg;
  GstElement *filesink = gst_bin_get_by_name(GST_BIN(si->savebin), "filesink");
  exit_if_true(!filesink, "Couldn't get the filesink from the bin!\n");

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ELEMENT:
      gst_structure_get(gst_message_get_structure(msg), "message", 
                        GST_TYPE_MESSAGE, &origmsg, NULL);
      if (GST_MESSAGE_EOS == GST_MESSAGE_TYPE(origmsg) && 
          filesink == GST_ELEMENT(GST_MESSAGE_SRC (origmsg))) {
        if (si->ignoreEOS) {
          si->ignoreEOS = FALSE;
          g_print("Changing output file\n");
          change_file(si);
          g_print("Unblocking the pad\n");
          reset_probe(si);
        } else {
          g_print ("Caught end of stream in the pipeline, strange, exiting!\n");
          g_main_loop_quit (si->loop);
        }
      }
      break;
    case GST_MESSAGE_EOS:
      g_print ("Caught end of stream in the pipeline, strange, exiting!\n");
      g_main_loop_quit (si->loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (si->loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

GstElement *new_jpeg_bin(StreamInfo *si) {
  GstElement *bin, *decodebin, *jpegenc, *multipartmux, *sink;
  GstPad *pad;

  bin = my_gst_bin_new ("jpegbin");
  decodebin = my_gst_element_factory_make ("decodebin",  "decodebin");
  jpegenc = my_gst_element_factory_make ("jpegenc", "jpegenc");
  multipartmux = my_gst_element_factory_make ("multipartmux", "multipartmux");
  sink = my_gst_element_factory_make ("appsink", "appsink");

  g_object_set (G_OBJECT (multipartmux), "boundary", HTTP_MULTIPART_BOUNDARY, NULL);

  g_signal_connect (decodebin, "pad-added", G_CALLBACK(padadd), jpegenc);
  g_signal_connect (sink, "new-sample", G_CALLBACK(new_jpeg), si);
  g_object_set (G_OBJECT (sink), "emit-signals", TRUE, NULL);

  gst_bin_add_many (GST_BIN(bin), decodebin, jpegenc, multipartmux, sink, NULL);
  gst_element_link_many (jpegenc, multipartmux, sink, NULL);

  /* add video ghostpad */
  pad = my_gst_element_get_static_pad (decodebin, "sink");
  gst_element_add_pad (bin, gst_ghost_pad_new ("video_sink", pad));
  gst_object_unref (GST_OBJECT (pad));

  return bin;
}

GstElement *new_save_bin(gchar *filedir) {
  GstElement *bin, *mux, *filesink;
  GstPad *pad;
  gchar *filename;

  if (filedir) {
    filename = g_strdup_printf("%s/%"G_GINT64_FORMAT".mkv",filedir,g_get_real_time());
  } else {
    filename = "/dev/null";
  }  

  bin = my_gst_bin_new ("savebin");
  g_object_set (G_OBJECT (bin), "message-forward", TRUE, NULL);
  mux = my_gst_element_factory_make ("matroskamux",  "matroskamux");
  filesink = my_gst_element_factory_make ("filesink", "filesink");

  gst_bin_add_many (GST_BIN(bin), mux,filesink, NULL);
  gst_element_link_many (mux, filesink, NULL);

  /* we set the input filename to the source element */
  g_print("Writing to %s\n", filename);
  g_object_set (G_OBJECT (filesink), "location", filename, NULL);

  /* add video ghostpad */
  pad = my_gst_element_get_request_pad (mux, "video_%u");

  gst_element_add_pad (bin, gst_ghost_pad_new ("video_sink", pad));
  gst_object_unref (GST_OBJECT (pad));

  return bin;
}

void change_file (StreamInfo *si){
  gst_element_set_state (si->savebin, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(si->pipeline), si->savebin);
      si->savebin = new_save_bin(si->filedir);
      gst_bin_add (GST_BIN (si->pipeline), si->savebin);
    gst_element_link (si->queue, si->savebin);
  gst_element_set_state (si->savebin, GST_STATE_PLAYING);
}

void reset_probe (StreamInfo *si) {
  gulong probeid;
  GstPad *pad;

  /* Add a probe to react to I-frames at the output of the queue blocking it
   and letting frames pile up if needed */
  pad = my_gst_element_get_static_pad (si->queue, "src");
  probeid = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER|
                                    GST_PAD_PROBE_TYPE_BLOCK,
                               (GstPadProbeCallback) probe_data, si, NULL);
  exit_if_true(!probeid, "Couldn't set the probe on queue src");

  if (si->probeid) {
    gst_pad_remove_probe (pad, si->probeid);
  }

  si->probeid = probeid;
  gst_object_unref (pad);
}

static void send_chunk (gpointer key, gpointer value, gpointer user_data) {
  GstBuffer *buffer = (GstBuffer *) user_data;
  SoupMessage *msg = SOUP_MESSAGE(key);
  SoupServer *server = SOUP_SERVER(value);
  GstMapInfo map;

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  soup_message_body_append (msg->response_body, SOUP_MEMORY_COPY, map.data, map.size);
  gst_buffer_unmap (buffer, &map);

  soup_server_unpause_message (server, msg);
}

typedef struct {
  StreamInfo *si;
  GstBuffer *buffer;
} SendBufferInfo;

gboolean send_buffer (SendBufferInfo *sbi) {
  /* Send the chunk to all active clients and then we're done with SendBufferInfo */
  g_hash_table_foreach(sbi->si->httpclients, (GHFunc) send_chunk, sbi->buffer);
  g_free(sbi);

  return FALSE;
}

static GstFlowReturn new_jpeg (GstAppSink *sink, gpointer data) {
  GstBuffer *buffer;
  SendBufferInfo *sbi;
  StreamInfo *si = (StreamInfo *) data;

  buffer = gst_sample_get_buffer(gst_app_sink_pull_sample(sink));

  /* Pack the StreamInfo and Buffer pointers into a single pointer o pass as data */
  sbi = g_new0(SendBufferInfo, 1);
  sbi->si = si;
  sbi->buffer = buffer;
  /* Run the actual buffer sending in the main context as libsoup is not thread safe */
  g_idle_add((GSourceFunc) send_buffer, sbi);

  return GST_FLOW_OK;
}

static GstPadProbeReturn probe_data (GstPad *pad, GstPadProbeInfo *info, gpointer data) {
  GstBufferFlags flags;
  GstPad *savebinpad;
  GstBuffer *buffer;
  StreamInfo *si = (StreamInfo *) data;

  (si->numframes)++;

  buffer = GST_PAD_PROBE_INFO_BUFFER(info);
  buffer = gst_buffer_make_writable(buffer);
  flags = GST_BUFFER_FLAGS(buffer);
  GST_PAD_PROBE_INFO_DATA(info) = buffer;

  if (!(flags & GST_BUFFER_FLAG_DELTA_UNIT)) {
    g_print("Buffer is I-frame!\n");
    if (si->numframes >= MIN_FRAMES_PER_FILE) {
      si->numframes = 0;
      savebinpad = my_gst_element_get_static_pad (si->savebin, "video_sink");
      g_print("Posting EOS\n");
      si->ignoreEOS = TRUE;
      gst_pad_send_event(savebinpad, gst_event_new_eos());
      gst_object_unref (GST_OBJECT (savebinpad));
     
      /* We don't need to apply the offset in this branch because the probe will 
         be running again after the swap and using the other branch */
      si->bufferoffset = GST_BUFFER_PTS(buffer);
      return GST_PAD_PROBE_OK;
    }
  }
  
  GST_BUFFER_PTS(buffer) -= si->bufferoffset;
  //g_print("Processing buffer #%d of file\n", si->numframes);
  return GST_PAD_PROBE_PASS;
}

void end_connection (SoupMessage *msg, gpointer user_data) {
  StreamInfo *si = (StreamInfo *) user_data;

  g_print("Got end of connection\n");

  g_hash_table_remove(si->httpclients, msg);
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

  soup_server_pause_message(server, msg);

  g_hash_table_replace(si->httpclients, msg, server);
  g_signal_connect (msg, "finished", G_CALLBACK(end_connection), si);
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

  /* Check input arguments */
  if (argc != 3) {
    usage();
    return -1;
  }

  if (!g_file_test(argv[2],G_FILE_TEST_IS_DIR)) {
    g_printerr ("FATAL: \"%s\" is not a directory\n", argv[2]);
    usage();
    return -2;
  }


  /* Create elements */
  si.pipeline = my_gst_pipeline_new ("savefile");
  si.source   = my_gst_element_factory_make ("uridecodebin", "uridecodebin");
  si.tee = my_gst_element_factory_make ("tee", "tee");
  si.queue  = my_gst_element_factory_make ("queue", "queue");
  si.queuepad = my_gst_element_get_static_pad (si.queue, "src");
  si.queue2  = my_gst_element_factory_make ("queue", "queue2");
  si.jpegbin = new_jpeg_bin(&si);

  /* create the bus */
  si.bus = gst_pipeline_get_bus (GST_PIPELINE (si.pipeline));
  si.bus_watch_id = gst_bus_add_watch (si.bus, bus_call, &si);
  gst_object_unref (si.bus);

  si.savebin = new_save_bin(NULL);
  exit_if_true(!si.savebin, "Couldn't create savebin");
  si.numframes = MIN_FRAMES_PER_FILE;
  si.filedir = argv[2];
  si.ignoreEOS = FALSE;
  si.probeid = 0;
  si.bufferoffset = 0;

  /* Set up the pipeline */

  /* we set the input filename to the source element */
  g_print("Reading from %s\n", argv[1]);
  g_object_set (G_OBJECT (si.source), "uri", argv[1], NULL);
  g_signal_connect (si.source, "autoplug-continue", G_CALLBACK(uriplug), NULL);
  g_signal_connect (si.source, "pad-added", G_CALLBACK(padadd), si.tee);

  /* we add all elements into the pipeline */
  gst_bin_add_many (GST_BIN (si.pipeline), si.source, si.tee, si.queue, 
                                           si.savebin, NULL); 
  g_object_set (G_OBJECT (si.pipeline), "message-forward", TRUE, NULL); 
  gst_element_link_many (si.tee, si.queue, si.savebin, NULL);

  gst_bin_add_many (GST_BIN (si.pipeline), si.queue2, si.jpegbin, NULL);
  gst_element_link_many (si.tee, si.queue2, si.jpegbin, NULL);

  /* Setup the data probe on queue element */
  /* Add a probe to react to I-frames at the output of the queue blocking it
 and letting frames pile up if needed */
  reset_probe(&si);

  si.httpclients = g_hash_table_new(NULL, NULL);
  httpaddress = soup_address_new(HTTP_LISTEN_ADDRESS, HTTP_LISTEN_PORT);
  exit_if_true(!httpaddress, "Couldn't create libsoup httpaddress\n");
  if (SOUP_STATUS_OK != soup_address_resolve_sync(httpaddress, NULL)) {
    g_printerr("FATAL: Can't resolve %s:%d\n", HTTP_LISTEN_ADDRESS, HTTP_LISTEN_PORT);
  }
  httpserver = soup_server_new("server-header", HTTP_SERVER_NAME,
                               "interface", httpaddress,
                               NULL);
  exit_if_true(!httpserver, "Couldn't create libsoup httpserver\n");
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
  g_source_remove (si.bus_watch_id);
  g_main_loop_unref (si.loop);

  return 0;
}
