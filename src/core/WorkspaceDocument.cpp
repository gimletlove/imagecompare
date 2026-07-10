#include "core/WorkspaceDocument.h"

#include <algorithm>

WorkspaceDocument::WorkspaceDocument(QObject* parent) : QObject(parent), m_entries_model(this) {}

QUuid WorkspaceDocument::add_source_entry(const QString& path, const QSize& pixel_size) {
    if (entry_count() >= k_max_entries) {
        return {};
    }

    const QUuid entry_id = QUuid::createUuid();
    const ViewableImageEntry entry = ViewableImageEntry::from_source(entry_id, path, pixel_size);
    m_entries_model.append_entry(entry);
    Q_EMIT entry_count_changed();
    return entry_id;
}

QUuid WorkspaceDocument::add_derived_entry(const QString& output_path, const QSize& pixel_size, const QString& secondary_header_label) {
    const int previous_entry_count = entry_count();

    for (int index = entry_count() - 1; index >= 0; --index) {
        if (!m_entries_model.entry_at(index).is_derived()) {
            continue;
        }
        m_entries_model.remove_entry_at(index);
    }

    if (entry_count() >= k_max_entries) {
        if (previous_entry_count != entry_count()) {
            Q_EMIT entry_count_changed();
        }
        return {};
    }

    const QUuid entry_id = QUuid::createUuid();
    const ViewableImageEntry entry = ViewableImageEntry::from_derived_heatmap(entry_id, output_path, pixel_size, secondary_header_label);
    m_entries_model.append_entry(entry);
    if (previous_entry_count != entry_count()) {
        Q_EMIT entry_count_changed();
    }
    return entry_id;
}

ViewableImageEntry WorkspaceDocument::entry_at(int index) const { return m_entries_model.entry_at(index); }

bool WorkspaceDocument::remove_entry_by_id(const QUuid& entry_id) {
    for (int index = 0; index < entry_count(); ++index) {
        if (m_entries_model.entry_at(index).entry_id == entry_id) {
            return remove_entry_at(index);
        }
    }
    return false;
}

bool WorkspaceDocument::remove_entry_at(int index) {
    if (index < 0 || index >= entry_count()) {
        return false;
    }

    m_entries_model.remove_entry_at(index);
    Q_EMIT entry_count_changed();
    return true;
}

bool WorkspaceDocument::remove_derived_heatmap_entries() {
    const int previous_entry_count = entry_count();

    for (int index = entry_count() - 1; index >= 0; --index) {
        if (!m_entries_model.entry_at(index).is_derived()) {
            continue;
        }
        m_entries_model.remove_entry_at(index);
    }

    if (previous_entry_count == entry_count()) {
        return false;
    }

    Q_EMIT entry_count_changed();
    return true;
}

bool WorkspaceDocument::move_entry_by_id(const QUuid& entry_id, int direction) {
    if (direction == 0) {
        return false;
    }

    int from = -1;
    for (int index = 0; index < entry_count(); ++index) {
        if (m_entries_model.entry_at(index).entry_id == entry_id) {
            from = index;
            break;
        }
    }
    if (from < 0) {
        return false;
    }

    const int last_index = entry_count() - 1;
    const int to = std::clamp(from + (direction < 0 ? -1 : 1), 0, last_index);
    if (from == to) {
        return false;
    }

    m_entries_model.move_entry(from, to);
    return true;
}

void WorkspaceDocument::set_display_mode(DisplayMode mode) {
    if (m_display_mode == mode) {
        return;
    }
    m_display_mode = mode;
    Q_EMIT display_mode_changed();
}

bool WorkspaceDocument::can_build_heatmap() const noexcept {
    int source_count = 0;
    QSize first_size;
    for (int index = 0; index < entry_count(); ++index) {
        const ViewableImageEntry entry = m_entries_model.entry_at(index);
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
        if (entry.pixel_size.isEmpty()) {
            return false;
        }
        if (!first_size.isValid()) {
            first_size = entry.pixel_size;
            continue;
        }
        if (entry.pixel_size != first_size) {
            return false;
        }
    }
    return source_count == 2 && first_size.isValid();
}
