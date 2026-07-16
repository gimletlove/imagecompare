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
#include <QSaveFile>
#include <QScopeGuard>
#include <QSet>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUrl>
#ifdef IMAGECOMPARE_FLATPAK
#include <QVariantMap>
#endif
#include <QVector>
#include <exception>
#include <utility>

#include "core/ImageSource.h"

#ifdef IMAGECOMPARE_FLATPAK
#include <sys/types.h>
#include <sys/xattr.h>
#endif

namespace {
#ifdef IMAGECOMPARE_FLATPAK
    bool running_in_flatpak() { return qEnvironmentVariableIsSet("FLATPAK_ID") || QFileInfo::exists(QStringLiteral("/.flatpak-info")); }

    QString document_portal_host_path(const QString& path) {
        if (path.isEmpty()) {
            return {};
        }

        const QByteArray native_path = QFile::encodeName(path);
        constexpr const char* k_host_path_attribute = "user.document-portal.host-path";
        const ssize_t size = getxattr(native_path.constData(), k_host_path_attribute, nullptr, 0);
        constexpr ssize_t k_max_reasonable_path_size = 64 * 1024;
        if (size <= 0 || size > k_max_reasonable_path_size) {
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

ApplicationController::ApplicationController(QObject* parent) : QObject(parent), m_workspace(this), m_job_queue(this) {
    connect(&m_job_queue, &ComparisonJobQueue::job_finished, this, &ApplicationController::on_job_finished);
    connect(&m_job_queue, &ComparisonJobQueue::job_failed, this, &ApplicationController::on_job_failed);
    connect(&m_workspace, &WorkspaceDocument::display_mode_changed, this, &ApplicationController::display_mode_changed);
}

ApplicationController::~ApplicationController() { remove_generated_heatmap(true); }

void ApplicationController::import_image_paths(const QStringList& paths) {
    QSet<QString> existing_source_paths;
    for (int index = 0; index < m_workspace.entry_count(); ++index) {
        const auto entry = m_workspace.entry_at(index);
        if (entry.is_source()) {
            existing_source_paths.insert(ImageSource(entry.image_path).path());
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
            if (!ImageSource::supported_image_path(normalized_path)) {
                continue;
            }
            const ImageSource source(normalized_path);
            if (existing_source_paths.contains(source.path())) {
                continue;
            }
            if (m_workspace.add_source_entry(normalized_path, source.pixel_size()).isNull()) {
                continue;
            }
            existing_source_paths.insert(source.path());
            cancel_pending_heatmap();
        } catch (const std::exception& ex) {
            qWarning().noquote() << "Failed to import image path" << normalized_path << ":" << ex.what();
        }
    }
}

void ApplicationController::open_images_with_native_dialog() {
    const QStringList paths =
        QFileDialog::getOpenFileNames(nullptr, QStringLiteral("Open Images"), {},
                                      QStringLiteral(
                                          "Images (*.png *.jpg *.jpeg *.webp *.avif *.jxl *.heic *.heif *.tif *.tiff *.bmp *.svg)"));
    if (paths.isEmpty()) {
        return;
    }
    import_image_paths(paths);
}

bool ApplicationController::remove_workspace_entry_by_id(const QString& entry_id) {
    const auto removed_entry = m_workspace.take_entry_by_id(QUuid(entry_id));
    if (!removed_entry) {
        return false;
    }
    if (removed_entry->is_source()) {
        cancel_pending_heatmap();
    } else if (removed_entry->image_path == m_generated_heatmap_path) {
        remove_generated_heatmap();
    }
    return true;
}

bool ApplicationController::move_workspace_entry_by_id(const QString& entry_id, int direction) {
    return m_workspace.move_entry_by_id(QUuid(entry_id), direction);
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
    QString heatmap_path;
    for (int index = 0; index < m_workspace.entry_count(); ++index) {
        const auto entry = m_workspace.entry_at(index);
        if (entry.entry_id == parsed_entry_id && entry.is_derived()) {
            heatmap_path = entry.image_path;
            break;
        }
    }
    if (heatmap_path.isEmpty()) {
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
    if (QFileInfo(heatmap_path).absoluteFilePath() == QFileInfo(output_path).absoluteFilePath()) {
        return true;
    }
    QFile input(heatmap_path);
    QSaveFile output(output_path);
    output.setDirectWriteFallback(true);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        qWarning().noquote() << "Failed to open heatmap export" << heatmap_path << "or" << output_path;
        return false;
    }

    constexpr qsizetype k_copy_buffer_size = 64 * 1024;
    while (true) {
        const QByteArray data = input.read(k_copy_buffer_size);
        if (data.isEmpty()) {
            if (input.error() == QFileDevice::NoError) {
                break;
            }
            output.cancelWriting();
            qWarning().noquote() << "Failed to read heatmap" << heatmap_path;
            return false;
        }
        if (output.write(data) != data.size()) {
            output.cancelWriting();
            qWarning().noquote() << "Failed to export heatmap" << heatmap_path << "to" << output_path;
            return false;
        }
    }
    if (!output.commit()) {
        qWarning().noquote() << "Failed to commit heatmap export to" << output_path;
        return false;
    }
    return true;
}

void ApplicationController::set_display_mode_faithful() { set_display_mode_and_reset_heatmap(DisplayMode::Faithful); }

void ApplicationController::set_display_mode_strict_raw() { set_display_mode_and_reset_heatmap(DisplayMode::StrictRaw); }

void ApplicationController::set_display_mode_and_reset_heatmap(DisplayMode mode) {
    if (m_workspace.display_mode() == mode) {
        return;
    }
    clear_existing_derived_heatmaps();
    m_workspace.set_display_mode(mode);
}

void ApplicationController::clear_existing_derived_heatmaps() {
    static_cast<void>(m_workspace.remove_derived_heatmap_entries());
    remove_generated_heatmap();
    cancel_pending_heatmap();
}

void ApplicationController::build_heatmap() {
    if (heatmap_in_progress() || !m_workspace.can_build_heatmap()) {
        return;
    }

    QVector<ViewableImageEntry> source_images;
    source_images.reserve(2);
    for (int index = 0; index < m_workspace.entry_count() && source_images.size() < 2; ++index) {
        const auto entry = m_workspace.entry_at(index);
        if (entry.is_source()) {
            source_images.push_back(entry);
        }
    }
    if (source_images.size() != 2 || source_images[0].image_path.isEmpty() || source_images[1].image_path.isEmpty()) {
        return;
    }

    clear_existing_derived_heatmaps();
    m_pending_heatmap_job = m_job_queue.enqueue({
        .first_image_path = source_images[0].image_path,
        .second_image_path = source_images[1].image_path,
        .display_mode = m_workspace.display_mode(),
    });
    Q_EMIT heatmap_in_progress_changed();
}

WorkspaceDocument* ApplicationController::workspace() noexcept { return &m_workspace; }

void ApplicationController::on_job_finished(QUuid job_id, const ComparisonResult& result) {
    if (m_pending_heatmap_job != job_id) {
        if (result.success && !result.output_path.isEmpty()) {
            delete_generated_heatmap_file(result.output_path);
        }
        return;
    }
    cancel_pending_heatmap();
    if (!result.success || result.output_path.isEmpty()) {
        return;
    }

    auto output_guard = qScopeGuard([this, &result] { delete_generated_heatmap_file(result.output_path); });
    try {
        const ImageSource result_source(result.output_path);
        const QString summary_label = QStringLiteral("overall dE00 %1 • peak dE00 %2")
                                          .arg(result.summary.overall_de00, 0, 'f', 2)
                                          .arg(result.summary.peak_de00, 0, 'f', 2);

        if (m_workspace.add_derived_entry(result.output_path, result_source.pixel_size(), summary_label).isNull()) {
            return;
        }
        m_generated_heatmap_path = result.output_path;
        output_guard.dismiss();
    } catch (const std::exception& ex) {
        qWarning().noquote() << "Failed to load generated heatmap result" << result.output_path << ":" << ex.what();
    }
}

void ApplicationController::on_job_failed(QUuid job_id, const QString& error_text) {
    if (m_pending_heatmap_job == job_id) {
        cancel_pending_heatmap();
    }
    qWarning().noquote() << "Heatmap job failed for" << job_id.toString(QUuid::WithoutBraces) << ":"
                         << (error_text.isEmpty() ? QStringLiteral("unknown error") : error_text);
}

void ApplicationController::cancel_pending_heatmap() {
    if (!heatmap_in_progress()) {
        return;
    }
    m_pending_heatmap_job = QUuid{};
    Q_EMIT heatmap_in_progress_changed();
}

void ApplicationController::delete_generated_heatmap_file(const QString& path) const {
    if (!QFileInfo::exists(path)) {
        return;
    }
    if (!QFile::remove(path)) {
        qWarning().noquote() << "Failed to remove generated heatmap file" << path;
    }
}

void ApplicationController::remove_generated_heatmap(bool immediate) {
    const QString path = std::exchange(m_generated_heatmap_path, {});
    if (path.isEmpty()) {
        return;
    }
    if (immediate) {
        delete_generated_heatmap_file(path);
        return;
    }
    QMetaObject::invokeMethod(this, [this, path]() { delete_generated_heatmap_file(path); }, Qt::QueuedConnection);
}
