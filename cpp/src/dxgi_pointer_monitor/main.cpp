#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QThread>

#include "DxgiPointerMonitor.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("DxgiPointerMonitor", "Main");

    // Create monitor in separate thread for testing
    QThread monitorThread;
    DxgiPointerMonitor *monitor = new DxgiPointerMonitor();
    monitor->moveToThread(&monitorThread);

    // Lambda to run the capture loop
    auto runCapture = [monitor]() {
        while (!QThread::currentThread()->isInterruptionRequested()) {
            bool visible = false;
            QPoint position{};
            QPoint hotspot{};
            QByteArray cursorData{};
            bool changed = false;
            monitor->capture(visible, position, hotspot, cursorData, changed);
            QThread::msleep(10);
        }
    };

    // Start the monitor thread
    QObject::connect(&monitorThread, &QThread::started, runCapture);
    monitorThread.start();

    // Clean up on exit
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&monitorThread, monitor]() {
        monitorThread.requestInterruption(); // Signal the loop to stop
        monitorThread.quit();
        monitorThread.wait();
        delete monitor; // Clean up the monitor object
    });

    int result = app.exec();

    return result;
}
