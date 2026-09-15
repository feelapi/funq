#include <QByteArray>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

static const char QML_SOURCE[] = R"QML(
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    width: 640
    height: 480
    title: "funq QML test app"

    Column {
        anchors.centerIn: parent
        spacing: 12

        Button {
            objectName: "btn_one"
            text: "Button One"
        }
        Button {
            objectName: "btn_two"
            text: "Button Two"
        }
    }
}
)QML";

int main(int argc, char * argv[]) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
    engine.loadData(QByteArray(QML_SOURCE));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return app.exec();
}
