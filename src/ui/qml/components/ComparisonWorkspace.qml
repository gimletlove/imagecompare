import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    focus: true
    property var controller

    component ToolbarButton: ToolButton {
        property string tip
        focusPolicy: Qt.NoFocus
        ToolTip.delay: 300
        ToolTip.visible: hovered
        ToolTip.text: tip
    }

    Action {
        id: open_action
        shortcut: "O"
        text: "Open (o)"
        onTriggered: root.controller.open_images_with_native_dialog()
    }

    Action {
        id: heatmap_action
        shortcut: "B"
        text: root.controller.heatmap_in_progress ? "Building..." : "Heatmap (b)"
        enabled: !root.controller.heatmap_in_progress && root.controller.workspace.can_build_heatmap
        onTriggered: root.controller.build_heatmap()
    }

    Action {
        id: color_mode_action
        shortcut: "R"
        text: (root.controller.workspace.apply_color_profile ? "Faithful" : "Raw") + " (r)"
        enabled: panes_grid.can_best_fit_action
        onTriggered: root.controller.toggle_color_mode()
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
        enabled: panes_grid.overlay_mode_enabled && panes_grid.focused_entry_id.length === 0
        onActivated: panes_grid.cycle_overlay_pane(1)
    }

    Shortcut {
        sequence: "Left"
        enabled: panes_grid.overlay_mode_enabled && panes_grid.focused_entry_id.length === 0
        onActivated: panes_grid.cycle_overlay_pane(-1)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 1

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            spacing: 4

            ToolbarButton {
                action: open_action
                tip: "Import images into the workspace"
            }

            Item {
                Layout.fillWidth: true
            }

            ToolbarButton {
                action: heatmap_action
                tip: "Build a heatmap of the level of difference from two images"
            }

            ToolbarButton {
                action: color_mode_action
                tip: "Faithful uses color profile metadata; Raw ignores color profile metadata"
            }

            ToolbarButton {
                action: zoom_fit_action
                tip: "Toggle best fit and 100 percent zoom (F)"
            }

            ToolbarButton {
                action: overlay_action
                tip: "Stack images and cycle with arrow keys"
            }

            ToolbarButton {
                action: match_zoom_action
                tip: "Synchronize zoom and pan proportionally across different image resolutions"
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            PanesGrid {
                id: panes_grid
                anchors.fill: parent
                model: root.controller.workspace
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
