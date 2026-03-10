import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string primary_text: ""
    property string secondary_text: ""
    property int max_primary_chars: 40
    readonly property bool has_secondary_in_display: String(root.secondary_text).trim().length > 0
    readonly property string full_header_tool_tip_text: has_secondary_in_display
        ? String(root.primary_text) + " • " + String(root.secondary_text).trim()
        : String(root.primary_text)
    readonly property string display_primary_text: {
        const raw_primary = String(root.primary_text);
        if (raw_primary.length <= root.max_primary_chars) {
            return raw_primary;
        }
        if (root.max_primary_chars <= 4) {
            return raw_primary.slice(0, root.max_primary_chars);
        }
        const dot_index = raw_primary.lastIndexOf(".");
        if (dot_index <= 0 || dot_index >= raw_primary.length - 1) {
            return raw_primary.slice(0, root.max_primary_chars - 3) + "...";
        }

        const extension = raw_primary.slice(dot_index);
        const suffix_budget = root.max_primary_chars - 3;
        if (extension.length >= suffix_budget) {
            return "..." + extension.slice(extension.length - suffix_budget);
        }

        const prefix_length = root.max_primary_chars - 3 - extension.length;
        return raw_primary.slice(0, prefix_length) + "..." + extension;
    }
    readonly property string display_text: has_secondary_in_display
        ? display_primary_text + " • " + String(root.secondary_text).trim()
        : display_primary_text
    signal remove_requested()

    implicitHeight: 28

    RowLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            text: root.display_text
            elide: Text.ElideRight
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
            onClicked: root.remove_requested()
        }
    }
}
