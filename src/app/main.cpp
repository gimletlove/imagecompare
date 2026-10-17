#include <QApplication>
#include <QIcon>
#include <QImageReader>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSurfaceFormat>

#include "app/ApplicationController.h"

int main(int argc, char* argv[]) {
    qputenv("QML_DISK_CACHE", "aot");
    QImageReader::setAllocationLimit(2000);

    // Qt may choose a 565 (16-bit) format on EGL, when buffer sizes are not explicitly set, causing banding.
    QSurfaceFormat surface_format = QSurfaceFormat::defaultFormat();
    surface_format.setRedBufferSize(8);
    surface_format.setGreenBufferSize(8);
    surface_format.setBlueBufferSize(8);
    QSurfaceFormat::setDefaultFormat(surface_format);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("io.github.gimletlove.imagecompare"));
    app.setApplicationVersion(QStringLiteral(IMAGECOMPARE_VERSION));
    app.setDesktopFileName(QStringLiteral("io.github.gimletlove.imagecompare"));
    app.setWindowIcon(QIcon(QStringLiteral(":/imagecompare.svg")));

    ApplicationController controller;
    controller.import_image_paths(app.arguments().mid(1));

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.rootContext()->setContextProperty("application_controller", &controller);
    const QUrl mainUrl(QStringLiteral("qrc:/qt/qml/ImageCompare/src/ui/qml/Main.qml"));
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    engine.rootObjects().constFirst()->installEventFilter(&controller);

    return app.exec();
}
