#include <QByteArray>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QThread>

static const char QML_SOURCE[] = R"QML(
import QtQuick 2.0
import QtQuick.Window 2.0

Window {
    visible: true
    width: 640
    height: 480
    title: "funq QML test app"

    Column {
        anchors.centerIn: parent
        spacing: 12

        Text {
            objectName: "label_one"
            text: "Window One"
        }
        Text {
            objectName: "label_two"
            text: "Window Two"
        }
    }
}
)QML";

int main(int argc, char * argv[]) {
    QGuiApplication app(argc, argv);

    if (app.arguments().contains("--exit-after-startup")) {
        QThread::msleep(5000);
        return 0;
    }

    QQmlApplicationEngine engine;
    engine.loadData(QByteArray(QML_SOURCE));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return app.exec();
}
