#include <gst/gst.h>
#include <glib.h>

#define IFRAMES_PER_FILE 10

typedef struct {
  GstElement *savebin;
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

static GstPadProbeReturn
cb_have_data (GstPad          *pad,
              GstPadProbeInfo *info,
              gpointer         data)
{
  GstBufferFlags flags;
  guint *numframes = (guint *) data;

  flags = GST_BUFFER_FLAGS(GST_PAD_PROBE_INFO_BUFFER (info));

  if (!(flags & GST_BUFFER_FLAG_DELTA_UNIT)) {
    if (*numframes == 0) {
      g_print("Changing file!\n");
      *numframes = IFRAMES_PER_FILE;
    }
    (*numframes)--;
    g_print("Adding Iframe to file!\n");
  }

  return GST_PAD_PROBE_PASS;
}

GstElement *new_save_bin(char *filename) {
  GstElement *bin, *avimux, *filesink;
  GstPad *pad;
  
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


int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;

  GstElement *pipeline, *source, *encoder, *queue, *savebin;
  GstBus *bus;
  GstPad *pad;
  guint bus_watch_id;
  guint numframes = 0;

  /* Initialisation */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* Check input arguments */
  if (argc != 2) {
    g_printerr ("Usage: %s <filename>\n", argv[0]);
    return -1;
  }

  /* Create elements */
  pipeline = gst_pipeline_new ("savefile");
  source   = gst_element_factory_make ("videotestsrc",  "videotestsrc");
  encoder  = gst_element_factory_make ("x264enc",      "x264enc");
  queue  = gst_element_factory_make ("queue",     "queue");
  savebin = new_save_bin(argv[1]);

  if (!pipeline || !source || !encoder || !queue || !savebin) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  /* Set up the pipeline */

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* we add all elements into the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, encoder, queue, savebin, NULL);
  gst_element_link_many (source, encoder, queue, savebin,NULL);

  /* Add a probe to react to I-frames at the output of the queue blocking it
     and letting frames pile up if needed */
  pad = gst_element_get_static_pad (queue, "src");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER|GST_PAD_PROBE_TYPE_BLOCK,
      (GstPadProbeCallback) cb_have_data, &numframes, NULL);
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
