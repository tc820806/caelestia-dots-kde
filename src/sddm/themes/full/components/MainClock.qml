import QtQuick

Item {
    id: root

    property bool firstInput
    property real mainCardComponentsOpacity
    property bool ap

    property real centerScale: 1
    property date currentTime: new Date()

    readonly property var fontAxesHours: ({
            "wght": 500,
            "wdth": 30,
            "ROND": 25,
            "opsz": 224 * centerScale
        })
    readonly property var fontAxesMinutes: ({
            "wght": 500,
            "wdth": 30,
            "ROND": 25,
            "opsz": 224 * centerScale
        })

    FontLoader {
        id: googleSansFlex

        source: "../assets/google-sans-flex/GoogleSansFlex.ttf"
    }

    Row {
        id: clock
        anchors.centerIn: parent

        Text {
            id: hourText

            renderType: Text.NativeRendering
            font.family: googleSansFlex.name
            font.variableAxes: root.fontAxesHours
            font.pixelSize: Math.round(224 * root.centerScale)
            color: Qt.lighter(config.primary, 1.6)
            text: {
                if (!root.ap)
                    return Qt.formatTime(root.currentTime, "hh");
                var h = root.currentTime.getHours() % 12;
                if (h === 0) h = 12;
                return h < 10 ? "0" + h : "" + h;
            }
        }

        Item {
            width: 5
            height: 1
        }

        Text {
            id: minuteText

            renderType: Text.NativeRendering
            font.family: googleSansFlex.name
            font.variableAxes: root.fontAxesMinutes
            font.pixelSize: Math.round(224 * root.centerScale)
            color: config.secondary
            text: Qt.formatTime(root.currentTime, "mm")
        }

        Text {
            id: amPmText

            visible: root.ap
            anchors.bottom: minuteText.bottom
            anchors.bottomMargin: Math.round(28 * root.centerScale)
            renderType: Text.NativeRendering
            font.family: googleSansFlex.name
            font.pixelSize: Math.max(14, Math.round(28 * root.centerScale))
            font.bold: true
            color: config.secondary
            text: root.currentTime.getHours() >= 12 ? "PM" : "AM"
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: 300
            easing.type: Easing.OutBack
        }
    }
}
