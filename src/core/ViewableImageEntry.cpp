#include "core/ViewableImageEntry.h"

#include <QFileInfo>
#include <QLocale>
#include <utility>

ViewableImageEntry ViewableImageEntry::from_source(QUuid entry_id, QString path, QSize pixel_size) {
    ViewableImageEntry entry;
    entry.kind = EntryKind::Source;
    entry.entry_id = entry_id;
    const QFileInfo file_info(path);
    entry.image_path = std::move(path);
    entry.primary_header_label = file_info.fileName();
    const QString resolution = QStringLiteral("%1x%2").arg(pixel_size.width()).arg(pixel_size.height());
    entry.secondary_header_label =
        QStringLiteral("%1 • %2").arg(resolution, QLocale().formattedDataSize(file_info.size(), 2, QLocale::DataSizeTraditionalFormat));
    entry.pixel_size = pixel_size;
    return entry;
}

ViewableImageEntry ViewableImageEntry::from_derived_heatmap(QUuid entry_id, QString output_path, QSize pixel_size,
                                                            QString secondary_header_label) {
    ViewableImageEntry entry;
    entry.kind = EntryKind::Derived;
    entry.entry_id = entry_id;
    entry.image_path = std::move(output_path);
    entry.primary_header_label = QStringLiteral("Heatmap");
    entry.secondary_header_label =
        secondary_header_label.isEmpty() ? QStringLiteral("Generated heatmap") : std::move(secondary_header_label);
    entry.pixel_size = pixel_size;
    return entry;
}
