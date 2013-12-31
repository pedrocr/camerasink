#include <gst/gst.h>
#include <glib.h>

typedef struct {
  guint       numfiles;
  guint       filenum;
  gchar     **filenames;
  GstElement *filesrc;
} FileInfo;

/* String to use when saving */
#define MATROSKA_APPNAME "camerasave"
/* Nanoseconds between indexes */
#define MATROSKA_MIN_INDEX_INTERVAL 1000000000

void padadd (GstElement *demux, GstPad *newpad, gpointer data) {
  GstElement *mux = (GstElement *) data;
  GstPad *mux_pad;

  mux_pad = gst_element_get_request_pad (mux, "video_0");
  if (!mux_pad) {
    g_printerr ("Couldn't get pad from matroskamux to connect to matroskademux\n");
    return;
  }
  gst_pad_link(newpad,mux_pad);
  gst_object_unref (GST_OBJECT (mux_pad));
}

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

void change_file(FileInfo *fi) {
  gchar *filename = fi->filenames[fi->filenum];

  g_print("Reading from %s\n", filename);
  g_object_set (G_OBJECT (fi->filesrc), "location", filename, NULL);
}

void usage () {
  g_printerr ("Usage: testread <output file> <input file1> ... <input file n>\n");
}

static GstPadProbeReturn
source_event (GstPad          *pad,
              GstPadProbeInfo *info,
              gpointer         data)
{
  FileInfo *fi = (FileInfo *) data;

  g_print("Got event: %s\n", gst_event_type_get_name(GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT (info))));
  
  if (GST_EVENT_EOS == GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT (info))) {
    (fi->filenum)++;
    if (fi->filenum < fi->numfiles) { /* If we're not past the last file */
      change_file(fi);
      return GST_PAD_PROBE_DROP;
    }
  }

  return GST_PAD_PROBE_PASS;
}

gboolean test_files(FileInfo *fl) {
  guint i;

  for(i=0; i < fl->filenum; i++) {
    if (!g_file_test(fl->filenames[i],G_FILE_TEST_EXISTS)) {
      g_printerr ("FATAL: \"%s\" doesn't exist\n", fl->filenames[i]);
      return FALSE;
    }
  }
  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;
  GstElement *pipeline, *source, *demux, *mux, *sink;
  GstBus *bus;
  GstPad *pad;
  FileInfo fi;
  guint bus_watch_id;

  /* Initialisation */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* Check input arguments */
  if (argc < 2) {
    usage();
    return -1;
  }

  fi.numfiles = argc - 2;
  fi.filenum = 0;
  fi.filenames = &argv[2];

  if (!test_files(&fi)) {
    usage();
    return -2;
  }

  /* Create elements */
  pipeline = gst_pipeline_new ("readfiles");
  source   = gst_element_factory_make ("filesrc", "filesrc");
  demux  = gst_element_factory_make ("matroskademux", "matroskademux");
  mux  = gst_element_factory_make ("matroskamux", "matroskamux");
  sink = gst_element_factory_make ("filesink", "filesink");

  if (!pipeline || !source || !demux || !mux || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  fi.filesrc = source;

  /* Set up the pipeline */

  /* we set the output filename to the sink element */
  g_print("Writing to %s\n", argv[1]);
  g_object_set (G_OBJECT (sink), "location", argv[1], NULL);

  /* we set the input filename to the source element */
  change_file(&fi);

  /* make well formatted matroska files */
  g_object_set (G_OBJECT (mux), "writing-app", MATROSKA_APPNAME, NULL);
  g_object_set (G_OBJECT (mux), "min-index-interval", 
                                (guint64) MATROSKA_MIN_INDEX_INTERVAL, NULL);

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* we add all elements into the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, demux, mux, sink, NULL);
  gst_element_link (source, demux);
  gst_element_link (mux, sink);

  /* Connect the video pad of the demux when it shows up */
  g_signal_connect (demux, "pad-added", G_CALLBACK(padadd), mux);

  /* Add a probe to react to EOS events are switch files */
  pad = gst_element_get_static_pad (source, "src");
  if (!pad) {
    g_printerr ("FATAL: Couldn't get pad from source to install probe\n");
    return -2;
  }
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM|
                          GST_PAD_PROBE_TYPE_BLOCK,
                     (GstPadProbeCallback) source_event, &fi, NULL);
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
