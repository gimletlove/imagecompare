import QtQuick
import QtQuick.Controls
import "components"

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 800
    title: "Image Compare " + Qt.application.version

    DropArea {
        id: drop_area
        anchors.fill: parent
    }

    ComparisonWorkspace {
        anchors.fill: parent
        controller: application_controller
        startup_options: application_startup_options
        repository_url: application_repository_url
    }

    Rectangle {
        anchors.fill: parent
        visible: drop_area.containsDrag
        z: 10
        color: "#33000000"

        Pane {
            anchors.centerIn: parent
            padding: 12

            Label {
                text: "Drop images to import"
            }
        }
    }
}
