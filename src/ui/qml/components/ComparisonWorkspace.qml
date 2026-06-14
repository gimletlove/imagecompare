import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    focus: true
    property var controller
    signal open_requested()

    function toggle_display_mode() {
        if (!root.controller) {
            return false;
        }
        if (!panes_grid.can_best_fit_action) {
            return false;
        }
        if (root.controller.display_mode === 0) {
            root.controller.set_display_mode_faithful();
            return true;
        }
        root.controller.set_display_mode_strict_raw();
        return true;
    }

    function toggle_overlay_mode() {
        if (!panes_grid.can_overlay_action) {
            return false;
        }
        return panes_grid.toggle_overlay_mode();
    }

    function cycle_overlay_forward() {
        return panes_grid.cycle_overlay_pane(1);
    }

    function cycle_overlay_backward() {
        return panes_grid.cycle_overlay_pane(-1);
    }

    function toggle_match_zoom() {
        if (!panes_grid.can_match_zoom_action) {
            return false;
        }
        panes_grid.match_zoom_enabled = !panes_grid.match_zoom_enabled;
        if (panes_grid.match_zoom_enabled) {
            panes_grid.reconcile_match_zoom_now();
        } else {
            panes_grid.restore_numeric_zoom_now();
        }
        return panes_grid.match_zoom_enabled;
    }

    function apply_best_fit() {
        if (!panes_grid.can_best_fit_action) {
            return false;
        }
        panes_grid.set_best_fit();
        return true;
    }

    function toggle_zoom_fit() {
        if (!panes_grid.can_best_fit_action) {
            return false;
        }
        return panes_grid.toggle_zoom_fit();
    }

    function build_heatmap() {
        if (!root.controller || !root.controller.workspace || !root.controller.workspace.can_build_heatmap) {
            return false;
        }
        root.controller.build_heatmap();
        return true;
    }

    Action {
        id: open_action
        shortcut: "O"
        text: "Open (o)"
        onTriggered: root.open_requested()
    }

    Action {
        id: heatmap_action
        shortcut: "B"
        text: root.controller && root.controller.heatmap_in_progress ? "Building..." : "Heatmap (b)"
        enabled: root.controller && root.controller.workspace
            ? !root.controller.heatmap_in_progress && root.controller.workspace.can_build_heatmap
            : false
        onTriggered: root.build_heatmap()
    }

    Action {
        id: display_mode_action
        shortcut: "R"
        text: (root.controller && root.controller.display_mode === 0 ? "Raw" : "Faithful") + " (r)"
        enabled: panes_grid.can_best_fit_action
        onTriggered: root.toggle_display_mode()
    }

    Action {
        id: zoom_fit_action
        shortcut: "F"
        text: panes_grid.zoom_readout + " (f)"
        enabled: panes_grid.can_best_fit_action
        onTriggered: root.toggle_zoom_fit()
    }

    Action {
        id: overlay_action
        shortcut: "V"
        text: "Stack (v)"
        enabled: panes_grid.can_overlay_action
        onTriggered: root.toggle_overlay_mode()
    }

    Action {
        id: match_zoom_action
        shortcut: "H"
        text: "Match Zoom (h)"
        enabled: panes_grid.can_match_zoom_action
        onTriggered: root.toggle_match_zoom()
    }

    Action {
        id: copy_action
        shortcut: "Ctrl+C"
        onTriggered: panes_grid.copy_active_path()
    }

    Action {
        id: remove_action
        shortcut: "Ctrl+W"
        onTriggered: panes_grid.remove_active_entry()
    }

    Action {
        id: move_left_action
        shortcut: "Ctrl+Left"
        onTriggered: panes_grid.move_active_pane(-1)
    }

    Action {
        id: move_right_action
        shortcut: "Ctrl+Right"
        onTriggered: panes_grid.move_active_pane(1)
    }

    Action {
        id: focus_action
        shortcut: "Return"
        onTriggered: panes_grid.toggle_focus_active_pane()
    }

    Action {
        id: enter_focus_action
        shortcut: "Enter"
        onTriggered: focus_action.trigger()
    }

    Action {
        id: clear_focus_action
        shortcut: "Escape"
        onTriggered: panes_grid.clear_focus()
    }

    Action {
        id: cycle_forward_action
        shortcut: "Right"
        enabled: panes_grid.overlay_mode_enabled
        onTriggered: root.cycle_overlay_forward()
    }

    Action {
        id: cycle_backward_action
        shortcut: "Left"
        enabled: panes_grid.overlay_mode_enabled
        onTriggered: root.cycle_overlay_backward()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 1

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 34

            AppToolbar {
                anchors.fill: parent
                open_action: open_action
                heatmap_action: heatmap_action
                display_mode_action: display_mode_action
                zoom_fit_action: zoom_fit_action
                overlay_action: overlay_action
                match_zoom_action: match_zoom_action
                overlay_mode_active: panes_grid.overlay_mode_enabled
                match_zoom_enabled: panes_grid.match_zoom_enabled
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            PanesGrid {
                id: panes_grid
                anchors.fill: parent
                model: root.controller ? root.controller.workspace.entries_model : null
                controller: root.controller
            }

            Pane {
                anchors.centerIn: parent
                visible: panes_grid.pane_count === 0

                Label {
                    text: "Drop images here or click Open"
                }
            }
        }
    }

    Connections {
        target: root.controller && root.controller.workspace ? root.controller.workspace : null
        function onEntry_count_changed() {
            if (root.controller && root.controller.workspace && root.controller.workspace.entry_count > 0) {
                Qt.callLater(root.apply_best_fit);
            }
        }
    }

}
