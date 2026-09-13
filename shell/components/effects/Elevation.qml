import QtQuick
import QtQuick.Effects
import qs.components
import qs.services

// RectangularShadow (QtQuick.Effects) only exists on Qt 6.9+. Distros still on
// Qt 6.8 (e.g. Debian trixie, older Fedora) throw "RectangularShadow is not a
// type" and the whole QML bundle - including the lock screen - fails to load.
// This reimplements the same "level" API on top of MultiEffect's shadow
// support, which has been available since Qt 6.5.
Item {
    id: root

    property int level
    property real dp: [0, 1, 3, 6, 8, 12][level]
    property alias radius: shadowSource.radius

    Behavior on dp {
        Anim {
            type: Anim.SlowEffects
        }
    }

    Rectangle {
        id: shadowSource

        anchors.fill: parent
        color: "black"
    }

    MultiEffect {
        anchors.fill: parent
        source: shadowSource
        autoPaddingEnabled: true

        shadowEnabled: root.level > 0
        shadowColor: root.level === 0 ? "transparent" : Qt.alpha(Colours.palette.m3shadow, 0.7)
        shadowBlur: Math.min(1, root.dp / 12)
        shadowScale: 1 + (-root.dp * 0.3 + (root.dp * 0.1) ** 2) / 100
        shadowHorizontalOffset: 0
        shadowVerticalOffset: root.dp / 2
    }
}
