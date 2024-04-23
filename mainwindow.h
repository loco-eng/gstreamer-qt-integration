#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/qevent.h>

#include <QLabel>
#include <QMainWindow>
#include <QtWidgets/QSlider>

#include "gst/gst.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

  void OnPlayButtonClicked();

  void OnPauseButtonClicked();

  void OnStopButtonClicked();

  void closeEvent(QCloseEvent* event) override;

  void OnSliderValueChanged(int current_value);

  struct CustomData;

  /**
   * @brief This function is called periodically to refresh the GUI
   *
   * @param custom_data
   * @return true
   * @return false
   */
  static bool RefreshUi(CustomData* custom_data);

  /**
   * @brief This function is called when new metadata is discovered in the stream
   *
   * @param playbin
   * @param stream
   * @param data
   */
  static void TagsCallback(GstElement* playbin, gint stream, CustomData* custom_data);

  /**
   * @brief This function is called when an error message is posted on the bus
   *
   * @param bus
   * @param msg
   * @param data
   */
  static void ErrorCallback(GstBus* bus, GstMessage* msg, CustomData* custom_data);

  /**
   * @brief This function is called when an End-Of-Stream message is posted on the bus.
   *        We just set the pipeline to READY (which stops playback)
   * @param bus
   * @param msg
   * @param data
   */
  static void EosCallback(GstBus* bus, GstMessage* msg, CustomData* custom_data);

  /**
   * @brief This function is called when the pipeline changes states. We use it to
   *        keep track of the current state.
   * @param bus
   * @param msg
   * @param data
   */
  static void StateChangedCallback(GstBus* bus, GstMessage* msg, CustomData* custom_data);

  /**
   * @brief Extract metadata from all the streams and write it to the text widget in the GUI
   *
   * @param custom_data
   */
  static void AnalyzeStreams(CustomData* custom_data);

  /**
   * @brief This function is called when an "application" message is posted on the bus.
   *        Here we retrieve the message posted by the tags_cb callback
   * @param bus
   * @param msg
   * @param custom_data
   */
  static void ApplicationCallback(GstBus* bus, GstMessage* msg, CustomData* custom_data);

 protected:
  /**
   * @brief showEvent
   *
   * @param event
   */
  void showEvent(QShowEvent* event) override;

 private:
  Ui::MainWindow* ui;

 private:
  /**
   * @brief Structure to contain all our information, so we can pass it around
   */
  struct CustomData {
    MainWindow* main_window;
    GstElement* playbin;  // One and only one pipeline

    QWidget* sink_widget;  // The widget where our video will be displayed

    QSlider* slider;  // Slider widget to keep track of current position

    QLabel* streams_list;  // Text widget to display info about the streams

    // Assuming that in Qt framework it is not needed
    // std::uint32_t slider_update_signal_id;  // Signal ID for the slider update signal

    GstState state;  // Current state of the pipeline

    std::int64_t duration;  // Duration of the clip, in nanoseconds
  } custom_data_;

  bool is_pipeline_playing_;

  GstElement* videosink_;
  GstElement* ximagesink_;
};
#endif  // MAINWINDOW_H
