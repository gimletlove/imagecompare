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
        text: (root.controller && root.controller.workspace && root.controller.workspace.display_mode === 0 ? "Raw" : "Faithful") + " (r)"
        enabled: panes_grid.can_best_fit_action
        onTriggered: root.controller.toggle_display_mode()
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
        text: "Relative Sync (h)"
        enabled: panes_grid.can_match_zoom_action
        checkable: true
        checked: panes_grid.can_match_zoom_action && panes_grid.match_zoom_enabled
        onTriggered: {
            panes_grid.match_zoom_enabled = !panes_grid.match_zoom_enabled
            panes_grid.sync_current_view_now()
        }
    }

    Shortcut {
        sequence: "L"
        onActivated: panes_grid.toggle_lock_active_pane()
    }

    Shortcut {
        sequence: "Ctrl+C"
        onActivated: panes_grid.copy_active_path()
    }

    Shortcut {
        sequence: "Ctrl+W"
        onActivated: panes_grid.remove_active_entry()
    }

    Shortcut {
        sequence: "Ctrl+Left"
        onActivated: panes_grid.move_active_pane(-1)
    }

    Shortcut {
        sequence: "Ctrl+Right"
        onActivated: panes_grid.move_active_pane(1)
    }

    Shortcut {
        sequences: ["Return", "Enter"]
        onActivated: panes_grid.toggle_focus_active_pane()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: panes_grid.clear_focus()
    }

    Shortcut {
        sequence: "Right"
        enabled: panes_grid.overlay_mode_enabled
        onActivated: panes_grid.cycle_overlay_pane(1)
    }

    Shortcut {
        sequence: "Left"
        enabled: panes_grid.overlay_mode_enabled
        onActivated: panes_grid.cycle_overlay_pane(-1)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 1

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 32

            RowLayout {
                anchors.fill: parent
                spacing: 4

                ToolButton {
                    action: open_action
                    ToolTip.delay: 300
                    ToolTip.visible: hovered
                    ToolTip.text: "Import images into the workspace"
                    focusPolicy: Qt.NoFocus
                }

                Item {
                    Layout.fillWidth: true
                }

                ToolButton {
                    action: heatmap_action
                    ToolTip.delay: 300
                    ToolTip.visible: hovered
                    ToolTip.text: "Build a heatmap of the level of difference from two images"
                    focusPolicy: Qt.NoFocus
                }

                ToolButton {
                    action: display_mode_action
                    ToolTip.delay: 300
                    ToolTip.visible: hovered
                    ToolTip.text: "Faithful uses color profile metadata; Raw ignores color profile metadata"
                    focusPolicy: Qt.NoFocus
                }

                ToolButton {
                    action: zoom_fit_action
                    ToolTip.delay: 300
                    ToolTip.visible: hovered
                    ToolTip.text: "Toggle best fit and 100 percent zoom (F)"
                    focusPolicy: Qt.NoFocus
                }

                ToolButton {
                    action: overlay_action
                    ToolTip.delay: 300
                    ToolTip.visible: hovered
                    ToolTip.text: "Stack images and cycle with arrow keys"
                    focusPolicy: Qt.NoFocus
                }

                ToolButton {
                    action: match_zoom_action
                    ToolTip.delay: 300
                    ToolTip.visible: hovered
                    ToolTip.text: "Synchronize zoom and pan proportionally across different image resolutions"
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

}
