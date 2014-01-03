#include <gst/gst.h>
#include <glib.h>

typedef struct {
  guint       numfiles;
  guint       filenum;
  gchar     **filenames;
  GstElement *filesrc;
  GstElement *mux;
  GstPad     *muxpad;
  GstElement *demux;
  GstElement *pipeline;
  GstClockTime bufferoffset;
  GstClockTime lastbuffertime;
} StreamInfo;

static GstPadProbeReturn source_event (GstPad *pad, GstPadProbeInfo *info, 
                                       gpointer data);
static GstPadProbeReturn source_buffer (GstPad *pad, GstPadProbeInfo *info, 
                                        gpointer data);

void padadd (GstElement *demux, GstPad *newpad, gpointer data) {
  StreamInfo *si = (StreamInfo *) data;
  
  if (!si->muxpad) {
    si->muxpad = gst_element_get_request_pad (si->mux, "video_0");
  }
  if (!si->muxpad) {
    g_printerr ("FATAL: Couldn't get pad from matroskamux to connect to matroskademux\n");
    /* FIXME: This should actually be fatal */
    return;
  }
  gst_pad_link(newpad,si->muxpad);

  /* Add a probe to react to EOS events and switch files */
  gst_pad_add_probe (newpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM|
                          GST_PAD_PROBE_TYPE_BLOCK,
                     (GstPadProbeCallback) source_event, si, NULL);
  /* Add a probe to adjust the frame timestamps */
  gst_pad_add_probe (newpad, GST_PAD_PROBE_TYPE_BUFFER|
                             GST_PAD_PROBE_TYPE_BLOCK,
                     (GstPadProbeCallback) source_buffer, si, NULL);
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

void create_source(StreamInfo *si) {
  gchar *filename = si->filenames[si->filenum];

  si->filesrc = gst_element_factory_make ("filesrc", "filesrc");
  si->demux  = gst_element_factory_make ("matroskademux", "matroskademux");
  if (!si->filesrc || !si->demux) {
    g_printerr("FATAL: Couldn't create filesrc or matroskademux");
    /* FIXME: we need to actually fail here */
  }
  /* Connect the video pad of the demux when it shows up */
  g_signal_connect (si->demux, "pad-added", G_CALLBACK(padadd), si);
  gst_bin_add_many (GST_BIN (si->pipeline), si->demux, si->filesrc, NULL);
  gst_element_link (si->filesrc, si->demux);
  g_print("Reading from %s\n", filename);
  g_object_set (G_OBJECT (si->filesrc), "location", filename, NULL);
}

gboolean change_file(StreamInfo *si) {
  gst_element_set_state (si->demux, GST_STATE_NULL);
  gst_element_set_state (si->filesrc, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(si->pipeline), si->demux);
    gst_bin_remove(GST_BIN(si->pipeline), si->filesrc);
    create_source(si);
  gst_element_set_state (si->demux, GST_STATE_PLAYING);
  gst_element_set_state (si->filesrc, GST_STATE_PLAYING);

  return FALSE;
}

void usage () {
  g_printerr ("Usage: testread <output.mp4> <input1.mkv> ... <inputN.mkv>\n");
}

static GstPadProbeReturn
source_event (GstPad          *pad,
              GstPadProbeInfo *info,
              gpointer         data)
{
  StreamInfo *si = (StreamInfo *) data;
 
  if (GST_EVENT_EOS == GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT (info))) {
    g_print("Got EOS!\n");

    (si->filenum)++;
    if (si->filenum < si->numfiles) { /* If we're not past the last file */
      g_idle_add((GSourceFunc) change_file, si);
      si->bufferoffset = si->lastbuffertime;
      return GST_PAD_PROBE_DROP;
    }
  }

  return GST_PAD_PROBE_PASS;
}

static GstPadProbeReturn
source_buffer (GstPad          *pad,
               GstPadProbeInfo *info,
               gpointer         data)
{
  StreamInfo *si = (StreamInfo *) data;

  GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER (info)) += si->bufferoffset;
  si->lastbuffertime = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER (info));
 
  return GST_PAD_PROBE_PASS;
}

gboolean test_files(StreamInfo *fl) {
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
  StreamInfo si;
  guint bus_watch_id;

  /* Initialisation */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* Check input arguments */
  if (argc < 2) {
    usage();
    return -1;
  }

  si.numfiles = argc - 2;
  si.filenum = 0;
  si.filenames = &argv[2];

  if (!test_files(&si)) {
    usage();
    return -2;
  }

  /* Create elements */
  pipeline = gst_pipeline_new ("readfiles");
  mux  = gst_element_factory_make ("mp4mux", "mp4mux");
  sink = gst_element_factory_make ("filesink", "filesink");

  if (!pipeline || !mux || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  si.pipeline = pipeline;
  si.mux = mux;
  si.demux = NULL;
  si.filesrc = NULL;
  si.muxpad = NULL;
  si.bufferoffset = 0;

  /* Set up the pipeline */

  /* we set the output filename to the sink element */
  g_print("Writing to %s\n", argv[1]);
  g_object_set (G_OBJECT (sink), "location", argv[1], NULL);

  /* we set the input filename to the source element */
  create_source(&si);

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
