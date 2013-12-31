#include <gst/gst.h>
#include <glib.h>

typedef struct {
  guint       numfiles;
  guint       filenum;
  gchar     **filenames;
  GstElement *filesrc;
  GstElement *mux;
  GstElement *demux;
  GstElement *pipeline;
} FileInfo;

/* String to use when saving */
#define MATROSKA_APPNAME "camerasave"
/* Nanoseconds between indexes */
#define MATROSKA_MIN_INDEX_INTERVAL 1000000000

static GstPadProbeReturn source_event (GstPad *pad, GstPadProbeInfo *info, 
                                       gpointer data);

void padadd (GstElement *demux, GstPad *newpad, gpointer data) {
  FileInfo *fi = (FileInfo *) data;
  GstPad *mux_pad;

  mux_pad = gst_element_get_request_pad (fi->mux, "video_0");
  if (!mux_pad) {
    g_printerr ("Couldn't get pad from matroskamux to connect to matroskademux\n");
    return;
  }
  gst_pad_link(newpad,mux_pad);
  gst_object_unref (GST_OBJECT (mux_pad));

  /* Add a probe to react to EOS events and switch files */
  gst_pad_add_probe (newpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM|
                          GST_PAD_PROBE_TYPE_BLOCK,
                     (GstPadProbeCallback) source_event, fi, NULL);
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

  if (fi->demux) {
  gst_element_set_state (fi->demux, GST_STATE_NULL);
  gst_element_set_state (fi->filesrc, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(fi->demux), fi->pipeline);
    gst_bin_remove(GST_BIN(fi->filesrc), fi->pipeline);
  }
        fi->filesrc = gst_element_factory_make ("filesrc", "filesrc");
        fi->demux  = gst_element_factory_make ("matroskademux", "matroskademux");
        if (!fi->filesrc || !fi->demux) {
          g_printerr("FATAL: Couldn't create filesrc or matroskademux");
          /* FIXME: we need to actually fail here */
        }
        /* Connect the video pad of the demux when it shows up */
        g_signal_connect (fi->demux, "pad-added", G_CALLBACK(padadd), fi);
    gst_bin_add_many (GST_BIN (fi->pipeline), fi->demux, fi->filesrc, NULL);
    gst_element_link (fi->filesrc, fi->demux);
  g_print("Reading from %s\n", filename);
  g_object_set (G_OBJECT (fi->filesrc), "location", filename, NULL);
  gst_element_set_state (fi->demux, GST_STATE_PLAYING);
  gst_element_set_state (fi->filesrc, GST_STATE_PLAYING);
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
  GstElement *pipeline, *mux, *sink;
  GstBus *bus;
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
  mux  = gst_element_factory_make ("matroskamux", "matroskamux");
  sink = gst_element_factory_make ("filesink", "filesink");

  if (!pipeline || !mux || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  fi.pipeline = pipeline;
  fi.mux = mux;
  fi.demux = NULL;
  fi.filesrc = NULL;

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
  gst_bin_add_many (GST_BIN (pipeline), mux, sink, NULL);
  gst_element_link (mux, sink);

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
