#include <QtQml/qqml.h>

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>

#include "app/ApplicationController.h"
#include "app/WindowDropFilter.h"
#include "ui/TiledImageItem.h"

extern "C" {
    void vips_shutdown(void);
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const QStringList launch_arguments = app.arguments().mid(1);
    app.setApplicationName(QStringLiteral("io.github.gimletlove.imagecompare"));
    app.setDesktopFileName(QStringLiteral("io.github.gimletlove.imagecompare"));
    app.setWindowIcon(QIcon(QStringLiteral(":/imagecompare.svg")));

    qmlRegisterType<TiledImageItem>("ImageCompare", 1, 0, "TiledImageItem");
    ApplicationController controller;
    if (!launch_arguments.isEmpty()) {
        controller.import_image_paths(launch_arguments);
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("application_controller", &controller);
    const QUrl mainUrl(QStringLiteral("qrc:/Main.qml"));
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    auto* root_window = qobject_cast<QWindow*>(engine.rootObjects().constFirst());
    if (root_window != nullptr) {
        auto* window_drop_filter = new WindowDropFilter(&controller, root_window);
        root_window->installEventFilter(window_drop_filter);
    }

    const int exit_code = app.exec();
    vips_shutdown();
    return exit_code;
}
