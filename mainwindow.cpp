#include "mainwindow.h"

#include <gst/video/videooverlay.h>

#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), is_pipeline_playing_{false}, custom_data_{} {
  ui->setupUi(this);

  ::QObject::connect(ui->playButton, &::QPushButton::clicked, this, &::MainWindow::OnPlayButtonClicked);
  ::QObject::connect(ui->pauseButton, &::QPushButton::clicked, this, &::MainWindow::OnPauseButtonClicked);
  ::QObject::connect(ui->stopButton, &::QPushButton::clicked, this, &::MainWindow::OnStopButtonClicked);

  ::QObject::connect(ui->horizontalSlider, &::QAbstractSlider::valueChanged, this, &::MainWindow::OnSliderValueChanged);

  // ::QObject::connect(ui->startPipeline, &::QPushButton::clicked, this, &::MainWindow::onStartPipelineButtonClicked);

  /* Initialize our data structure */
  custom_data_.duration = GST_CLOCK_TIME_NONE;

  /* Create the elements */
  custom_data_.playbin = gst_element_factory_make("playbin", "playbin");
  // videosink_ = gst_element_factory_make("glsinkbin", "glsinkbin");

  // ximagesink_ = gst_element_factory_make("ximagesink", NULL);

  // if (videosink_ != NULL && ximagesink_ != NULL) {
  // g_printerr("Successfully created ximagesink and videosink\n");

  /* Set the Qt video widget to the video sink */
  custom_data_.sink_widget = ui->videoWidget;
  // } else {
  // g_printerr("Not all elements could be created.\n");
  // QApplication::instance()->exit(EXIT_FAILURE);
  // }

  // if (custom_data_.playbin == NULL || videosink_ == NULL) {
  //   if (!custom_data_.playbin) {
  //     g_printerr("Not all elements could be created, playbin is NULL.\n");
  //   } else {
  //     g_printerr("Not all elements could be created, videosink is NULL.\n");
  //   }

  //   QApplication::instance()->exit(EXIT_FAILURE);
  // }

  custom_data_.main_window = this;
  custom_data_.slider = ui->horizontalSlider;
  custom_data_.streams_list = ui->textWidget;

  /* Set the URI to play */
  g_object_set(custom_data_.playbin, "uri", "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
               NULL);

  /* Set the video-sink  */
  // g_object_set(custom_data_.playbin, "video-sink", videosink_, NULL);

  /* Connect to interesting signals in playbin */
  g_signal_connect(G_OBJECT(custom_data_.playbin), "video-tags-changed", (GCallback)TagsCallback, &custom_data_);
  g_signal_connect(G_OBJECT(custom_data_.playbin), "audio-tags-changed", (GCallback)TagsCallback, &custom_data_);
  g_signal_connect(G_OBJECT(custom_data_.playbin), "text-tags-changed", (GCallback)TagsCallback, &custom_data_);

  GstBus* bus;

  /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
  bus = gst_element_get_bus(custom_data_.playbin);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)ErrorCallback, &custom_data_);
  g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)EosCallback, &custom_data_);
  g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)StateChangedCallback, &custom_data_);
  g_signal_connect(G_OBJECT(bus), "message::application", (GCallback)ApplicationCallback, &custom_data_);
  gst_object_unref(bus);
}

MainWindow::~MainWindow() {
  /* Free resources */
  gst_element_set_state(custom_data_.playbin, GST_STATE_NULL);
  gst_object_unref(custom_data_.playbin);
  // gst_object_unref(videosink_);

  delete ui;
}

/* This function is called when the PLAY button is clicked */
void MainWindow::OnPlayButtonClicked() { gst_element_set_state(custom_data_.playbin, GST_STATE_PLAYING); }

/* This function is called when the PAUSE button is clicked */
void MainWindow::OnPauseButtonClicked() { gst_element_set_state(custom_data_.playbin, GST_STATE_PAUSED); }

/* This function is called when the STOP button is clicked */
void MainWindow::OnStopButtonClicked() { gst_element_set_state(custom_data_.playbin, GST_STATE_READY); }

/* This function is called when the slider changes its position. We perform a seek to the new position here. */
void MainWindow::OnSliderValueChanged(int current_value) {
  // const float value = current_value; //current_value / float(ui->horizontalSlider->maximum());

  gst_element_seek_simple(custom_data_.playbin, GST_FORMAT_TIME,
                          static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                          static_cast<gint64>(current_value * GST_SECOND));
}

/* This function is called when the main window is closed */
void MainWindow::closeEvent(QCloseEvent* event) {
  OnStopButtonClicked();

  event->accept();
}

bool MainWindow::RefreshUi(CustomData* custom_data) {
  /* We do not want to update anything unless we are in the PAUSED or PLAYING states */
  if (custom_data->state < GST_STATE_PAUSED) {
    return true;
  }

  /* If we didn't know it yet, query the stream duration */
  if (!GST_CLOCK_TIME_IS_VALID(custom_data->duration)) {
    if (!gst_element_query_duration(custom_data->playbin, GST_FORMAT_TIME, &custom_data->duration)) {
      g_printerr("Could not query current duration.\n");
    }
  } else {
    /* Set the range of the slider to the clip duration, in SECONDS */
    custom_data->slider->setMinimum(0);
    custom_data->slider->setMaximum(custom_data->duration / GST_SECOND - 1);
  }

  gint64 current = -1;

  if (gst_element_query_position(custom_data->playbin, GST_FORMAT_TIME, &current)) {
    /* Block the "value-changed" signal, so the slider_cb function is not
     * called (which would trigger a seek the user has not requested) */
    ::QObject::disconnect(custom_data->slider, &::QAbstractSlider::valueChanged, custom_data->main_window,
                          &::MainWindow::OnSliderValueChanged);
    /* Set the position of the slider to the current pipeline position, in SECONDS */
    custom_data->slider->setValue(double(current) / GST_SECOND);
    /* Re-enable the signal */
    ::QObject::connect(custom_data->slider, &::QAbstractSlider::valueChanged, custom_data->main_window,
                       &::MainWindow::OnSliderValueChanged);
  }

  return true;
}

void MainWindow::TagsCallback(GstElement* playbin, gint stream, CustomData* custom_data) {
  /* We are possibly in a GStreamer working thread, so we notify the main
   * thread of this event through a message in the bus */
  gst_element_post_message(
      playbin, gst_message_new_application(GST_OBJECT(playbin), gst_structure_new_empty("tags-changed-my")));
}

void MainWindow::ErrorCallback(GstBus* bus, GstMessage* msg, CustomData* custom_data) {
  GError* err;
  gchar* debug_info;

  /* Print error details on the screen */
  gst_message_parse_error(msg, &err, &debug_info);
  g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
  g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error(&err);
  g_free(debug_info);

  /* Set the pipeline to READY (which stops playback) */
  gst_element_set_state(custom_data->playbin, GST_STATE_READY);
}

void MainWindow::EosCallback(GstBus* bus, GstMessage* msg, CustomData* custom_data) {
  g_print("End of stream reached.\n");
  /* Set the pipeline to READY (which stops playback) */
  gst_element_set_state(custom_data->playbin, GST_STATE_READY);
}

void MainWindow::StateChangedCallback(GstBus* bus, GstMessage* msg, CustomData* custom_data) {
  GstState old_state;
  GstState new_state;
  GstState pending_state;

  gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

  // If an object that posted message our pipeline
  if (GST_MESSAGE_SRC(msg) == GST_OBJECT(custom_data->playbin)) {
    custom_data->state = new_state;
    g_print("State set to %s\n", gst_element_state_get_name(new_state));

    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
      /* For extra responsiveness, we refresh the GUI as soon as we reach the PAUSED state */
      RefreshUi(custom_data);
    }
  }
}

void MainWindow::AnalyzeStreams(CustomData* custom_data) {
  // Extract metadata from all the streams and write it to the text widget in the GUI

  gint i;
  GstTagList* tags;
  gchar* str;  //, *total_str;
  QString total_str;
  guint rate;
  gint n_video, n_audio, n_text;

  /* Clean current contents of the widget */
  custom_data->streams_list->clear();

  /* Read some properties */
  g_object_get(custom_data->playbin, "n-video", &n_video, NULL);
  g_object_get(custom_data->playbin, "n-audio", &n_audio, NULL);
  g_object_get(custom_data->playbin, "n-text", &n_text, NULL);

  for (i = 0; i < n_video; ++i) {
    tags = NULL;

    /* Retrieve the stream's video tags */
    g_signal_emit_by_name(custom_data->playbin, "get-video-tags", i, &tags);

    if (tags) {
      total_str = custom_data->streams_list->text() + QString("video stream %1:\n").arg(i);

      custom_data->streams_list->setText(total_str);

      gst_tag_list_get_string(tags, GST_TAG_VIDEO_CODEC, &str);

      total_str += QString("Codec: %1\n").arg(str ? str : "unknown");

      custom_data->streams_list->setText(total_str);

      g_free(str);
      gst_tag_list_free(tags);
    }
  }

  for (i = 0; i < n_audio; ++i) {
    tags = NULL;
    /* Retrieve the stream's audio tags */
    g_signal_emit_by_name(custom_data->playbin, "get-audio-tags", i, &tags);

    if (tags) {
      total_str = custom_data->streams_list->text() + QString("audio stream %1:\n").arg(i);
      custom_data->streams_list->setText(total_str);

      if (gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &str)) {
        total_str += QString("Codec: %1\n").arg(str);

        custom_data->streams_list->setText(total_str);
        g_free(str);
      }

      if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str)) {
        total_str += QString("Language: %1\n").arg(str);
        custom_data->streams_list->setText(total_str);
        g_free(str);
      }

      if (gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &rate)) {
        total_str += QString("Bitrate: %1\n").arg(rate);
        custom_data->streams_list->setText(total_str);
      }

      gst_tag_list_free(tags);
    }
  }

  for (i = 0; i < n_text; ++i) {
    tags = NULL;
    /* Retrieve the stream's subtitle tags */
    g_signal_emit_by_name(custom_data->playbin, "get-text-tags", i, &tags);

    if (tags) {
      total_str = custom_data->streams_list->text() + QString("subtitle stream %1:\n").arg(i);
      custom_data->streams_list->setText(total_str);

      if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str)) {
        total_str += QString("Language: %1\n").arg(str);
        custom_data->streams_list->setText(total_str);
        g_free(str);
      }

      gst_tag_list_free(tags);
    }
  }
}

void MainWindow::ApplicationCallback(GstBus* bus, GstMessage* msg, CustomData* custom_data) {
  /* This function is called when an "application" message is posted on the bus.
   * Here we retrieve the message posted by the tags_cb callback */

  if (g_strcmp0(gst_structure_get_name(gst_message_get_structure(msg)), "tags-changed-my") == 0) {
    /* If the message is the "tags-changed-my" (only one we are currently issuing), update
     * the stream info GUI */
    AnalyzeStreams(custom_data);
  }
}

void MainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);

  if (is_pipeline_playing_) {
    return;
  }

  is_pipeline_playing_ = true;

  gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(custom_data_.playbin), ui->videoWidget->winId());

  /* Start playing */
  auto ret = gst_element_set_state(custom_data_.playbin, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(custom_data_.playbin);

    QApplication::instance()->exit(EXIT_FAILURE);
  }

  /* Register a function that GLib will call every second */
  g_timeout_add_seconds(1, (GSourceFunc)RefreshUi, &custom_data_);
}
