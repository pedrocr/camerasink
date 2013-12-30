#include <gst/gst.h>
#include <glib.h>

/* String to use when saving */
#define MATROSKA_APPNAME "camerasave"
/* Nanoseconds between indexes */
#define MATROSKA_MIN_INDEX_INTERVAL 1000000000

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
  g_printerr ("Usage: testread <output file> <input file1> ... <input file n>\n");
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;
  GstElement *pipeline, *source, *demux, *queue, *mux, *sink;
  GstBus *bus;
  guint bus_watch_id;

  /* Initialisation */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* Check input arguments */
  if (argc < 2) {
    usage();
    return -1;
  }

  if (!g_file_test(argv[2],G_FILE_TEST_EXISTS)) {
    g_printerr ("FATAL: \"%s\" doesn't exist\n", argv[2]);
    usage();
    return -2;
  }

  /* Create elements */
  pipeline = gst_pipeline_new ("readfiles");
  source   = gst_element_factory_make ("filesrc", "filesrc");
  demux  = gst_element_factory_make ("matroskademux", "matroskademux");
  queue  = gst_element_factory_make ("queue", "queue");
  mux  = gst_element_factory_make ("matroskamux", "matroskamux");
  sink = gst_element_factory_make ("filesink", "filesink");

  if (!pipeline || !source || !demux || !queue || !mux || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  /* Set up the pipeline */

  /* we set the output filename to the sink element */
  g_print("Writing to %s\n", argv[1]);
  g_object_set (G_OBJECT (sink), "location", argv[1], NULL);

  /* we set the input filename to the source element */
  g_print("Reading from %s\n", argv[2]);
  g_object_set (G_OBJECT (source), "location", argv[2], NULL);

  /* make well formatted matroska files */
  g_object_set (G_OBJECT (mux), "writing-app", MATROSKA_APPNAME, NULL);
  g_object_set (G_OBJECT (mux), "min-index-interval", 
                                MATROSKA_MIN_INDEX_INTERVAL, NULL);

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* we add all elements into the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, demux, queue, mux, sink, NULL);
  gst_element_link_many (source, demux, queue, mux, sink,NULL);

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
