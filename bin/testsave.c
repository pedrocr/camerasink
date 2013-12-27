#include <gst/gst.h>
#include <glib.h>

#define IFRAMES_PER_FILE 10

typedef struct {
  GstElement *savebin;
  GstElement *pipeline;
  GstElement *queue;
  gchar      *filedir;
  guint       numframes;
} StreamInfo;

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

GstElement *new_save_bin(gchar *filedir) {
  GstElement *bin, *avimux, *filesink;
  GstPad *pad;
  gchar *filename;

  if (filedir) {
    filename = g_strdup_printf("%s/%"G_GINT64_FORMAT".avi",filedir,g_get_real_time());
  } else {
    filename = "/dev/null";
  }  

  bin      = gst_bin_new ("savebin");
  avimux   = gst_element_factory_make ("avimux",  "avimux");
  filesink = gst_element_factory_make ("filesink", "filesink");

  if (!bin || !avimux || !filesink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return NULL;
  }

  gst_bin_add_many (GST_BIN(bin), avimux,filesink, NULL);
  gst_element_link_many (avimux, filesink, NULL);

  /* we set the input filename to the source element */
  g_print("Writing to %s\n", filename);
  g_object_set (G_OBJECT (filesink), "location", filename, NULL);

  /* add ghostpad */
  pad = gst_element_get_request_pad (avimux, "video_0");
  if (!pad) {
    g_printerr ("Couldn't get the pad for avimux\n");
    return NULL;
  }
  gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (GST_OBJECT (pad));

  return bin;
}

void change_file (StreamInfo *si){
  if (si->savebin) {
    gst_element_set_state (si->savebin, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(si->pipeline), si->savebin);
    si->savebin = new_save_bin(si->filedir);
    gst_bin_add (GST_BIN (si->pipeline), si->savebin);
    gst_element_link (si->queue, si->savebin);
    gst_element_set_state (si->savebin, GST_STATE_PLAYING);
  }

  si->numframes = IFRAMES_PER_FILE;
}

static GstPadProbeReturn
cb_have_data (GstPad          *pad,
              GstPadProbeInfo *info,
              gpointer         data)
{
  GstBufferFlags flags;
  StreamInfo *si = (StreamInfo *) data;

  flags = GST_BUFFER_FLAGS(GST_PAD_PROBE_INFO_BUFFER (info));

  if (!(flags & GST_BUFFER_FLAG_DELTA_UNIT)) {
    if (si->numframes == 0) {
      change_file(si);
    }
    (si->numframes)--;
    g_print("Adding Iframe to file!\n");
  }

  return GST_PAD_PROBE_PASS;
}


void usage () {
  g_printerr ("Usage: testsave <filename>\n");
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;

  GstElement *pipeline, *source, *encoder, *queue, *fakesink;
  GstBus *bus;
  GstPad *pad;
  guint bus_watch_id;
  StreamInfo si;

  /* Initialisation */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* Check input arguments */
  if (argc != 2) {
    usage();
    return -1;
  }

  if (!g_file_test(argv[1],G_FILE_TEST_IS_DIR)) {
    g_printerr ("FATAL: \"%s\" is not a directory\n", argv[1]);
    usage();
    return -2;
  }

  /* Create elements */
  pipeline = gst_pipeline_new ("savefile");
  source   = gst_element_factory_make ("videotestsrc", "videotestsrc");
  encoder  = gst_element_factory_make ("x264enc", "x264enc");
  queue  = gst_element_factory_make ("queue", "queue");
  fakesink = new_save_bin(NULL);
  si.numframes = 0;
  si.savebin = fakesink;
  si.pipeline = pipeline;
  si.queue = queue;
  si.filedir = argv[1];

  if (!pipeline || !source || !encoder || !queue || !fakesink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  /* Set up the pipeline */

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* we add all elements into the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, encoder, queue, fakesink, NULL);
  gst_element_link_many (source, encoder, queue, fakesink,NULL);

  /* Add a probe to react to I-frames at the output of the queue blocking it
     and letting frames pile up if needed */
  pad = gst_element_get_static_pad (queue, "src");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER|GST_PAD_PROBE_TYPE_BLOCK,
      (GstPadProbeCallback) cb_have_data, &si, NULL);
  gst_object_unref (pad);

  /* Set the pipeline to "playing" state*/
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Iterate */
  g_print ("Running...\n");
  g_main_loop_run (loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

  return 0;
}
