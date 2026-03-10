#include "core/WorkspaceDocument.h"

WorkspaceDocument::WorkspaceDocument(QObject* parent) : QObject(parent), m_entries_model(this) {}

QUuid WorkspaceDocument::add_source_entry(const QUuid& image_handle_id, const QString& path, const QSize& pixel_size) {
    if (m_entries.size() >= k_max_entries) {
        return {};
    }

    const bool previous_can_build_heatmap = can_build_heatmap();

    const QUuid entry_id = QUuid::createUuid();
    const ViewableImageEntry entry = ViewableImageEntry::from_source(entry_id, image_handle_id, path, pixel_size);
    m_entries.push_back(entry);
    m_entries_model.append_entry(entry);
    Q_EMIT entry_count_changed();
    if (previous_can_build_heatmap != can_build_heatmap()) {
        Q_EMIT can_build_heatmap_changed();
    }
    return entry_id;
}

QUuid WorkspaceDocument::add_derived_entry(const QUuid& image_handle_id, const QString& output_path, const QSize& pixel_size,
                                           const QString& secondary_header_label) {
    const bool previous_can_build_heatmap = can_build_heatmap();
    const int previous_entry_count = entry_count();

    for (int index = m_entries.size() - 1; index >= 0; --index) {
        if (!m_entries[index].is_derived()) {
            continue;
        }
        m_entries.removeAt(index);
        m_entries_model.remove_entry_at(index);
    }

    if (m_entries.size() >= k_max_entries) {
        if (previous_entry_count != entry_count()) {
            Q_EMIT entry_count_changed();
        }
        if (previous_can_build_heatmap != can_build_heatmap()) {
            Q_EMIT can_build_heatmap_changed();
        }
        return {};
    }

    const QUuid entry_id = QUuid::createUuid();
    const ViewableImageEntry entry =
        ViewableImageEntry::from_derived_heatmap(entry_id, image_handle_id, output_path, pixel_size, secondary_header_label);
    m_entries.push_back(entry);
    m_entries_model.append_entry(entry);
    if (previous_entry_count != entry_count()) {
        Q_EMIT entry_count_changed();
    }
    if (previous_can_build_heatmap != can_build_heatmap()) {
        Q_EMIT can_build_heatmap_changed();
    }
    return entry_id;
}

ViewableImageEntry WorkspaceDocument::entry_at(int index) const {
    if (index < 0 || index >= m_entries.size()) {
        return {};
    }
    return m_entries[index];
}

bool WorkspaceDocument::remove_entry_by_id(const QUuid& entry_id) {
    for (int index = 0; index < m_entries.size(); ++index) {
        if (m_entries[index].entry_id() == entry_id) {
            return remove_entry_at(index);
        }
    }
    return false;
}

bool WorkspaceDocument::remove_entry_at(int index) {
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }

    const bool previous_can_build_heatmap = can_build_heatmap();

    m_entries.removeAt(index);
    m_entries_model.remove_entry_at(index);
    Q_EMIT entry_count_changed();
    if (previous_can_build_heatmap != can_build_heatmap()) {
        Q_EMIT can_build_heatmap_changed();
    }
    return true;
}

bool WorkspaceDocument::remove_derived_heatmap_entries() {
    const bool previous_can_build_heatmap = can_build_heatmap();
    const int previous_entry_count = entry_count();

    for (int index = m_entries.size() - 1; index >= 0; --index) {
        if (!m_entries[index].is_derived()) {
            continue;
        }
        m_entries.removeAt(index);
        m_entries_model.remove_entry_at(index);
    }

    if (previous_entry_count == entry_count()) {
        return false;
    }

    Q_EMIT entry_count_changed();
    if (previous_can_build_heatmap != can_build_heatmap()) {
        Q_EMIT can_build_heatmap_changed();
    }
    return true;
}

void WorkspaceDocument::set_display_mode(DisplayMode mode) {
    if (m_display_mode == mode) {
        return;
    }
    m_display_mode = mode;
    Q_EMIT display_mode_changed();
}

int WorkspaceDocument::source_entry_count() const noexcept {
    int source_count = 0;
    for (const ViewableImageEntry& entry : m_entries) {
        if (entry.is_source()) {
            ++source_count;
        }
    }
    return source_count;
}

bool WorkspaceDocument::can_build_heatmap() const noexcept {
    int source_count = 0;
    QSize first_size;
    for (const ViewableImageEntry& entry : m_entries) {
        if (entry.is_derived()) {
            return false;
        }
        if (!entry.is_source()) {
            continue;
        }

        ++source_count;
        if (source_count > 2) {
            return false;
        }
        if (entry.pixel_size().isEmpty()) {
            return false;
        }
        if (!first_size.isValid()) {
            first_size = entry.pixel_size();
            continue;
        }
        if (entry.pixel_size() != first_size) {
            return false;
        }
    }
    return source_count == 2 && first_size.isValid();
}
