#include <QGuiApplication>
#include <QQuickView>
#include <QThread>

int main(int argc, char * argv[]) {
    QGuiApplication app(argc, argv);

    if (app.arguments().contains("--exit-after-startup")) {
        QThread::msleep(5000);
        return 0;
    }

    QQuickView view;
    view.setSource(QUrl(QStringLiteral("qrc:///qml/children.qml")));
    view.show();
    return app.exec();
}
