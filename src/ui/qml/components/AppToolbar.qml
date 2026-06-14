import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var open_action
    property var heatmap_action
    property var display_mode_action
    property var zoom_fit_action
    property var overlay_action
    property var match_zoom_action
    property bool overlay_mode_active: false
    property bool match_zoom_enabled: false

    RowLayout {
        anchors.fill: parent
        spacing: 4

        ToolButton {
            action: root.open_action
            property string tool_tip_text: "Import images into the workspace"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
        }

        Item {
            Layout.fillWidth: true
        }

        ToolButton {
            action: root.heatmap_action
            property string tool_tip_text: "Build a heatmap of the level of difference from two images"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
        }

        ToolButton {
            action: root.display_mode_action
            property string tool_tip_text: "Faithful uses color profile metadata; Raw ignores color profile metadata"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
        }

        ToolButton {
            action: root.zoom_fit_action
            property string tool_tip_text: "Toggle best fit and 100 percent zoom (F)"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
        }

        ToolButton {
            action: root.overlay_action
            checkable: true
            checked: root.overlay_mode_active
            property string tool_tip_text: "Stack image panes and cycle with arrow keys"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
        }

        ToolButton {
            action: root.match_zoom_action
            checkable: true
            checked: root.match_zoom_enabled
            property string tool_tip_text: "Normalize zoom and pan across different image sizes"
            ToolTip.visible: hovered
            ToolTip.text: tool_tip_text
            focusPolicy: Qt.NoFocus
        }

    }
}
