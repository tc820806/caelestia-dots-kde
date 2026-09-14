/*
    SPDX-FileCopyrightText: 2026 tc820806
    SPDX-License-Identifier: GPL-3.0-or-later

    Live CPU / GPU / RAM / disk usage for the SDDM greeter.

    The greeter runs as the sddm system user, isolated from the logged-in
    user's session and home directory, so there is no live data-source
    framework available here (unlike the KDE lockscreen's Caelestia.Services
    plugin). Instead, ~/.local/bin/caelestia-sddm-resources samples the
    system on a user systemd timer and writes a small JSON snapshot to
    /tmp/caelestia-sddm-resources.json (world-readable), which this card
    re-reads on its own timer via the QML_XHR_ALLOW_FILE_READ override
    already enabled for this theme.
*/

import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property int cpu: -1
    property var cpuTemp: null
    property var gpu: null
    property var gpuTemp: null
    property int ram: -1
    property int disk: -1

    readonly property color clWarn: "#f2b13d"

    FontLoader {
        id: gsfFont
        source: "../assets/google-sans-flex/GoogleSansFlex.ttf"
    }
    readonly property string fontFamily: gsfFont.name.length > 0 ? gsfFont.name : "Google Sans Flex"

    Timer {
        interval: 3000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: loader.reload()
    }

    Item {
        id: loader

        function reload() {
            const xhr = new XMLHttpRequest();
            // Cache-bust: file:// XHR responses can otherwise be reused by Qt's network cache.
            xhr.open("GET", "file:///tmp/caelestia-sddm-resources.json?t=" + Date.now());
            xhr.onreadystatechange = function () {
                if (xhr.readyState !== XMLHttpRequest.DONE) return;
                try {
                    const data = JSON.parse(xhr.responseText);
                    root.cpu = typeof data.cpu === "number" ? data.cpu : -1;
                    root.cpuTemp = typeof data.cpuTemp === "number" ? data.cpuTemp : null;
                    root.gpu = typeof data.gpu === "number" ? data.gpu : null;
                    root.gpuTemp = typeof data.gpuTemp === "number" ? data.gpuTemp : null;
                    root.ram = typeof data.ram === "number" ? data.ram : -1;
                    root.disk = typeof data.disk === "number" ? data.disk : -1;
                } catch (e) {
                    // Sampler hasn't written a first snapshot yet, or isn't installed; leave stale/placeholder values.
                }
            };
            xhr.send();
        }
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: 16
        columns: 3
        columnSpacing: 14
        rowSpacing: 14

        Gauge {
            Layout.fillWidth: true
            Layout.fillHeight: true
            label: "CPU"
            value: root.cpu
            unit: "%"
            ringColor: config.primary
        }

        Gauge {
            Layout.fillWidth: true
            Layout.fillHeight: true
            label: "GPU"
            value: root.gpu ?? -1
            unit: "%"
            ringColor: config.tertiary
            hasData: root.gpu !== null
        }

        Gauge {
            Layout.fillWidth: true
            Layout.fillHeight: true
            label: "RAM"
            value: root.ram
            unit: "%"
            ringColor: config.secondary
        }

        Gauge {
            Layout.fillWidth: true
            Layout.fillHeight: true
            label: "CPU TEMP"
            value: root.cpuTemp ?? -1
            unit: "°C"
            maxValue: 100
            ringColor: (root.cpuTemp ?? 0) >= 90 ? config.error : root.clWarn
            hasData: root.cpuTemp !== null
        }

        Gauge {
            Layout.fillWidth: true
            Layout.fillHeight: true
            label: "GPU TEMP"
            value: root.gpuTemp ?? -1
            unit: "°C"
            maxValue: 100
            ringColor: (root.gpuTemp ?? 0) >= 90 ? config.error : root.clWarn
            hasData: root.gpuTemp !== null
        }

        Gauge {
            Layout.fillWidth: true
            Layout.fillHeight: true
            label: "DISK"
            value: root.disk
            unit: "%"
            ringColor: config.secondary
        }
    }

    component Gauge: Item {
        id: gauge

        property string label: ""
        property real value: -1
        property real maxValue: 100
        property string unit: "%"
        property color ringColor: config.primary
        property bool hasData: true

        readonly property real fraction: gauge.value < 0 ? 0 : Math.max(0, Math.min(1, gauge.value / gauge.maxValue))

        Canvas {
            id: canvas
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            width: Math.min(parent.width, parent.height - labelText.implicitHeight - 6)
            height: width

            readonly property real strokeW: Math.max(4, width * 0.09)

            Connections {
                target: gauge
                function onFractionChanged() { canvas.requestPaint(); }
                function onRingColorChanged() { canvas.requestPaint(); }
                function onHasDataChanged() { canvas.requestPaint(); }
            }

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const cx = width / 2;
                const cy = height / 2;
                const r = width / 2 - strokeW / 2 - 2;

                ctx.lineWidth = strokeW;
                ctx.lineCap = "round";

                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.12);
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, Math.PI * 2);
                ctx.stroke();

                if (gauge.hasData && gauge.fraction > 0) {
                    ctx.strokeStyle = gauge.ringColor;
                    const start = -Math.PI / 2;
                    const end = start + Math.PI * 2 * gauge.fraction;
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, start, end);
                    ctx.stroke();
                }
            }

            Text {
                anchors.centerIn: parent
                text: gauge.hasData && gauge.value >= 0 ? Math.round(gauge.value) + gauge.unit : "--"
                color: config.text
                font.family: root.fontFamily
                font.pointSize: Math.max(9, Math.round(canvas.width * 0.16))
                font.bold: true
            }
        }

        Text {
            id: labelText
            anchors.top: canvas.bottom
            anchors.topMargin: 6
            anchors.horizontalCenter: parent.horizontalCenter
            text: gauge.label
            color: config.textDark
            opacity: 0.75
            font.family: root.fontFamily
            font.pointSize: 9
            font.letterSpacing: 1
        }
    }
}
