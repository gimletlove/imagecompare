#include "core/ViewableImageEntry.h"

#include <QFileInfo>
#include <utility>

namespace {
    QString format_file_size(qint64 bytes) {
        constexpr double kilobyte = 1024.0;
        constexpr double megabyte = 1024.0 * 1024.0;

        if (bytes >= static_cast<qint64>(megabyte)) {
            return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / megabyte, 0, 'f', 2);
        }
        if (bytes < static_cast<qint64>(kilobyte)) {
            return QStringLiteral("%1 B").arg(bytes);
        }
        return QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / kilobyte, 0, 'f', 2);
    }
}  // namespace

ViewableImageEntry ViewableImageEntry::from_source(QUuid entry_id, QUuid image_handle_id, QString path, QSize pixel_size) {
    ViewableImageEntry entry;
    entry.m_kind = EntryKind::Source;
    entry.m_entry_id = entry_id;
    entry.m_image_handle_id = image_handle_id;
    const QFileInfo file_info(path);
    entry.m_image_path = std::move(path);
    entry.m_primary_header_label = file_info.fileName();
    const QString resolution = QStringLiteral("%1x%2").arg(pixel_size.width()).arg(pixel_size.height());
    entry.m_secondary_header_label = QStringLiteral("%1 • %2").arg(resolution, format_file_size(file_info.size()));
    entry.m_pixel_size = pixel_size;
    return entry;
}

ViewableImageEntry ViewableImageEntry::from_derived_heatmap(QUuid entry_id, QUuid image_handle_id, QString output_path, QSize pixel_size,
                                                            QString secondary_header_label) {
    ViewableImageEntry entry;
    entry.m_kind = EntryKind::Derived;
    entry.m_entry_id = entry_id;
    entry.m_image_handle_id = image_handle_id;
    entry.m_image_path = std::move(output_path);
    entry.m_primary_header_label = QStringLiteral("Heatmap");
    entry.m_secondary_header_label =
        secondary_header_label.isEmpty() ? QStringLiteral("Generated heatmap") : std::move(secondary_header_label);
    entry.m_pixel_size = pixel_size;
    return entry;
}

bool ViewableImageEntry::is_source() const noexcept { return m_kind == EntryKind::Source; }

bool ViewableImageEntry::is_derived() const noexcept { return m_kind == EntryKind::Derived; }

QSize ViewableImageEntry::pixel_size() const noexcept { return m_pixel_size; }

QUuid ViewableImageEntry::entry_id() const noexcept { return m_entry_id; }

QUuid ViewableImageEntry::image_handle_id() const noexcept { return m_image_handle_id; }

QString ViewableImageEntry::image_path() const { return m_image_path; }

QString ViewableImageEntry::primary_header_label() const { return m_primary_header_label; }

QString ViewableImageEntry::secondary_header_label() const { return m_secondary_header_label; }
