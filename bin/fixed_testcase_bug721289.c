/* This is a testcase for https://bugzilla.gnome.org/show_bug.cgi?id=721289 */
#include <gst/gst.h>
#include <glib.h>

typedef struct {
  GstElement *pipeline;
  GMainLoop  *loop;

  GstElement *source;
  GstElement *queue;
  GstElement *output;
  
  gulong      probeid;
  gboolean    iniframe;
} StreamInfo;

void reset_probe (StreamInfo *si);
void change_output (StreamInfo *si);

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  StreamInfo *si = (StreamInfo *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("Caught EOS\n");
      change_output(si);
      reset_probe(si);
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

void change_output (StreamInfo *si){
  gst_element_set_state (si->output, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(si->pipeline), si->output);
      si->output = gst_element_factory_make ("fakesink", "fakesink");
      gst_bin_add (GST_BIN (si->pipeline), si->output);
    gst_element_link (si->queue, si->output);
  gst_element_set_state (si->output, GST_STATE_PLAYING);
}

static GstPadProbeReturn
cb_have_data (GstPad          *pad,
              GstPadProbeInfo *info,
              gpointer         data)
{
  GstBufferFlags flags;
  GstPad *outputpad;
  StreamInfo *si = (StreamInfo *) data;

  flags = GST_BUFFER_FLAGS(GST_PAD_PROBE_INFO_BUFFER (info));

  if (flags & GST_BUFFER_FLAG_DELTA_UNIT) {
    si->iniframe = FALSE;
  }

  if (!(flags & GST_BUFFER_FLAG_DELTA_UNIT)) {
    g_print("Caught an iframe buffer\n");
    if (!si->iniframe) {
      si->iniframe = TRUE;
      g_print("New I-frame let's swap output!\n");
      outputpad = gst_element_get_static_pad (si->output, "sink");
      gst_pad_send_event(outputpad, gst_event_new_eos());
      gst_object_unref (GST_OBJECT (outputpad));
      g_print("We leave the pad blocked\n");
      return GST_PAD_PROBE_OK;
    }
  }
  
  return GST_PAD_PROBE_PASS;
}

void reset_probe (StreamInfo *si) {
  GstPad *pad;

  /* Add a probe to react to I-frames at the output of the queue blocking it
   and letting frames pile up if needed */
  pad = gst_element_get_static_pad (si->queue, "src");
  if (!si->probeid) {
    si->probeid = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER|
                                          GST_PAD_PROBE_TYPE_BLOCK,
                                     (GstPadProbeCallback) cb_have_data, si, NULL);
  }
  g_print("We now unblock the pad\n");
  gst_pad_unblock(pad);
  gst_object_unref (pad);
}

int
main (int   argc,
      char *argv[])
{
  GstBus *bus;
  guint bus_watch_id;
  StreamInfo si;

  /* Initialisation */
  gst_init (&argc, &argv);
  si.loop = g_main_loop_new (NULL, FALSE);
  si.pipeline = gst_pipeline_new ("savefile");
  si.source   = gst_element_factory_make ("videotestsrc", "videotestsrc");
  si.queue  = gst_element_factory_make ("queue", "queue");
  si.output = gst_element_factory_make ("fakesink", "fakesink");
  si.probeid = 0;
  si.iniframe = FALSE;

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (si.pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, &si);
  gst_object_unref (bus);

  /* we add all elements into the pipeline */
  gst_bin_add_many (GST_BIN (si.pipeline), si.source, si.queue, si.output, NULL);
  gst_element_link_many (si.source, si.queue, si.output,NULL);

  /* Setup the data probe on queue element */
  reset_probe(&si);

  /* Set the pipeline to "playing" state*/
  gst_element_set_state (si.pipeline, GST_STATE_PLAYING);

  /* Iterate */
  g_print ("Running...\n");
  g_main_loop_run (si.loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (si.pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (si.pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (si.loop);

  return 0;
}
