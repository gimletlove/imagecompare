import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string primary_text: ""
    property string secondary_text: ""
    readonly property bool has_secondary_in_display: String(root.secondary_text).trim().length > 0
    readonly property string full_header_tool_tip_text: has_secondary_in_display
        ? String(root.primary_text) + " • " + String(root.secondary_text).trim()
        : String(root.primary_text)
    signal remove_requested()

    implicitHeight: 28

    RowLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            text: root.full_header_tool_tip_text
            elide: Text.ElideMiddle
            HoverHandler {
                id: primary_label_hover
            }
            ToolTip.visible: primary_label_hover.hovered
            ToolTip.text: root.full_header_tool_tip_text
            Layout.fillWidth: true
        }

        ToolButton {
            text: "x"
            implicitWidth: 22
            implicitHeight: 22
            focusPolicy: Qt.NoFocus
            ToolTip.visible: hovered
            ToolTip.text: "Remove image"
            onClicked: root.remove_requested()
        }
    }
}
