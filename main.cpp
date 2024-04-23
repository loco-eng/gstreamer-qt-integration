#include <QApplication>

#include "gst/gst.h"
#include "mainwindow.h"

int main(int argc, char *argv[]) {
  /* Initialize Qt application */
  QApplication a(argc, argv);

  /* Initialize GStreamer */
  gst_init(&argc, &argv);

  MainWindow w;
  w.show();
  return a.exec();
}
