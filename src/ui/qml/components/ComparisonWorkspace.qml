import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    focus: true
    property var controller

    Action {
        id: open_action
        shortcut: "O"
        text: "Open (o)"
        onTriggered: {
            if (root.controller) {
                root.controller.open_images_with_native_dialog()
            }
        }
    }

    Action {
        id: heatmap_action
        shortcut: "B"
        text: root.controller && root.controller.heatmap_in_progress ? "Building..." : "Heatmap (b)"
        enabled: root.controller && root.controller.workspace
            ? !root.controller.heatmap_in_progress && root.controller.workspace.can_build_heatmap
            : false
        onTriggered: root.controller.build_heatmap()
    }

    Action {
        id: display_mode_action
        shortcut: "R"
        text: (root.controller && root.controller.display_mode === 0 ? "Raw" : "Faithful") + " (r)"
        enabled: panes_grid.can_best_fit_action
        onTriggered: {
            if (root.controller.display_mode === 0) {
                root.controller.set_display_mode_faithful()
            } else {
                root.controller.set_display_mode_strict_raw()
            }
        }
    }

    Action {
        id: zoom_fit_action
        shortcut: "F"
        text: panes_grid.zoom_readout + " (f)"
        enabled: panes_grid.can_best_fit_action
        onTriggered: panes_grid.toggle_zoom_fit()
    }

    Action {
        id: overlay_action
        shortcut: "V"
        text: "Stack (v)"
        enabled: panes_grid.can_overlay_action
        checkable: true
        checked: panes_grid.overlay_mode_enabled
        onTriggered: panes_grid.toggle_overlay_mode()
    }

    Action {
        id: match_zoom_action
        shortcut: "H"
        text: "Match Zoom (h)"
        enabled: panes_grid.can_match_zoom_action
        checkable: true
        checked: panes_grid.match_zoom_enabled
        onTriggered: {
            panes_grid.match_zoom_enabled = !panes_grid.match_zoom_enabled
            if (panes_grid.match_zoom_enabled) {
                panes_grid.reconcile_match_zoom_now()
            } else {
                panes_grid.restore_numeric_zoom_now()
            }
        }
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
        onTriggered: panes_grid.cycle_overlay_pane(1)
    }

    Action {
        id: cycle_backward_action
        shortcut: "Left"
        enabled: panes_grid.overlay_mode_enabled
        onTriggered: panes_grid.cycle_overlay_pane(-1)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 1

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 34

            RowLayout {
                anchors.fill: parent
                spacing: 4

                ToolButton {
                    action: open_action
                    ToolTip.visible: hovered
                    ToolTip.text: "Import images into the workspace"
                    focusPolicy: Qt.NoFocus
                }

                Item {
                    Layout.fillWidth: true
                }

                ToolButton {
                    action: heatmap_action
                    ToolTip.visible: hovered
                    ToolTip.text: "Build a heatmap of the level of difference from two images"
                    focusPolicy: Qt.NoFocus
                }

                ToolButton {
                    action: display_mode_action
                    ToolTip.visible: hovered
                    ToolTip.text: "Faithful uses color profile metadata; Raw ignores color profile metadata"
                    focusPolicy: Qt.NoFocus
                }

                ToolButton {
                    action: zoom_fit_action
                    ToolTip.visible: hovered
                    ToolTip.text: "Toggle best fit and 100 percent zoom (F)"
                    focusPolicy: Qt.NoFocus
                }

                ToolButton {
                    action: overlay_action
                    ToolTip.visible: hovered
                    ToolTip.text: "Stack image panes and cycle with arrow keys"
                    focusPolicy: Qt.NoFocus
                }

                ToolButton {
                    action: match_zoom_action
                    ToolTip.visible: hovered
                    ToolTip.text: "Normalize zoom and pan across different image sizes"
                    focusPolicy: Qt.NoFocus
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
                Qt.callLater(() => panes_grid.set_best_fit())
            }
        }
    }

}
