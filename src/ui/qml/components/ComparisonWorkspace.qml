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
            return true;
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
            return true;
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
            return true;
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
            return true;
        }
        panes_grid.set_best_fit();
        return true;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 34

            AppToolbar {
                anchors.fill: parent
                workspace: root.controller ? root.controller.workspace : null
                zoom_readout: panes_grid.zoom_readout
                overlay_mode_active: panes_grid.overlay_mode_enabled
                match_zoom_enabled: panes_grid.match_zoom_enabled
                can_best_fit_action: panes_grid.can_best_fit_action
                can_display_mode_action: panes_grid.can_best_fit_action
                can_overlay_action: panes_grid.can_overlay_action
                can_match_zoom_action: panes_grid.can_match_zoom_action
                display_mode_toggle_text: root.controller && root.controller.display_mode === 0 ? "Raw" : "Faithful"
                onOpen_requested: root.open_requested()
                onCompare_tools_requested: {
                    if (root.controller && root.controller.workspace && root.controller.workspace.can_build_heatmap) {
                        root.controller.build_heatmap()
                    }
                }
                onZoom100_requested: {
                    if (!panes_grid.can_best_fit_action) {
                        return;
                    }
                    panes_grid.set_zoom100();
                }
                onBest_fit_requested: {
                    root.apply_best_fit();
                }
                onOverlay_mode_toggle_requested: {
                    root.toggle_overlay_mode();
                }
                onMatch_zoom_toggle_requested: {
                    root.toggle_match_zoom();
                }
                onDisplay_mode_toggle_requested: {
                    root.toggle_display_mode();
                }
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
                visible: panes_grid.visible_pane_count === 0

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

    Shortcut {
        sequence: "O"
        context: Qt.ApplicationShortcut
        onActivated: root.open_requested()
    }

    Shortcut {
        sequence: "B"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root.controller && root.controller.workspace && root.controller.workspace.can_build_heatmap) {
                root.controller.build_heatmap()
            }
        }
    }

    Shortcut {
        sequence: "R"
        context: Qt.ApplicationShortcut
        onActivated: root.toggle_display_mode()
    }

    Shortcut {
        sequence: "T"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!panes_grid.can_best_fit_action) {
                return;
            }
            panes_grid.set_zoom100();
        }
    }

    Shortcut {
        sequence: "F"
        context: Qt.ApplicationShortcut
        onActivated: root.apply_best_fit()
    }

    Shortcut {
        sequence: "V"
        context: Qt.ApplicationShortcut
        onActivated: root.toggle_overlay_mode()
    }

    Shortcut {
        sequence: "H"
        context: Qt.ApplicationShortcut
        onActivated: root.toggle_match_zoom()
    }

    Shortcut {
        sequence: "Right"
        context: Qt.ApplicationShortcut
        onActivated: root.cycle_overlay_forward()
    }

    Shortcut {
        sequence: "Left"
        context: Qt.ApplicationShortcut
        onActivated: root.cycle_overlay_backward()
    }
}
