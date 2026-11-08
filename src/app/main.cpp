#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QIcon>
#include <QImageReader>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QSettings>
#include <QSurfaceFormat>
#include <QVariantMap>
#include <cmath>
#include <memory>
#include <utility>

#include "app/ApplicationController.h"

namespace {
    const QString k_repository_url = QStringLiteral("https://github.com/gimletlove/imagecompare");
#ifdef IMAGECOMPARE_FLATPAK
    const QString k_about_text = k_repository_url + QStringLiteral("\nRunning in Flatpak");
#else
    const QString k_about_text = k_repository_url;
#endif

    // Stored as ~/.config/<app id>.conf. Without an organization, QSettings would add an "Unknown Organization" folder.
    QSettings app_settings() { return QSettings(QCoreApplication::applicationName(), QString()); }

    // Shows the window at its last windowed size and maximized state, and saves both on quit.
    void show_with_saved_geometry(QQuickWindow* window) {
        QSettings settings = app_settings();
        if (const QSize saved = settings.value(QStringLiteral("window/size")).toSize(); !saved.isEmpty()) {
            window->resize(saved.boundedTo(window->screen()->availableSize()));
        }

        // Platforms may report the maximized size before the maximized state, so keep the size before the last resize too.
        struct WindowedSizes {
            QSize current;
            QSize previous;
        };
        auto sizes = std::make_shared<WindowedSizes>(window->size(), window->size());
        const auto record_size = [window, sizes] {
            if (window->visibility() == QWindow::Windowed && window->size() != sizes->current) {
                sizes->previous = std::exchange(sizes->current, window->size());
            }
        };
        QObject::connect(window, &QWindow::widthChanged, window, record_size);
        QObject::connect(window, &QWindow::heightChanged, window, record_size);
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, window, [window, sizes] {
            // Closing the window hides it before quit, so visibility() is Hidden here; windowStates() keeps the flag.
            const bool maximized = window->windowStates().testFlag(Qt::WindowMaximized);
            QSettings settings = app_settings();
            settings.setValue(QStringLiteral("window/size"),
                              maximized && sizes->current == window->size() ? sizes->previous : sizes->current);
            settings.setValue(QStringLiteral("window/maximized"), maximized);
        });

        if (settings.value(QStringLiteral("window/maximized")).toBool()) {
            window->showMaximized();
        } else {
            window->show();
        }
    }
}  // namespace

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

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Image Compare\n") + k_about_text);
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption stack_option(QStringLiteral("stack"), QStringLiteral("Start in stacked view."));
    const QCommandLineOption zoom_option(QStringLiteral("zoom"), QStringLiteral("Starting zoom: a percentage such as 100, or fit."),
                                         QStringLiteral("percent|fit"), QStringLiteral("fit"));
    const QCommandLineOption raw_option(QStringLiteral("raw"), QStringLiteral("Start with raw rendering, ignoring color profiles."));
    const QCommandLineOption sync_option(
        QStringLiteral("sync"), QStringLiteral("How zoom and pan sync across different resolutions: relative or absolute."),
        QStringLiteral("relative|absolute"), QStringLiteral("relative"));
    parser.addOptions({stack_option, zoom_option, raw_option, sync_option});
    parser.addPositionalArgument(QStringLiteral("images"), QStringLiteral("Images to open."), QStringLiteral("[images...]"));

    // Only --version reads this text, the window title needs the bare version.
    app.setApplicationVersion(QStringLiteral(IMAGECOMPARE_VERSION "\n") + k_about_text);
    parser.process(app);
    app.setApplicationVersion(QStringLiteral(IMAGECOMPARE_VERSION));

    double zoom_percent = 0.0;
    if (const QString zoom = parser.value(zoom_option); zoom != QStringLiteral("fit")) {
        bool valid = false;
        zoom_percent = zoom.toDouble(&valid);
        if (!valid || !std::isfinite(zoom_percent) || zoom_percent <= 0.0) {
            qCritical().noquote() << "Invalid --zoom value:" << zoom;
            return 1;
        }
    }
    const QString sync = parser.value(sync_option);
    if (sync != QStringLiteral("relative") && sync != QStringLiteral("absolute")) {
        qCritical().noquote() << "Invalid --sync value:" << sync;
        return 1;
    }

    ApplicationController controller;
    controller.import_image_paths(parser.positionalArguments());
    if (parser.isSet(raw_option)) {
        controller.toggle_color_mode();
    }

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.rootContext()->setContextProperty("application_controller", &controller);
    engine.rootContext()->setContextProperty("application_startup_options", QVariantMap{
                                                                    {QStringLiteral("stack"), parser.isSet(stack_option)},
                                                                    {QStringLiteral("zoom_percent"), zoom_percent},
                                                                    {QStringLiteral("absolute_sync"), sync == QStringLiteral("absolute")},
                                                                });
    engine.rootContext()->setContextProperty("application_repository_url", k_repository_url);
    const QUrl mainUrl(QStringLiteral("qrc:/qt/qml/ImageCompare/src/ui/qml/Main.qml"));
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    engine.rootObjects().constFirst()->installEventFilter(&controller);
    show_with_saved_geometry(qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst()));

    return app.exec();
}
