#include <gst/gst.h>
#include <glib.h>


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


void usage () {
  g_printerr ("Usage: testread <file1> ... <file n>\n");
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;

  GstElement *pipeline, *source, *queue, *sink;
  GstBus *bus;
  GstPad *pad;
  guint bus_watch_id;
  StreamInfo si;

  /* Initialisation */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* Check input arguments */
  if (argc < 2) {
    usage();
    return -1;
  }

  /* Create elements */
  pipeline = gst_pipeline_new ("readfiles");
  source   = gst_element_factory_make ("uridecodebin", "uridecodebin");
  queue  = gst_element_factory_make ("queue", "queue");
  sink = gst_element_factory_make ("filesink", "filesink");

  if (!pipeline || !source || !queue || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  /* Set up the pipeline */

  /* we set the input filename to the source element */
  g_print("Reading from %s\n", argv[1]);
  g_object_set (G_OBJECT (source), "uri", argv[1], NULL);

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* we add all elements into the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, queue, sink, NULL);
  gst_element_link_many (queue, fakesink,NULL);

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
