pragma ComponentBehavior: Bound

import QtQuick
import ImageCompare 1.0

Item {
    id: root
    property var model
    property var controller
    property bool overlay_mode_enabled: false
    property int active_overlay_source_index: 0
    property string active_entry_id: ""
    property string focused_entry_id: ""
    property alias match_zoom_enabled: interaction_controller.match_zoom_enabled
    readonly property string zoom_readout: active_zoom_readout()
    readonly property int pane_count: pane_repeater.count
    readonly property int overlay_source_pane_count: source_pane_count()
    readonly property int unlocked_pane_count: unlocked_count()
    readonly property bool can_best_fit_action: pane_count > 0
    readonly property bool can_overlay_action: overlay_source_pane_count > 1
    readonly property bool unlocked_pane_images_same_resolution: unlocked_panes_share_resolution()
    readonly property bool can_match_zoom_action: unlocked_pane_count > 1 && !unlocked_pane_images_same_resolution
    readonly property int layout_pane_count: effective_pane_count()
    readonly property int column_count: layout_pane_count <= 1 ? 1 : (layout_pane_count === 2 ? 2 : (layout_pane_count === 3 ? 3 : 2))
    readonly property int row_count: layout_pane_count === 0 ? 1 : Math.ceil(layout_pane_count / column_count)

    function source_pane_count(): int {
        let source_count = 0;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (pane && pane.source_pane_value) {
                source_count += 1;
            }
        }
        return source_count;
    }

    function unlocked_count(): int {
        let count = 0;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (pane && !pane.synchronization_locked) {
                count += 1;
            }
        }
        return count;
    }

    function unlocked_panes_share_resolution(): bool {
        let baseline_width = 0.0;
        let baseline_height = 0.0;
        let comparable_panes = 0;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane || pane.synchronization_locked) {
                continue;
            }
            const width = Number(pane.image_pixel_width);
            const height = Number(pane.image_pixel_height);
            if (width <= 0.0 || height <= 0.0) {
                return false;
            }
            if (comparable_panes === 0) {
                baseline_width = width;
                baseline_height = height;
            } else if (width !== baseline_width || height !== baseline_height) {
                return false;
            }
            comparable_panes += 1;
        }
        return comparable_panes > 1;
    }

    function active_zoom_readout(): string {
        const pane = pane_for_entry_id(active_entry_id);
        const zoom = pane ? Number(pane.zoom_factor_value) : Number(interaction_controller.shared_zoom_factor);
        return Math.round(zoom * 100.0) + "%";
    }

    function effective_pane_count(): int {
        if (focused_entry_id.length > 0) {
            return pane_count > 0 ? 1 : 0;
        }
        if (!overlay_mode_enabled || overlay_source_pane_count <= 1) {
            return pane_count;
        }
        return pane_count - overlay_source_pane_count + 1;
    }

    function source_ordinal_for_repeater_index(repeater_index: int): int {
        let source_ordinal = 0;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane || !pane.source_pane_value) {
                continue;
            }
            if (index === repeater_index) {
                return source_ordinal;
            }
            source_ordinal += 1;
        }
        return -1;
    }

    function source_ordinal_for_entry_id(entry_id: string): int {
        let source_ordinal = 0;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane || !pane.source_pane_value) {
                continue;
            }
            if (pane.entry_id_value === entry_id) {
                return source_ordinal;
            }
            source_ordinal += 1;
        }
        return -1;
    }

    function source_entry_id_for_ordinal(target_ordinal: int): string {
        let source_ordinal = 0;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane || !pane.source_pane_value) {
                continue;
            }
            if (source_ordinal === target_ordinal) {
                return pane.entry_id_value;
            }
            source_ordinal += 1;
        }
        return "";
    }

    function pane_for_entry_id(entry_id: string): var {
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (pane && pane.entry_id_value === entry_id) {
                return pane;
            }
        }
        return null;
    }

    function validate_active_entry(): void {
        if (focused_entry_id.length > 0 && !pane_for_entry_id(focused_entry_id)) {
            focused_entry_id = "";
        }
        if (active_entry_id.length > 0 && pane_for_entry_id(active_entry_id)) {
            return;
        }

        const next_pane = interaction_controller.first_visible_pane();
        active_entry_id = next_pane ? next_pane.entry_id_value : "";
    }

    function activate_entry(entry_id: string): bool {
        const pane = pane_for_entry_id(entry_id);
        if (!pane) {
            return false;
        }

        active_entry_id = entry_id;
        if (overlay_mode_enabled && pane.source_pane_value) {
            const source_ordinal = source_ordinal_for_entry_id(entry_id);
            if (source_ordinal >= 0) {
                active_overlay_source_index = source_ordinal;
            }
        }
        return true;
    }

    function is_pane_visible_in_layout(repeater_index: int, source_pane: bool, entry_id: string): bool {
        if (focused_entry_id.length > 0) {
            return entry_id === focused_entry_id;
        }
        if (!overlay_mode_enabled || !source_pane || overlay_source_pane_count <= 1) {
            return true;
        }
        return source_ordinal_for_repeater_index(repeater_index) === active_overlay_source_index;
    }

    function normalize_overlay_selection(): void {
        const source_count = overlay_source_pane_count;
        if (source_count <= 0) {
            active_overlay_source_index = 0;
            return;
        }
        const normalized = ((active_overlay_source_index % source_count) + source_count) % source_count;
        if (normalized !== active_overlay_source_index) {
            active_overlay_source_index = normalized;
        }
    }

    function toggle_overlay_mode(): bool {
        if (overlay_source_pane_count <= 1) {
            overlay_mode_enabled = false;
            return false;
        }
        overlay_mode_enabled = !overlay_mode_enabled;
        normalize_overlay_selection();
        if (overlay_mode_enabled) {
            const active_pane = pane_for_entry_id(active_entry_id);
            if (active_pane && active_pane.source_pane_value) {
                const source_ordinal = source_ordinal_for_entry_id(active_entry_id);
                if (source_ordinal >= 0) {
                    active_overlay_source_index = source_ordinal;
                }
            } else {
                active_entry_id = source_entry_id_for_ordinal(active_overlay_source_index);
            }
        }
        return true;
    }

    function cycle_overlay_pane(step: int): bool {
        if (!overlay_mode_enabled || overlay_source_pane_count <= 1) {
            return false;
        }
        const direction = Number(step) >= 0 ? 1 : -1;
        active_overlay_source_index = active_overlay_source_index + direction;
        normalize_overlay_selection();
        active_entry_id = source_entry_id_for_ordinal(active_overlay_source_index);
        return true;
    }

    function sync_current_view_now(): bool {
        return interaction_controller.sync_current_view_now();
    }

    function toggle_entry_lock(entry_id: string): bool {
        const pane = pane_for_entry_id(entry_id);
        if (!pane) {
            return false;
        }

        if (!pane.synchronization_locked) {
            pane.local_best_fit_active = interaction_controller.shared_best_fit_active;
            pane.synchronization_locked = true;
            return true;
        }

        const source_pane = interaction_controller.active_sync_pane(pane);
        const rejoined = interaction_controller.rejoin_pane(pane, source_pane);
        pane.synchronization_locked = false;
        pane.local_best_fit_active = false;
        return rejoined;
    }

    function toggle_lock_active_pane(): bool {
        validate_active_entry();
        return toggle_entry_lock(active_entry_id);
    }

    function remove_entry(entry_id: string): bool {
        if (!root.controller || !entry_id) {
            return false;
        }
        const removed = root.controller.remove_workspace_entry_by_id(String(entry_id));
        if (removed && active_entry_id === entry_id) {
            active_entry_id = "";
        }
        if (removed && focused_entry_id === entry_id) {
            focused_entry_id = "";
        }
        Qt.callLater(validate_active_entry);
        return removed;
    }

    function remove_active_entry(): bool {
        validate_active_entry();
        return remove_entry(active_entry_id);
    }

    function move_entry(entry_id: string, direction: int): bool {
        if (overlay_mode_enabled || !root.controller || !entry_id) {
            return false;
        }
        const moved = root.controller.move_workspace_entry_by_id(String(entry_id), direction);
        if (moved) {
            active_entry_id = String(entry_id);
            Qt.callLater(validate_active_entry);
        }
        return moved;
    }

    function move_active_pane(direction: int): bool {
        validate_active_entry();
        return move_entry(active_entry_id, direction);
    }

    function copy_active_path(): bool {
        validate_active_entry();
        const pane = pane_for_entry_id(active_entry_id);
        return pane && pane.source_pane_value && root.controller ? root.controller.copy_path_to_clipboard(pane.image_path_value) : false;
    }

    function export_entry_heatmap(entry_id: string): bool {
        return root.controller && entry_id ? root.controller.export_heatmap_by_id(String(entry_id)) : false;
    }

    function toggle_focus_active_pane(): bool {
        validate_active_entry();
        if (active_entry_id.length <= 0) {
            return false;
        }
        focused_entry_id = focused_entry_id === active_entry_id ? "" : active_entry_id;
        return true;
    }

    function clear_focus(): bool {
        if (focused_entry_id.length <= 0) {
            return false;
        }
        focused_entry_id = "";
        validate_active_entry();
        return true;
    }

    function toggle_zoom_fit(): bool {
        validate_active_entry();
        return interaction_controller.toggle_zoom_fit(pane_for_entry_id(active_entry_id));
    }

    onPane_countChanged: {
        if (overlay_mode_enabled && overlay_source_pane_count <= 1) {
            overlay_mode_enabled = false;
        }
        normalize_overlay_selection();
        if (pane_count > 0) {
            interaction_controller.shared_best_fit_active = true;
            interaction_controller.schedule_best_fit_refresh();
        }
        Qt.callLater(validate_active_entry);
    }
    onCan_match_zoom_actionChanged: {
        if (can_match_zoom_action && match_zoom_enabled) {
            Qt.callLater(sync_current_view_now);
        }
    }
    onOverlay_mode_enabledChanged: {
        normalize_overlay_selection()
        Qt.callLater(validate_active_entry)
    }
    onLayout_pane_countChanged: interaction_controller.schedule_best_fit_refresh()
    onWidthChanged: interaction_controller.schedule_best_fit_refresh()
    onHeightChanged: interaction_controller.schedule_best_fit_refresh()

    PaneGridInteractionController {
        id: interaction_controller
        pane_repeater: pane_repeater
    }

    Grid {
        id: pane_grid
        anchors.fill: parent
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        anchors.topMargin: 0
        anchors.bottomMargin: 2
        columns: root.column_count
        spacing: 3

        Repeater {
            id: pane_repeater
            model: root.model

            delegate: ComparisonPaneItem {
                required property int index
                required property string primaryHeader
                required property string secondaryHeader
                required property string entryId
                required property string imagePath
                required property bool isSource

                primary_header_text: primaryHeader
                secondary_header_text: secondaryHeader
                entry_id_value: entryId
                image_path_value: imagePath
                source_pane_value: isSource
                display_mode_value: root.controller && root.controller.workspace ? root.controller.workspace.display_mode : 1
                active: root.active_entry_id === entry_id_value
                focused: root.focused_entry_id === entry_id_value
                can_move: !root.overlay_mode_enabled
                visible: root.is_pane_visible_in_layout(index, source_pane_value, entry_id_value)
                width: (pane_grid.width - (pane_grid.columns - 1) * pane_grid.spacing) / pane_grid.columns
                height: (pane_grid.height - (root.row_count - 1) * pane_grid.spacing) / root.row_count
                clip: true
                onRemove_requested: entry_id => root.remove_entry(entry_id)
                onActivate_requested: entry_id => root.activate_entry(entry_id)
                onCopy_path_requested: path => source_pane_value && root.controller && root.controller.copy_path_to_clipboard(path)
                onOpen_folder_requested: path => source_pane_value && root.controller && root.controller.open_containing_folder(path)
                onMove_requested: (entry_id, direction) => root.move_entry(entry_id, direction)
                onExport_heatmap_requested: entry_id => root.export_entry_heatmap(entry_id)
                onSynchronization_lock_toggle_requested: entry_id => root.toggle_entry_lock(entry_id)
                onView_changed: (pane, zoom_factor, image_center) => {
                    if (pane.synchronization_locked && !interaction_controller.syncing_view_state) {
                        pane.local_best_fit_active = false;
                    }
                    interaction_controller.update_shared_from_pane(pane, zoom_factor, image_center)
                }
            }
        }
    }
}
