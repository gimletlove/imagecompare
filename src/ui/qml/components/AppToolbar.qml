import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string zoom_readout: "100%"
    property bool overlay_mode_active: false
    property bool match_zoom_enabled: false
    property bool can_best_fit_action: false
    property bool can_display_mode_action: false
    property bool can_heatmap_action: false
    property bool can_overlay_action: false
    property bool can_match_zoom_action: false
    property bool heatmap_in_progress: false
    property string display_mode_toggle_text: "Faithful"
    signal open_requested()
    signal heatmap_requested()
    signal zoom_fit_toggle_requested()
    signal overlay_mode_toggle_requested()
    signal match_zoom_toggle_requested()
    signal display_mode_toggle_requested()

    RowLayout {
        anchors.fill: parent
        spacing: 4

        ToolButton {
            text: "Open (o)"
            property string tool_tip_text: "Import images into the workspace"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
            onClicked: root.open_requested()
        }

        Item {
            Layout.fillWidth: true
        }

        ToolButton {
            text: root.heatmap_in_progress ? "Building..." : "Heatmap (b)"
            enabled: !root.heatmap_in_progress && root.can_heatmap_action
            property string tool_tip_text: "Build a heatmap of the level of difference from two images"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
            onClicked: root.heatmap_requested()
        }

        ToolButton {
            text: root.display_mode_toggle_text + " (r)"
            enabled: root.can_display_mode_action
            property string tool_tip_text: "Faithful uses color profile metadata; Raw ignores color profile metadata"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
            onClicked: root.display_mode_toggle_requested()
        }

        ToolButton {
            text: root.zoom_readout + " (f)"
            enabled: root.can_best_fit_action
            property string tool_tip_text: "Toggle best fit and 100 percent zoom (F)"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
            onClicked: root.zoom_fit_toggle_requested()
        }

        ToolButton {
            text: "Stack (v)"
            checkable: true
            checked: root.overlay_mode_active
            enabled: root.can_overlay_action
            property string tool_tip_text: "Stack image panes and cycle with arrow keys"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
            onClicked: root.overlay_mode_toggle_requested()
        }

        ToolButton {
            text: "Match Zoom (h)"
            checkable: true
            checked: root.match_zoom_enabled
            enabled: root.can_match_zoom_action
            property string tool_tip_text: "Normalize zoom and pan across different image sizes"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
            onClicked: root.match_zoom_toggle_requested()
        }

    }
}
