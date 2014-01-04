#include <stdlib.h>
#include <gst/gst.h>

void exit_if_true(gboolean condition, gchar *message) {
  if (condition) {
    g_printerr("FATAL: %s\n", message);
    exit(2);
  }
}

GstElement *my_gst_element_factory_make(const gchar *factoryname, const gchar *name) {
  GstElement *ret = gst_element_factory_make(factoryname, name);
  if (!ret) {
    g_printerr("FATAL: Couldn't create element %s/%s\n", factoryname, name);
    exit(2);
  }
  return ret;
} 

GstPad *my_gst_element_get_static_pad(GstElement *element, const gchar *name) {
  GstPad *ret = gst_element_get_static_pad (element, name);
  if (!ret) {
    g_printerr("FATAL: Couldn't get %s pad for element\n", name);
    exit(2);
  }
  return ret;
} 

GstPad *my_gst_element_get_request_pad(GstElement *element, const gchar *name) {
  GstPad *ret = gst_element_get_request_pad (element, name);
  if (!ret) {
    g_printerr("FATAL: Couldn't get %s pad for element\n", name);
    exit(2);
  }
  return ret;
} 


GstElement *my_gst_bin_new(const gchar *name) {
  GstElement *ret = gst_bin_new(name);
  if (!ret) {
    g_printerr("FATAL: Couldn't create bin %s\n", name);
    exit(2);
  }
  return ret;
} 

GstElement *my_gst_pipeline_new(const gchar *name) {
  GstElement *ret = gst_pipeline_new(name);
  if (!ret) {
    g_printerr("FATAL: Couldn't create pipeline %s\n", name);
    exit(2);
  }
  return ret;
} 
