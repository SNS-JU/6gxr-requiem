/*
 * Copyright (C) 2024 by its authors (See AUTHORS).  Licensed under the
 * EUPL-1.2 or later.  See LICENSE for the exact licensing conditions.
 */

/*
from the answer,
https://stackoverflow.com/questions/8093967/rtsp-pipeline-implemented-via-c-code-not-working
*/

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/gstpad.h>
#include <gst/rtsp/gstrtsp.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <string>
#include <map>
#include <getopt.h>
#include <math.h>
#include <unistd.h>

using namespace std;

typedef struct myDataTag {
	GstElement *pipeline;
	GstElement *payloader;
} myData_t;

myData_t appData;

size_t counted_buffers = 0;

string quinnquicsrc = "quinnquicsrc0";

#define LOG_STORAGE_DEFFAULT_CAPACITY 10240

/* ************************************************************************* */

typedef struct log_item {
  uint32_t rtp_ts;
  uint64_t sys_ts_ns;
  uint32_t rtp_seq_num;
  uint32_t marker_bit;
} log_item_t;

typedef struct log_storage {
  log_item_t *items;
  log_item_t *next_free;
  int remaining;
} log_storage_t;

log_storage_t* log_new(int capacity)
{
  log_storage_t *log = (log_storage_t*)malloc(sizeof(log_storage_t));
  log_item_t *items = (log_item_t*)malloc(capacity * sizeof(log_item_t));
  if (log == NULL || items == NULL) {
    g_printerr("Failed to malloc log_storage.\n");
    return NULL;
  }
  memset(log, 0, sizeof(log_storage_t));
  log->items = items;
  log->remaining = capacity;
  log->next_free = log->items;
  return log;
}

void log_free(log_storage_t *log)
{
  if (log != NULL) {
    free(log->items);
    free(log);
  }
}

int log_add_item(log_storage_t *log,
                 uint32_t rtp_ts, uint64_t sys_ts_ns,
                 uint32_t rtp_seq_num, uint32_t marker_bit)
{
  if (log->remaining == 0)
    return -1;
  log->next_free->rtp_ts = rtp_ts;
  log->next_free->sys_ts_ns = sys_ts_ns;
  log->next_free->rtp_seq_num = rtp_seq_num;
  log->next_free->marker_bit = marker_bit;
  log->next_free ++;
  log->remaining --;
  return 0;
}

int log_save(log_storage_t *log, const string filename)
{
  FILE *f = fopen(filename.c_str(), "w");
  if (f == NULL) {
    g_printerr("log_save: Failed to open %s.\n", filename.c_str());
  }
  int error_code = 0;
  if (log->remaining == 0) {
    error_code = 2;
  }
  // print headers
  fprintf(f, "rtp-ts sys-ts-ns rtp-seq-num marker-bit error\n");
  for (log_item_t *i = log->items; i != log->next_free; i++) {
    fprintf(f, "%" G_GUINT32_FORMAT " %ld %" G_GUINT32_FORMAT " %d %d\n",
            i->rtp_ts, i->sys_ts_ns, i->rtp_seq_num, i->marker_bit, error_code);
  }
  return 0;
}

typedef struct iqa_log_item {
  guint32 iqa_frame_id;
  gdouble time_ms;
  gdouble dssim_value;
} iqa_log_item_t;

typedef struct iqa_log_storage {
  iqa_log_item_t *items;
  iqa_log_item_t *next_free;
  int remaining;
} iqa_log_storage_t;

iqa_log_storage_t* iqa_log_new(int capacity)
{
  iqa_log_storage_t *log = (iqa_log_storage_t*)malloc(sizeof(iqa_log_storage_t));
  iqa_log_item_t *items = (iqa_log_item_t*)malloc(capacity * sizeof(iqa_log_item_t));
  if (log == NULL || items == NULL) {
    g_printerr("Failed to malloc log_storage.\n");
    return NULL;
  }
  memset(log, 0, sizeof(iqa_log_storage_t));
  log->items = items;
  log->remaining = capacity;
  log->next_free = log->items;
  return log;
}

void iqa_log_free(iqa_log_storage_t *log)
{
  if (log != NULL) {
    free(log->items);
    free(log);
  }
}

int iqa_log_add_item(iqa_log_storage_t *log,
                     guint32 iqa_frame_id,
                     gdouble time_ms,
                     gdouble dssim_value)
{
  if (log->remaining == 0)
    return -1;
  log->next_free->iqa_frame_id = iqa_frame_id;
  log->next_free->time_ms = time_ms;
  log->next_free->dssim_value = dssim_value;
  log->next_free ++;
  log->remaining --;
  return 0;
}

int iqa_log_save(iqa_log_storage_t *log, const string filename)
{
  FILE *f = fopen(filename.c_str(), "w");
  if (f == NULL) {
    g_printerr("log_save: Failed to open %s.\n", filename.c_str());
  }
  int error_code = 0;
  if (log->remaining == 0) {
    error_code = 2;
  }
  // print headers
  fprintf(f, "iqa-frame-seq-num iqa-time-ms dssim-value error\n");
  for (iqa_log_item_t *i = log->items; i != log->next_free; i++) {
    fprintf(f, "%" G_GUINT32_FORMAT " %f %f %d\n",
            i->iqa_frame_id, i->time_ms, i->dssim_value, error_code);
  }
  return 0;
}

/* ************************************************************************* */

/* Save QUIC statistics.  Basically, print a GstStructure object as a
 * json-formatted string.  Use a rudimentary, incomplete json
 * serialization algorithm.
 */

typedef struct print_data {
  FILE *f;
  gboolean need_comma;
} print_data_t;

gboolean print_value(GQuark field, const GValue * value, gpointer gp);
gboolean print_structure(const GstStructure *structure, gpointer gp)
{
  print_data_t pd = { f:((print_data_t*)gp)->f, need_comma: FALSE };
  fprintf(pd.f, "{");

  gst_structure_foreach(structure, print_value, (gpointer)&pd);
  fprintf(pd.f, "}");
  return TRUE;
}

void print_comma(gpointer gp)
{
  print_data_t *pd = (print_data_t*) gp;
  if (pd->need_comma)
    fprintf(pd->f, ", ");
  pd->need_comma = TRUE;
}

gboolean print_value(GQuark field, const GValue * value, gpointer gp)
{
  print_data_t *pd = (print_data_t*) gp;
  print_comma(gp);
  fprintf(pd->f, "\"%s\": ", g_quark_to_string(field));
  if (GST_VALUE_HOLDS_STRUCTURE(value)) {
    print_structure(gst_value_get_structure(value), gp);
    return TRUE;
  }
  auto valueType = G_VALUE_TYPE(value);
  switch (valueType) {
  case G_TYPE_BOOLEAN:
    fprintf(pd->f, "%s", g_value_get_boolean(value)? "true": "false");
    break;
  case G_TYPE_UINT64:
    fprintf(pd->f, "%lu", g_value_get_uint64(value));
    break;
  case G_TYPE_UCHAR:
    fprintf(pd->f, "%u", g_value_get_uchar(value));
    break;
  default:
    fprintf(pd->f, "\"unknown type: %s\"", G_VALUE_TYPE_NAME(value));
  }
  return TRUE;
}


void print_stats(GstElement *pipeline, const char* outfile)
{
    GstElement *quicsrc = gst_bin_get_by_name(GST_BIN(appData.pipeline),
                                              quinnquicsrc.c_str());
    if (quicsrc == NULL) {
      return;
    }

    GstStructure *stats;
    g_object_get(quicsrc, "stats", &stats, NULL);
    FILE *f = fopen(outfile, "w");
    if (f == NULL) {
      g_printerr("error: failed to open %s.\n", outfile)        ;
      return;
    }
    print_data_t pd = { f: f, need_comma: FALSE };
    print_structure(stats, (gpointer)&pd);
    fprintf(f, "\n");
    fclose(f);
}

/* ************************************************************************* */

/* Capture of metadata for sending time extraction and latency calculation  */
static GstPadProbeReturn
cb_have_pad(GstPad *pad,
             GstPadProbeInfo *info,
             gpointer user_data)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstBuffer *buffer;
  log_storage_t *storage = (log_storage_t*) user_data;

  buffer = GST_PAD_PROBE_INFO_BUFFER(info);
  buffer = gst_buffer_make_writable(buffer);
  if (buffer == NULL)
    return GST_PAD_PROBE_OK;

  if (G_UNLIKELY(!gst_rtp_buffer_map(buffer, GST_MAP_READ, &rtp))) {
    g_print("invalid_buffer\n");
  }
  else {
    struct timespec systime_st;
    clock_gettime(CLOCK_REALTIME, &systime_st);
    GstClockTime systime = (GstClockTime)systime_st.tv_sec * 1000000000 + \
      systime_st.tv_nsec;

    counted_buffers++;
    log_add_item(storage,
                 gst_rtp_buffer_get_timestamp(&rtp),
                 GST_TIME_AS_NSECONDS(systime),
                 gst_rtp_buffer_get_seq(&rtp),
                 gst_rtp_buffer_get_marker(&rtp));
  }

  return GST_PAD_PROBE_OK;
}

bool send_eos(GstElement* pipeline)
{
  if (pipeline != NULL) {
    gboolean handled = gst_element_send_event(pipeline,
                                              gst_event_new_eos());
    if(!handled) {
      g_print("EOS signal sent but was not handled\n");
    } else {
      g_print("EOS sent and handled\n");
      return true;
    }
  } else {
    g_print("Cannot send EOS pipeline==NULL\n");
  }
  return false;
}

char dry_run_arg[] = "dry-run";
char gst_plugin_path_arg[] = "gst-plugin-path";
char help_arg[] = "help";
char iqa_element_arg[] = "iqa-element";
char migrate_after_arg[] = "migrate-after";
char next_client_address_arg[] = "next-client-address";
char out_dir_arg[] = "out-dir";
char pipeline_arg[] = "pipeline";
char rtp_element_arg[] = "rtp-element";
char timeout_arg[] = "timeout";

string dry_run_doc = "Only print the pipeline, do not run it";
string gst_plugin_path_doc = "(GStreamer argument) Sets the GST_PLUGIN_PATH "
  "environment variable to PATH for the application";
string help_doc = "Display this help";
string iqa_element_doc = "Name of the GStreamer iqa element";
string migrate_after_doc = "Send a connection migration signal after "
  "SEC seconds";
string next_client_address_doc = "Send ADDR in connection migration signal";
string out_dir_doc = "Directory to save output files (default: /tmp)";
string pipeline_doc = "GStreamer pipeline to run";
string rtp_element_doc = "Name of the GStreamer (de)payloader element";
string timeout_doc = "The pipeline will quit after SEC seconds";

void print_flag(char* flag, string arg, string docs, int sep) {
  if (!arg.empty()) {
    arg = " " + arg;
  }
  char* p = flag;
  for (; *p != '\0'; ++p) {}
  int flag_len = p - flag;
  sep += 9 - flag_len - arg.length();
  sep = sep < 1 ? 4 : sep;
  string flag_s(flag);
  string sep_s(sep, ' ');
  string line = "  --" + flag_s + arg + sep_s + docs + "\n";

  fprintf(stderr, line.c_str(), flag);
}

void usage()
{
    int longest_flag_name = 23;

    fprintf(stderr, "GStreamer experimental pipeline runner\n");
    fprintf(stderr, "Usage: gst_run <options>\n");
    fprintf(stderr, "Options:\n");
    print_flag(dry_run_arg, "",
               dry_run_doc, longest_flag_name);
    print_flag(gst_plugin_path_arg, "PATH",
               gst_plugin_path_doc, longest_flag_name);
    print_flag(help_arg, "",
               help_doc, longest_flag_name);
    print_flag(iqa_element_arg, "NAME",
               iqa_element_doc, longest_flag_name);
    print_flag(migrate_after_arg, "SEC",
               migrate_after_doc, longest_flag_name);
    print_flag(next_client_address_arg, "ADDR",
               next_client_address_doc, longest_flag_name);
    print_flag(out_dir_arg, "DIR",
               out_dir_doc, longest_flag_name);
    print_flag(pipeline_arg, "PIPELINE",
               pipeline_doc, longest_flag_name);
    print_flag(rtp_element_arg, "NAME",
               rtp_element_doc, longest_flag_name);
    print_flag(timeout_arg, "SEC",
               timeout_doc, longest_flag_name);
}

int main(int argc, char *argv[])
{
    static struct option long_options[] = {
      {dry_run_arg,                  no_argument,       0, 0 },
      {gst_plugin_path_arg,          required_argument, 0, 0 },
      {help_arg,                     no_argument,       0, 0 },
      {iqa_element_arg,              required_argument, 0, 0 },
      {migrate_after_arg,            required_argument, 0, 0 },
      {next_client_address_arg,      required_argument, 0, 0 },
      {out_dir_arg,                  required_argument, 0, 0 },
      {pipeline_arg,                 required_argument, 0, 0 },
      {rtp_element_arg,              required_argument, 0, 0 },
      {timeout_arg,                  required_argument, 0, 0 },
      {0,                            0,                 0, 0 }
    };
    char option_string[512];
    int opt;
    int option_index = 0;

    bool dry_run = false;
    string gst_plugin_path = "";
    string iqa_element = "";
    size_t migrate_after = 0;
    string next_client_address = "";
    string out_dir = "/tmp";
    string pipeline = "";
    string rtp_element = "";
    size_t timeout = 0;

    while ((opt = getopt_long(argc, argv, option_string,
                              long_options, &option_index)) != -1) {
      const char *option_name = long_options[option_index].name;
      switch (opt) {
      case 0:
        if (strcmp(dry_run_arg, option_name) == 0) {
          dry_run = true;
          break;
        }
        if (strcmp(gst_plugin_path_arg, option_name) == 0) {
          gst_plugin_path = optarg;
          gst_plugin_path = "GST_PLUGIN_PATH=" + gst_plugin_path;
          break;
        }
        if (strcmp(help_arg, option_name) == 0) {
          usage();
          return 0;
        }
        if (strcmp(iqa_element_arg, option_name) == 0) {
          iqa_element = optarg;
          break;
        }
        if (strcmp(migrate_after_arg, option_name) == 0) {
          migrate_after = atoi(optarg);
          break;
        }
        if (strcmp(next_client_address_arg, option_name) == 0) {
          next_client_address = optarg;
          break;
        }
        if (strcmp(out_dir_arg, option_name) == 0) {
          out_dir = optarg;
          break;
        }
        if (strcmp(pipeline_arg, option_name) == 0) {
          pipeline = optarg;
          break;
        }
        if (strcmp(rtp_element_arg, option_name) == 0) {
          rtp_element = optarg;
          break;
        }
        if (strcmp(timeout_arg, option_name) == 0) {
          timeout = atoi(optarg);
          break;
        }
      default:
        usage();
        return 0;
      }
    }

    g_print("Starting:\n%s\n", pipeline.c_str());

	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;
    GstPad *probed_pad;

    if (!gst_plugin_path.empty()) {
      char* char_array = new char[gst_plugin_path.length() + 1];
      strcpy(char_array, gst_plugin_path.c_str());
      putenv(char_array);
    }

	gst_init(NULL, NULL);

    if (pipeline.empty()) {
      string arg_name = pipeline_arg;
      string error = "'--" + arg_name + "' needs to be specified\n";
      g_printerr("%s\n", error.c_str());
      usage();
      exit(-1);
    }

    log_storage_t *log = NULL;
    int log_capacity = LOG_STORAGE_DEFFAULT_CAPACITY;
    size_t rtp_element_pos = pipeline.find(rtp_element);
    bool deplayloader_ppln = false;
    if (!rtp_element.empty() && rtp_element_pos != string::npos) {

      for (size_t beg_pos = rtp_element_pos; beg_pos >= 0; beg_pos--) {
        if (pipeline[beg_pos] == '!') {
          string ppln_element = pipeline;
          ppln_element.erase(rtp_element_pos);
          if (ppln_element.find("depay", beg_pos) != string::npos) {
            deplayloader_ppln = true;
          }
          break;
        }
      }

      string pipeline_name = "";
      if (deplayloader_ppln) {
        pipeline_name = "videoplayer";
        g_print("Detected rtp depayloader (receiver): "
                "setting pipeline name to: '%s'; "
                "setting probe to '%s' sink pad\n",
                pipeline_name.c_str(),
                rtp_element.c_str());
      } else {
        pipeline_name = "videosource";
        g_print("Detected rtp payloader (sender): "
                "setting pipeline name to: '%s'; "
                "setting probe to '%s' src pad\n",
                pipeline_name.c_str(),
                rtp_element.c_str());
      }
      if (dry_run) exit(0);

      appData.pipeline = gst_pipeline_new(pipeline_name.c_str());
      appData.pipeline = gst_parse_launch(pipeline.c_str(), NULL);
      appData.payloader = gst_bin_get_by_name(GST_BIN(appData.pipeline),
                                              rtp_element.c_str());

      if (deplayloader_ppln) {
        probed_pad = gst_element_get_static_pad(appData.payloader, "sink");
      } else {
        probed_pad = gst_element_get_static_pad(appData.payloader, "src");
      }

      if ((log = log_new(log_capacity)) == NULL) {
        return -1;
      }
      gst_pad_add_probe(probed_pad, GST_PAD_PROBE_TYPE_BUFFER,
                        (GstPadProbeCallback)cb_have_pad, log, NULL);

    } else {
      // If the pipeline string has "depay" as its substring, we
      // assume that it is a receiver pipeline
      deplayloader_ppln = pipeline.find("depay") == string::npos ? \
        false : true;

      string pipeline_name = "";
      if (deplayloader_ppln) {
        pipeline_name = "videoplayer";
        g_print("Detected rtp depayloader (receiver): "
                "setting pipeline name to: '%s'; ",
                pipeline_name.c_str());
      } else {
        pipeline_name = "videosource";
        g_print("Detected rtp payloader (sender): "
                "setting pipeline name to: '%s'; ",
                pipeline_name.c_str());
      }
      g_print("no RTP timestamping pad probe is set ");
      if (rtp_element.empty()) {
        g_print("(--rtp-element is not set)\n");
      } else {
        g_print("(--rtp-element=%s is not found in the pipeline)\n",
                rtp_element.c_str());
      }
      if (dry_run) exit(0);

      appData.pipeline = gst_pipeline_new(pipeline_name.c_str());
      appData.pipeline = gst_parse_launch(pipeline.c_str(), NULL);
    }

	/* Set the pipeline to "playing" state*/
	ret = gst_element_set_state(appData.pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Unable to set the pipeline to the playing state.\n");
		gst_object_unref(appData.pipeline);
		return -1;
	}

	// Wait until error, timeout (real or due to migrate after) or EOS.
	bus = gst_element_get_bus(appData.pipeline);
    size_t next_timeout = 0;
    bool signal_emitted = false;
    if (timeout == 0) {
      next_timeout = migrate_after;
    } else {
      if (migrate_after == 0 ||
          timeout < migrate_after) {
        next_timeout = timeout;
        signal_emitted = true;
        if (timeout < migrate_after) {
          g_print("Warning: timeout is smaller than migrate-after,"
                  " ignoring migrate-after\n");
        }
      } else {
        next_timeout = migrate_after;
      }
    }
    guint64 gst_timeout_clock = next_timeout > 0 ?
      next_timeout * GST_SECOND : GST_CLOCK_TIME_NONE;
    GstMessageType msg_type;
    iqa_log_storage_t *iqa_log = NULL;
    if (iqa_element.empty()) {
      msg_type = (GstMessageType)(GST_MESSAGE_ERROR |
                                  GST_MESSAGE_EOS);
    } else {
      g_print("Collecting iqa DSSIM values and timestamps\n");
      msg_type = (GstMessageType)(GST_MESSAGE_ERROR |
                                  GST_MESSAGE_EOS |
                                  GST_MESSAGE_ELEMENT);
      if ((iqa_log = iqa_log_new(log_capacity)) == NULL) {
        return -1;
      }
    }
    bool stop_pipeline = false;
    bool eos_sent = false;
    guint32 iqa_frame_id = 1;

    while(!stop_pipeline) {
      msg = gst_bus_timed_pop_filtered(bus, gst_timeout_clock, msg_type);

      /* Parse message */
      if (msg == NULL) {
        // GStramer returned after the given time, decide whether to
        // emit signal as per migrate-after or send EOS as per timeout.
        if (migrate_after > 0 && !signal_emitted) {
          if (next_client_address.empty()) {
            g_print("Warning: not emitting signal at element '%s':"
                    " next-client-address is empty\n", quinnquicsrc.c_str());
          } else {
            g_print("Emitting signal at element '%s': %s=%s\n", quinnquicsrc.c_str(),
                    "rebind", next_client_address.c_str());
            g_signal_emit_by_name(gst_bin_get_by_name(GST_BIN(appData.pipeline),
                                                      quinnquicsrc.c_str()),
                                  "rebind", next_client_address.c_str(), NULL);
          }
          signal_emitted = true;
          next_timeout = timeout == 0 ? 0 : timeout - migrate_after;
          gst_timeout_clock = next_timeout > 0 ?
            next_timeout * GST_SECOND : GST_CLOCK_TIME_NONE;
          if (next_timeout != 0 || timeout == 0) {
            continue;
          }
        }
        if ((signal_emitted && next_timeout > 0) ||
            next_timeout == 0) {
          // Timeout occurred
          g_print("Timeout occurred\n");
          eos_sent = send_eos(appData.pipeline);
        }
      } else {
		GError *err;
		gchar *debug_info;

		switch (GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &err, &debug_info);
			g_printerr("Error received from element %s: %s\n",
                       GST_OBJECT_NAME(msg->src), err->message);
			g_printerr("Debugging information: %s\n",
                       debug_info ? debug_info : "none");
            eos_sent = send_eos(appData.pipeline);
			g_clear_error(&err);
			g_free(debug_info);
            stop_pipeline = true;
			break;
		case GST_MESSAGE_EOS:
			g_print("End-Of-Stream reached.\n");
            stop_pipeline = true;
			break;
        case GST_MESSAGE_ELEMENT:
          // Received message from an element, deal with only those
          // coming from the given iqa filter.
          if (strncmp(GST_MESSAGE_SRC_NAME(msg), iqa_element.c_str(),
                      iqa_element.length()) == 0) {
            const GstStructure* msg_structure = gst_message_get_structure(msg);
            // Structure name is fixed, see gst-plugins-bad/ext/iqa/iqa.c.
            if (strncmp(gst_structure_get_name(msg_structure), "IQA",
                        3) == 0) {
              const GstStructure* dssim_structure;
              // Field name as given in gst-plugins-bad/ext/iqa/iqa.c.
              gst_structure_get(msg_structure, "dssim", GST_TYPE_STRUCTURE,
                                &dssim_structure, NULL);
              if (dssim_structure) {
                guint64 time_ns;
                gdouble time_ms;
                gdouble dssim_value;
                // gst-plugins-bad/ext/iqa/iqa.c dynamically
                // determines the name of the field where it stores
                // the DSSIM value depending on how many video sources
                // are compared to each other. The current solution
                // (field name "sink_1") works for only the case when
                // two videos are compared to each other.
                gst_structure_get_double(dssim_structure, "sink_1",
                                         &dssim_value);
                gst_structure_get_uint64(msg_structure, "time", &time_ns);
                time_ms = time_ns/1000.0/1000.0;
                iqa_log_add_item(iqa_log, iqa_frame_id++,
                                 time_ms, dssim_value);
              }
            }
          }
          break;
		default:
			/* We should not reach here because we only asked for
               ERRORs and EOS */
			g_printerr("Unexpected message received.\n");
            stop_pipeline = true;
			break;
		}
		gst_message_unref(msg);
      }
    }
    print_stats(appData.pipeline, (out_dir + "/quicsrc.json").c_str());

    // Wait some time for the pipeline to react to EOS: if the
    // pipeline does not properly process the EOS, the filesink does
    // not terminate/close the video file properly, thus, the video is
    // not playable.
    if (eos_sent) sleep(2);

    /* Unreference the probed pad */
    if (!rtp_element.empty() && rtp_element_pos != string::npos) {
      gst_object_unref(probed_pad);

      string role = "src";
      if (deplayloader_ppln) {
        role = "sink";
      }
      log_save(log, out_dir + "/rtp-" + role + ".log");

      /* Free resources */
      log_free(log);
    }
    if (!iqa_element.empty()) {
      iqa_log_save(iqa_log, out_dir + "/iqa.log");
      iqa_log_free(iqa_log);
    }
	gst_object_unref(bus);
	gst_element_set_state(appData.pipeline, GST_STATE_NULL);
	gst_object_unref(appData.pipeline);
    g_print("counted-rtp-ts=%ld\n", counted_buffers);
	return 0;
}
