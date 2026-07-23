#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>

#include "app/ApplicationController.h"
#include "app/WindowDropFilter.h"

int main(int argc, char* argv[]) {
    qputenv("QML_DISK_CACHE", "aot");

    QApplication app(argc, argv);
    const QStringList launch_arguments = app.arguments().mid(1);
    app.setApplicationName(QStringLiteral("io.github.gimletlove.imagecompare"));
    app.setApplicationVersion(QStringLiteral(IMAGECOMPARE_VERSION));
    app.setDesktopFileName(QStringLiteral("io.github.gimletlove.imagecompare"));
    app.setWindowIcon(QIcon(QStringLiteral(":/imagecompare.svg")));

    ApplicationController controller;
    if (!launch_arguments.isEmpty()) {
        controller.import_image_paths(launch_arguments);
    }

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.rootContext()->setContextProperty("application_controller", &controller);
    const QUrl mainUrl(QStringLiteral("qrc:/qt/qml/ImageCompare/Main.qml"));
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    WindowDropFilter window_drop_filter(&controller);
    if (auto* root_window = qobject_cast<QWindow*>(engine.rootObjects().constFirst()); root_window != nullptr) {
        root_window->installEventFilter(&window_drop_filter);
    }

    return app.exec();
}
