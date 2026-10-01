#include "app/ApplicationController.h"

#ifdef IMAGECOMPARE_FLATPAK
#include <KUrlMimeData>
#endif
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
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QMimeData>
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

    QList<QUrl> dropped_urls(const QMimeData* mime_data) {
#ifdef IMAGECOMPARE_FLATPAK
        if (QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(mime_data, KUrlMimeData::PreferLocalUrls); !urls.isEmpty()) {
            return urls;
        }
#endif
        return mime_data->urls();
    }

    void write_png(const QImage& image, const QString& output_path) {
        QSaveFile output(output_path);
        if (!output.open(QIODevice::WriteOnly)) {
            qWarning().noquote() << "Failed to open heatmap export" << output_path << ":" << output.errorString();
            return;
        }

        {
            QImageWriter writer(&output, QByteArrayLiteral("png"));
            writer.setCompression(1);
            if (!writer.write(image)) {
                qWarning().noquote() << "Failed to export heatmap to" << output_path << ":" << writer.errorString();
                return;
            }
        }
        if (!output.commit()) {
            qWarning().noquote() << "Failed to commit heatmap export to" << output_path << ":" << output.errorString();
        }
    }

#ifdef IMAGECOMPARE_FLATPAK
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
            existing_source_paths.insert(canonical_image_path(entry.source_path));
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
            const QString canonical_path = canonical_image_path(normalized_path);
            if (existing_source_paths.contains(canonical_path)) {
                continue;
            }
            if (m_workspace.add_source(normalized_path, probe_image_size(canonical_path)).isNull()) {
                continue;
            }
            existing_source_paths.insert(canonical_path);
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

void ApplicationController::copy_path_to_clipboard(const QString& path) const {
    if (path.isEmpty() || QGuiApplication::clipboard() == nullptr) {
        return;
    }
#ifdef IMAGECOMPARE_FLATPAK
    QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(user_visible_path(path)));
#else
    QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(path));
#endif
}

void ApplicationController::open_containing_folder(const QString& path) const {
    const QFileInfo file_info(path);
    if (path.isEmpty() || file_info.absolutePath().isEmpty()) {
        return;
    }
#ifdef IMAGECOMPARE_FLATPAK
    if (file_info.exists() && open_containing_folder_with_portal(path)) {
        return;
    }
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(file_info.absolutePath()));
}

void ApplicationController::export_heatmap_by_id(const QString& entry_id) const {
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
        return;
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
        return;
    }
    if (QFileInfo(output_path).suffix().isEmpty()) {
        output_path += QStringLiteral(".png");
    }
    write_png(heatmap, output_path);
}

void ApplicationController::toggle_color_mode() {
    clear_existing_heatmap();
    m_workspace.toggle_color_profile();
}

void ApplicationController::clear_existing_heatmap() {
    m_workspace.remove_heatmap();
    cancel_comparison();
}

void ApplicationController::build_heatmap() {
    if (heatmap_in_progress() || !m_workspace.can_build_heatmap()) {
        return;
    }

    m_heatmap_in_progress = true;
    m_image_comparison.start(m_workspace.entry_at(0).source_image, m_workspace.entry_at(1).source_image);
    Q_EMIT heatmap_in_progress_changed();
}

WorkspaceModel* ApplicationController::workspace() noexcept { return &m_workspace; }

bool ApplicationController::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::Drop) {
        auto* drop_event = static_cast<QDropEvent*>(event);
        const QList<QUrl> urls = dropped_urls(drop_event->mimeData());
        if (!urls.isEmpty()) {
            if (event->type() == QEvent::Drop) {
                import_image_paths(QUrl::toStringList(urls));
            }
            drop_event->acceptProposedAction();
        }
    }
    return QObject::eventFilter(watched, event);
}

void ApplicationController::on_comparison_finished(ComparisonResult result) {
    m_heatmap_in_progress = false;
    Q_EMIT heatmap_in_progress_changed();

    if (result.heatmap.isNull()) {
        return;
    }
    m_workspace.add_heatmap(std::move(result.heatmap), QStringLiteral("SSIM %1").arg(result.ssim, 0, 'f', 6));
}

void ApplicationController::cancel_comparison() {
    m_image_comparison.cancel();
    if (!m_heatmap_in_progress) {
        return;
    }
    m_heatmap_in_progress = false;
    Q_EMIT heatmap_in_progress_changed();
}
