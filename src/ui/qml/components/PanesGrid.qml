import QtQuick
import ImageCompare 1.0

Item {
    id: root
    property var model
    property var controller
    property bool overlay_mode_enabled: false
    property int active_overlay_source_index: 0
    property alias shared_zoom_factor: interaction_controller.shared_zoom_factor
    property alias shared_image_center: interaction_controller.shared_image_center
    property alias shared_best_fit_active: interaction_controller.shared_best_fit_active
    property alias match_zoom_enabled: interaction_controller.match_zoom_enabled
    readonly property string zoom_readout: interaction_controller.zoom_readout
    readonly property int visible_pane_count: pane_repeater.count
    readonly property int overlay_source_pane_count: source_pane_count()
    readonly property bool can_best_fit_action: visible_pane_count > 0
    readonly property bool can_overlay_action: overlay_source_pane_count > 1
    readonly property bool all_pane_images_same_resolution: panes_share_resolution()
    readonly property bool can_match_zoom_action: visible_pane_count > 1 && !all_pane_images_same_resolution
    readonly property int layout_pane_count: effective_pane_count()
    readonly property int column_count: layout_pane_count <= 1 ? 1 : (layout_pane_count === 2 ? 2 : (layout_pane_count === 3 ? 3 : 2))
    readonly property int row_count: layout_pane_count === 0 ? 1 : Math.ceil(layout_pane_count / column_count)

    function source_pane_count() {
        let source_count = 0;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (pane && pane.source_pane_value) {
                source_count += 1;
            }
        }
        return source_count;
    }

    function panes_share_resolution() {
        let baseline_width = 0.0;
        let baseline_height = 0.0;
        let comparable_panes = 0;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (!pane) {
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

    function non_source_pane_count() {
        let count = 0;
        for (let index = 0; index < pane_repeater.count; ++index) {
            const pane = pane_repeater.itemAt(index);
            if (pane && !pane.source_pane_value) {
                count += 1;
            }
        }
        return count;
    }

    function effective_pane_count() {
        if (!overlay_mode_enabled || overlay_source_pane_count <= 1) {
            return visible_pane_count;
        }
        return non_source_pane_count() + 1;
    }

    function source_ordinal_for_repeater_index(repeater_index) {
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

    function is_pane_visible_in_layout(repeater_index, source_pane) {
        if (!overlay_mode_enabled || !source_pane || overlay_source_pane_count <= 1) {
            return true;
        }
        return source_ordinal_for_repeater_index(repeater_index) === active_overlay_source_index;
    }

    function normalize_overlay_selection() {
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

    function toggle_overlay_mode() {
        if (overlay_source_pane_count <= 1) {
            overlay_mode_enabled = false;
            return false;
        }
        overlay_mode_enabled = !overlay_mode_enabled;
        normalize_overlay_selection();
        return true;
    }

    function cycle_overlay_pane(step) {
        if (!overlay_mode_enabled || overlay_source_pane_count <= 1) {
            return false;
        }
        const direction = Number(step) >= 0 ? 1 : -1;
        active_overlay_source_index = active_overlay_source_index + direction;
        normalize_overlay_selection();
        return true;
    }

    function reconcile_match_zoom_now() {
        return interaction_controller.reconcile_match_zoom_now();
    }

    function restore_numeric_zoom_now() {
        return interaction_controller.restore_numeric_zoom_now();
    }

    function remove_entry(entry_id) {
        if (!root.controller || !entry_id) {
            return false;
        }
        return root.controller.remove_workspace_entry_by_id(String(entry_id));
    }

    function set_zoom100() {
        return interaction_controller.set_zoom100();
    }

    function set_best_fit() {
        return interaction_controller.set_best_fit();
    }

    onVisible_pane_countChanged: {
        if (overlay_mode_enabled && overlay_source_pane_count <= 1) {
            overlay_mode_enabled = false;
        }
        normalize_overlay_selection();
        if (visible_pane_count > 0) {
            shared_best_fit_active = true;
            interaction_controller.schedule_best_fit_refresh();
        }
    }
    onCan_match_zoom_actionChanged: {
        if (!can_match_zoom_action && match_zoom_enabled) {
            match_zoom_enabled = false;
            restore_numeric_zoom_now();
        }
    }
    onOverlay_mode_enabledChanged: normalize_overlay_selection()
    onWidthChanged: interaction_controller.schedule_best_fit_refresh()
    onHeightChanged: interaction_controller.schedule_best_fit_refresh()

    PaneGridInteractionController {
        id: interaction_controller
        pane_repeater: pane_repeater
    }

    Grid {
        id: pane_grid
        anchors.fill: parent
        anchors.margins: 2
        columns: root.column_count
        spacing: 4

        Repeater {
            id: pane_repeater
            model: root.model

            delegate: ComparisonPaneItem {
                primary_header_text: primaryHeader
                secondary_header_text: secondaryHeader
                entry_id_value: String(entryId)
                image_path_value: String(imagePath)
                source_pane_value: Boolean(isSource)
                display_mode_value: root.controller ? root.controller.display_mode : 1
                shared_zoom_factor: root.shared_zoom_factor
                visible: root.is_pane_visible_in_layout(index, source_pane_value)
                width: (pane_grid.width - (pane_grid.columns - 1) * pane_grid.spacing) / pane_grid.columns
                height: (pane_grid.height - (root.row_count - 1) * pane_grid.spacing) / root.row_count
                clip: true
                onRemove_requested: entry_id => root.remove_entry(entry_id)
                onView_changed: (pane, zoom_factor, image_center) =>
                    interaction_controller.update_shared_from_pane(pane, zoom_factor, image_center)
            }
        }
    }
}
