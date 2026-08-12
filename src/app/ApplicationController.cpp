#include "app/ApplicationController.h"

#include <QClipboard>
#ifdef IMAGECOMPARE_FLATPAK
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#endif
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUrl>
#ifdef IMAGECOMPARE_FLATPAK
#include <QVariantMap>
#endif
#include <algorithm>
#include <exception>
#include <utility>

#include "core/ImageSource.h"

#ifdef IMAGECOMPARE_FLATPAK
#include <sys/types.h>
#include <sys/xattr.h>
#endif

namespace {
    QString image_dialog_filter() {
        QStringList patterns;
        for (const QByteArray& format : QImageReader::supportedImageFormats()) {
            patterns.push_back(QStringLiteral("*.%1").arg(QString::fromLatin1(format).toLower()));
        }
        patterns.removeDuplicates();
        std::sort(patterns.begin(), patterns.end());
        return QStringLiteral("Images (%1)").arg(patterns.join(u' '));
    }

    bool write_png(const QImage& image, const QString& output_path) {
        QSaveFile output(output_path);
        if (!output.open(QIODevice::WriteOnly)) {
            qWarning().noquote() << "Failed to open heatmap export" << output_path << ":" << output.errorString();
            return false;
        }

        {
            QImageWriter writer(&output, QByteArrayLiteral("png"));
            writer.setCompression(1);
            if (!writer.write(image)) {
                qWarning().noquote() << "Failed to export heatmap to" << output_path << ":" << writer.errorString();
                return false;
            }
        }
        if (!output.commit()) {
            qWarning().noquote() << "Failed to commit heatmap export to" << output_path << ":" << output.errorString();
            return false;
        }
        return true;
    }

#ifdef IMAGECOMPARE_FLATPAK
    bool running_in_flatpak() { return qEnvironmentVariableIsSet("FLATPAK_ID") || QFileInfo::exists(QStringLiteral("/.flatpak-info")); }

    QString document_portal_host_path(const QString& path) {
        if (path.isEmpty()) {
            return {};
        }

        const QByteArray native_path = QFile::encodeName(path);
        constexpr const char* k_host_path_attribute = "user.document-portal.host-path";
        const ssize_t size = getxattr(native_path.constData(), k_host_path_attribute, nullptr, 0);
        constexpr ssize_t k_max_attribute_size = 64 * 1024;
        if (size <= 0 || size > k_max_attribute_size) {
            return {};
        }

        QByteArray value(size, Qt::Uninitialized);
        const ssize_t read_size = getxattr(native_path.constData(), k_host_path_attribute, value.data(), value.size());
        if (read_size <= 0 || read_size > value.size()) {
            return {};
        }
        value.resize(read_size);
        return QFile::decodeName(value);
    }

    QString user_visible_path(const QString& path) {
        if (!running_in_flatpak()) {
            return path;
        }
        const QString host_path = document_portal_host_path(path);
        return host_path.isEmpty() ? path : host_path;
    }

    bool open_containing_folder_with_portal(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"), QStringLiteral("/org/freedesktop/portal/desktop"),
                              QStringLiteral("org.freedesktop.portal.OpenURI"), QDBusConnection::sessionBus());
        if (!portal.isValid()) {
            return false;
        }

        const QDBusMessage reply = portal.call(QStringLiteral("OpenDirectory"), QString(),
                                               QVariant::fromValue(QDBusUnixFileDescriptor(file.handle())), QVariantMap{});
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning().noquote() << "Failed to open containing folder through portal:" << reply.errorMessage();
            return false;
        }
        return true;
    }
#endif
}  // namespace

ApplicationController::ApplicationController(QObject* parent) : QObject(parent), m_workspace(this), m_image_comparison(this) {
    connect(&m_image_comparison, &ImageComparison::finished, this, &ApplicationController::on_comparison_finished);
}

void ApplicationController::import_image_paths(const QStringList& paths) {
    QSet<QString> existing_source_paths;
    for (int index = 0; index < m_workspace.entry_count(); ++index) {
        const auto entry = m_workspace.entry_at(index);
        if (entry.is_source()) {
            existing_source_paths.insert(ImageSource(entry.source_path).path());
        }
    }

    for (const QString& raw_path : paths) {
        QString normalized_path = raw_path.trimmed();
        if (normalized_path.isEmpty()) {
            continue;
        }

        const QUrl input_url(normalized_path);
        if (input_url.isValid() && input_url.isLocalFile()) {
            normalized_path = input_url.toLocalFile();
        }

        try {
            const ImageSource source(normalized_path);
            if (existing_source_paths.contains(source.path())) {
                continue;
            }
            if (m_workspace.add_source(normalized_path, source.pixel_size()).isNull()) {
                continue;
            }
            existing_source_paths.insert(source.path());
            clear_existing_heatmap();
        } catch (const std::exception& ex) {
            qWarning().noquote() << "Failed to import image path" << normalized_path << ":" << ex.what();
        }
    }
}

void ApplicationController::open_images_with_native_dialog() {
    const QStringList paths = QFileDialog::getOpenFileNames(nullptr, QStringLiteral("Open Images"), {}, image_dialog_filter());
    if (paths.isEmpty()) {
        return;
    }
    import_image_paths(paths);
}

bool ApplicationController::remove_workspace_entry_by_id(const QString& entry_id) {
    if (!m_workspace.remove_entry(QUuid(entry_id))) {
        return false;
    }
    clear_existing_heatmap();
    return true;
}

bool ApplicationController::move_workspace_entry_by_id(const QString& entry_id, int direction) {
    return m_workspace.move_entry(QUuid(entry_id), direction);
}

bool ApplicationController::copy_path_to_clipboard(const QString& path) const {
    if (path.isEmpty() || QGuiApplication::clipboard() == nullptr) {
        return false;
    }
#ifdef IMAGECOMPARE_FLATPAK
    QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(user_visible_path(path)));
#else
    QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(path));
#endif
    return true;
}

bool ApplicationController::open_containing_folder(const QString& path) const {
    const QFileInfo file_info(path);
    if (path.isEmpty() || file_info.absolutePath().isEmpty()) {
        return false;
    }
#ifdef IMAGECOMPARE_FLATPAK
    if (running_in_flatpak() && file_info.exists() && open_containing_folder_with_portal(path)) {
        return true;
    }
#endif
    return QDesktopServices::openUrl(QUrl::fromLocalFile(file_info.absolutePath()));
}

bool ApplicationController::export_heatmap_by_id(const QString& entry_id) const {
    const QUuid parsed_entry_id(entry_id);
    QImage heatmap;
    for (int index = 0; index < m_workspace.entry_count(); ++index) {
        const auto entry = m_workspace.entry_at(index);
        if (entry.entry_id == parsed_entry_id && entry.is_heatmap()) {
            heatmap = entry.heatmap;
            break;
        }
    }
    if (heatmap.isNull()) {
        return false;
    }

    QString default_directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (default_directory.isEmpty()) {
        default_directory = QDir::homePath();
    }
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-HHmmss"));
    const QString default_path = QDir(default_directory).filePath(QStringLiteral("imagecompare-heatmap-%1.png").arg(timestamp));
    QString output_path =
        QFileDialog::getSaveFileName(nullptr, QStringLiteral("Export Heatmap"), default_path, QStringLiteral("PNG Image (*.png)"));
    if (output_path.isEmpty()) {
        return false;
    }
    if (QFileInfo(output_path).suffix().isEmpty()) {
        output_path += QStringLiteral(".png");
    }
    return write_png(heatmap, output_path);
}

void ApplicationController::toggle_color_mode() {
    clear_existing_heatmap();
    m_workspace.set_color_mode(m_workspace.color_mode() == ColorMode::Faithful ? ColorMode::Raw : ColorMode::Faithful);
}

void ApplicationController::clear_existing_heatmap() {
    static_cast<void>(m_workspace.remove_heatmap());
    cancel_comparison();
}

void ApplicationController::build_heatmap() {
    if (heatmap_in_progress() || !m_workspace.can_build_heatmap()) {
        return;
    }

    const auto first = m_workspace.entry_at(0);
    const auto second = m_workspace.entry_at(1);
    m_heatmap_in_progress = true;
    m_image_comparison.start({
        .first_image_path = first.source_path,
        .second_image_path = second.source_path,
        .color_mode = m_workspace.color_mode(),
    });
    Q_EMIT heatmap_in_progress_changed();
}

WorkspaceModel* ApplicationController::workspace() noexcept { return &m_workspace; }

void ApplicationController::on_comparison_finished(ComparisonResult result) {
    if (!m_heatmap_in_progress) {
        return;
    }
    m_heatmap_in_progress = false;
    Q_EMIT heatmap_in_progress_changed();

    if (!result.succeeded()) {
        qWarning().noquote() << "Heatmap job failed:" << (result.error.isEmpty() ? QStringLiteral("unknown error") : result.error);
        return;
    }
    const QString summary = QStringLiteral("SSIM %1").arg(result.ssim, 0, 'f', 6);
    if (m_workspace.add_heatmap(std::move(result.heatmap), summary).isNull()) {
        qWarning().noquote() << "Failed to add heatmap result to the workspace";
    }
}

void ApplicationController::cancel_comparison() {
    m_image_comparison.cancel();
    if (!m_heatmap_in_progress) {
        return;
    }
    m_heatmap_in_progress = false;
    Q_EMIT heatmap_in_progress_changed();
}
