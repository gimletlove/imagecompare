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
    readonly property int overlay_source_pane_count: source_panes().length
    readonly property int unlocked_pane_count: interaction_controller.unlocked_panes().length
    readonly property bool can_best_fit_action: pane_count > 0
    readonly property bool can_overlay_action: overlay_source_pane_count > 1
    readonly property bool unlocked_pane_images_same_resolution: unlocked_panes_share_resolution()
    readonly property bool can_match_zoom_action: unlocked_pane_count > 1 && !unlocked_pane_images_same_resolution
    readonly property int layout_pane_count: effective_pane_count()
    readonly property int column_count: layout_pane_count <= 1 ? 1 : (layout_pane_count === 3 ? 3 : Math.ceil(Math.sqrt(layout_pane_count)))
    readonly property int row_count: layout_pane_count === 0 ? 1 : Math.ceil(layout_pane_count / column_count)

    function source_panes() {
        return interaction_controller.panes().filter(pane => pane.is_source);
    }

    function unlocked_panes_share_resolution(): bool {
        const sizes = interaction_controller.unlocked_panes().map(pane => pane.image_item.image_pixel_size);
        return sizes.length > 1 && sizes.every(size => size.width > 0 && size.height > 0
            && size.width === sizes[0].width && size.height === sizes[0].height);
    }

    function active_zoom_readout(): string {
        const pane = pane_for_entry_id(active_entry_id);
        const zoom = pane ? pane.image_item.zoom_factor : 1.0;
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

    function source_ordinal_for_entry_id(entry_id: string): int {
        return source_panes().findIndex(pane => pane.entry_id === entry_id);
    }

    function source_entry_id_for_ordinal(target_ordinal: int): string {
        const pane = source_panes()[target_ordinal];
        return pane ? pane.entry_id : "";
    }

    function pane_for_entry_id(entry_id: string): var {
        return interaction_controller.panes().find(pane => pane.entry_id === entry_id) || null;
    }

    function validate_active_entry(): void {
        if (focused_entry_id.length > 0 && !pane_for_entry_id(focused_entry_id)) {
            focused_entry_id = "";
        }
        if (active_entry_id.length > 0 && pane_for_entry_id(active_entry_id)) {
            return;
        }

        const next_pane = interaction_controller.first_visible_pane();
        active_entry_id = next_pane ? next_pane.entry_id : "";
    }

    function activate_entry(entry_id: string): void {
        const pane = pane_for_entry_id(entry_id);
        if (!pane) {
            return;
        }

        active_entry_id = entry_id;
        if (overlay_mode_enabled && pane.is_source) {
            active_overlay_source_index = source_ordinal_for_entry_id(entry_id);
        }
    }

    function is_pane_visible_in_layout(source_pane: bool, entry_id: string): bool {
        if (focused_entry_id.length > 0) {
            return entry_id === focused_entry_id;
        }
        if (!overlay_mode_enabled || !source_pane || overlay_source_pane_count <= 1) {
            return true;
        }
        return source_ordinal_for_entry_id(entry_id) === active_overlay_source_index;
    }

    function normalize_overlay_selection(): void {
        const source_count = Math.max(1, overlay_source_pane_count);
        active_overlay_source_index = ((active_overlay_source_index % source_count) + source_count) % source_count;
    }

    function toggle_overlay_mode(): void {
        overlay_mode_enabled = !overlay_mode_enabled;
        if (overlay_mode_enabled) {
            const active_pane = pane_for_entry_id(active_entry_id);
            if (active_pane && active_pane.is_source) {
                active_overlay_source_index = source_ordinal_for_entry_id(active_entry_id);
            } else {
                active_entry_id = source_entry_id_for_ordinal(active_overlay_source_index);
            }
        }
    }

    function cycle_overlay_pane(step: int): void {
        active_overlay_source_index += step;
        normalize_overlay_selection();
        active_entry_id = source_entry_id_for_ordinal(active_overlay_source_index);
    }

    function sync_current_view_now(): void {
        interaction_controller.sync_current_view_now();
    }

    function toggle_entry_lock(entry_id: string): void {
        const pane = pane_for_entry_id(entry_id);
        if (!pane) {
            return;
        }

        if (!pane.synchronization_locked) {
            pane.local_best_fit_active = interaction_controller.shared_best_fit_active;
            pane.synchronization_locked = true;
            return;
        }

        interaction_controller.rejoin_pane(pane, interaction_controller.active_sync_pane(pane));
        pane.synchronization_locked = false;
        pane.local_best_fit_active = false;
    }

    function toggle_lock_active_pane(): void {
        validate_active_entry();
        toggle_entry_lock(active_entry_id);
    }

    function remove_entry(entry_id: string): void {
        if (!entry_id) {
            return;
        }
        const removed = root.controller.remove_workspace_entry_by_id(String(entry_id));
        if (removed && active_entry_id === entry_id) {
            active_entry_id = "";
        }
        if (removed && focused_entry_id === entry_id) {
            focused_entry_id = "";
        }
        Qt.callLater(validate_active_entry);
    }

    function remove_active_entry(): void {
        validate_active_entry();
        remove_entry(active_entry_id);
    }

    function move_entry(entry_id: string, direction: int): void {
        if (overlay_mode_enabled || !entry_id) {
            return;
        }
        if (root.controller.move_workspace_entry_by_id(String(entry_id), direction)) {
            active_entry_id = String(entry_id);
            Qt.callLater(validate_active_entry);
        }
    }

    function move_active_pane(direction: int): void {
        validate_active_entry();
        move_entry(active_entry_id, direction);
    }

    function copy_active_path(): void {
        validate_active_entry();
        const pane = pane_for_entry_id(active_entry_id);
        if (pane && pane.is_source) {
            root.controller.copy_path_to_clipboard(pane.source_path);
        }
    }

    function toggle_focus_active_pane(): void {
        validate_active_entry();
        focused_entry_id = focused_entry_id === active_entry_id ? "" : active_entry_id;
    }

    function clear_focus(): void {
        focused_entry_id = "";
        validate_active_entry();
    }

    function toggle_zoom_fit(): void {
        validate_active_entry();
        interaction_controller.toggle_zoom_fit(pane_for_entry_id(active_entry_id));
    }

    onPane_countChanged: {
        if (pane_count > 0) {
            interaction_controller.shared_best_fit_active = true;
            Qt.callLater(interaction_controller.refresh_best_fit);
        }
        Qt.callLater(validate_active_entry);
    }
    onOverlay_source_pane_countChanged: {
        if (overlay_mode_enabled && overlay_source_pane_count <= 1) {
            overlay_mode_enabled = false;
        }
        const active_ordinal = source_ordinal_for_entry_id(active_entry_id);
        if (active_ordinal >= 0) {
            active_overlay_source_index = active_ordinal;
        }
        normalize_overlay_selection();
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
    onLayout_pane_countChanged: Qt.callLater(interaction_controller.refresh_best_fit)
    onWidthChanged: Qt.callLater(interaction_controller.refresh_best_fit)
    onHeightChanged: Qt.callLater(interaction_controller.refresh_best_fit)

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
                required property var model

                title: model.title
                subtitle: model.subtitle
                entry_id: model.entryId
                source_path: model.sourcePath
                input_image: model.heatmapImage
                is_source: model.isSource
                apply_color_profile: root.controller.workspace.apply_color_profile
                active: root.active_entry_id === entry_id
                focused: root.focused_entry_id === entry_id
                can_move: !root.overlay_mode_enabled
                visible: root.is_pane_visible_in_layout(is_source, entry_id)
                width: (pane_grid.width - (pane_grid.columns - 1) * pane_grid.spacing) / pane_grid.columns
                height: (pane_grid.height - (root.row_count - 1) * pane_grid.spacing) / root.row_count
                clip: true
                onRemove_requested: entry_id => root.remove_entry(entry_id)
                onSnapshot_changed: (entry_id, image) => root.model.set_source_image(entry_id, image)
                onActivate_requested: entry_id => root.activate_entry(entry_id)
                onCopy_path_requested: path => root.controller.copy_path_to_clipboard(path)
                onOpen_folder_requested: path => root.controller.open_containing_folder(path)
                onMove_requested: (entry_id, direction) => root.move_entry(entry_id, direction)
                onExport_heatmap_requested: entry_id => root.controller.export_heatmap_by_id(entry_id)
                onFocus_toggle_requested: entry_id => {
                    root.activate_entry(entry_id);
                    root.toggle_focus_active_pane();
                }
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
