import QtQuick 2.15
import QtQuick.Controls 2.15

Window {
    visible: true
    width: 720
    height: 1280
    color: "#1a1a1a"
    title: "Lightroom Controller"

    // ── Fixed dimensions ──
    readonly property int margin:       6
    readonly property int btnW:         150
    readonly property int btnH:         60
    readonly property int btnGap:       5
    readonly property int encH:         75
    readonly property int encW:         94
    readonly property int btnFontSize:  14
    readonly property int encLabelSize: 14
    readonly property int encValueSize: 20

    // ── Luminance helper ──
    // Threshold 0.4 — errs on side of white text for mid-range colors
    function isLight(hexColor) {
        if (!hexColor || hexColor.length < 7) return false
        var c = hexColor.replace("#", "")
        if (c.length !== 6) return false
        var r = parseInt(c.substr(0, 2), 16)
        var g = parseInt(c.substr(2, 2), 16)
        var b = parseInt(c.substr(4, 2), 16)
        if (isNaN(r) || isNaN(g) || isNaN(b)) return false
        return (0.299 * r + 0.587 * g + 0.114 * b) / 255 > 0.4
    }

    // ── Text processing — \n and - line breaks ──
    function processLabel(text) {
        if (!text) return ""
        var s = text.replace(/\\n/g, "\n")
        s = s.replace(/-/g, "-\u200B")
        return s
    }

    // ── Rotated content — 800×480 landscape on 720×1280 portrait screen ──
    Item {
        width: 800; height: 480
        x: 0; y: 1240
        transform: [
            Rotation { angle: 270; origin.x: 0; origin.y: 0 },
            Scale { xScale: 1.5; yScale: 1.5; origin.x: 0; origin.y: 0 }
        ]

    // ── Button grid ── anchored top
    Item {
        id: buttonGrid
        anchors.top:        parent.top
        anchors.left:       parent.left
        anchors.topMargin:  margin
        anchors.leftMargin: margin
        width:  5 * btnW + 4 * btnGap
        height: 6 * btnH + 5 * btnGap

        Repeater {
            model: 30
            delegate: Rectangle {
                id: btnRect
                property int btnRow: Math.floor(index / 5)
                property int btnCol: index % 5

                // ── Row classification ──
                // Rows 0-1: config selectors
                // Row 5 col 4: toggle bank (no MIDI)
                // All other cells: action buttons
                property bool isSelector:   btnRow < 2
                property bool isToggleBank: btnRow === configManager.toggleBankRow() &&
                                            btnCol === configManager.toggleBankCol()

                property int selectorIndex: btnRow * 5 + btnCol

                // Action button config from JSON
                property var btnConfig: {
                    if (isSelector || isToggleBank) return null
                    if (!configManager.buttons) return null
                    for (var i = 0; i < configManager.buttons.length; i++) {
                        var b = configManager.buttons[i]
                        if (b.row === btnRow && b.col === btnCol)
                            return b
                    }
                    return null
                }

                property bool hasConfig: {
                    if (isToggleBank) return true
                    if (isSelector)   return selectorIndex < configManager.configs.length
                    return btnConfig !== null
                }

                property bool isActiveSelector: isSelector &&
                    selectorIndex < configManager.configs.length &&
                    configManager.activeConfig === selectorIndex

                property bool isPressed: mouseArea.pressed && hasConfig

                property string resolvedColor: {
                    if (isToggleBank)
                        return configManager.activeBank === 0 ? "#1a4a2a" : "#4a1a2a"
                    if (isSelector) {
                        if (!hasConfig) return "#222222"
                        return isActiveSelector ? "#5599cc" : "#1a5276"
                    }
                    if (!hasConfig) return "#222222"
                    var col = btnConfig.color
                    if (!col || col.length < 4) return "#222222"
                    return col
                }

                property string pressedColor: Qt.lighter(resolvedColor, 1.8)
                property string currentColor: isPressed ? pressedColor : resolvedColor

                x:      btnCol * (btnW + btnGap)
                y:      btnRow * (btnH + btnGap)
                width:  btnW
                height: btnH
                radius: 6
                color:  currentColor

                Behavior on color { ColorAnimation { duration: 80 } }
                scale: isPressed ? 0.95 : 1.0
                Behavior on scale { NumberAnimation { duration: 80 } }

                Text {
                    anchors.centerIn: parent
                    width: btnRect.width - 8

                    text: {
                        if (btnRect.isToggleBank)
                            return configManager.activeBank === 0 ? "Bank A" : "Bank B"
                        if (btnRect.isSelector) {
                            if (!btnRect.hasConfig) return ""
                            var cfg = configManager.configs[btnRect.selectorIndex]
                            return processLabel(cfg ? cfg.name : "")
                        }
                        return processLabel(btnRect.btnConfig ? btnRect.btnConfig.label : "")
                    }

                    color: isLight(btnRect.currentColor) ? "#111111" : "#ffffff"

                    font.pixelSize: btnFontSize
                    font.bold: btnRect.isSelector || btnRect.isToggleBank
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment:   Text.AlignVCenter
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }

                // Active selector dot
                Rectangle {
                    visible: btnRect.isActiveSelector
                    width: 6; height: 6; radius: 3
                    color: isLight(btnRect.resolvedColor) ? "#111111" : "#ffffff"
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottomMargin: 3
                }

                // Bank toggle dot
                Rectangle {
                    visible: btnRect.isToggleBank
                    width: 8; height: 8; radius: 4
                    color: configManager.activeBank === 0 ? "#44ff88" : "#ff4488"
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottomMargin: 3
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    onPressed: {
                        if (btnRect.isToggleBank) {
                            configManager.toggleBank()
                            return
                        }
                        if (btnRect.isSelector) {
                            if (btnRect.selectorIndex < configManager.configs.length) {
                                configManager.selectConfig(btnRect.selectorIndex)
                                midiWorker.sendConfigSelect(btnRect.selectorIndex)
                            }
                            return
                        }
                        if (btnRect.btnConfig === null) return
                        midiWorker.sendNoteOn(
                            btnRect.btnConfig.note,
                            configManager.midiMap
                                ? configManager.midiMap.gridChannel : 10
                        )
                    }
                    onReleased: {
                        if (btnRect.isToggleBank) return
                        if (btnRect.isSelector)   return
                        if (btnRect.btnConfig === null) return
                        midiWorker.sendNoteOff(
                            btnRect.btnConfig.note,
                            configManager.midiMap
                                ? configManager.midiMap.gridChannel : 10
                        )
                    }
                }
            }
        }
    }

    // ── Encoder bar ── anchored bottom
    Item {
        id: encoderBar
        anchors.bottom:       parent.bottom
        anchors.left:         parent.left
        anchors.right:        parent.right
        anchors.bottomMargin: margin
        anchors.leftMargin:   margin
        anchors.rightMargin:  margin
        height: encH

        Repeater {
            model: 8
            delegate: Rectangle {
                id: encRect

                property var encList: configManager.activeBank === 0
                    ? configManager.encodersA
                    : configManager.encodersB

                property var encConfig: {
                    if (!encList) return null
                    for (var i = 0; i < encList.length; i++) {
                        if (encList[i].id === index)
                            return encList[i]
                    }
                    return null
                }

                property string bgColor: {
                    if (!encRect.encConfig) return "#2a2a2a"
                    var c = encRect.encConfig.color
                    if (!c || c.length < 4) return "#2a2a2a"
                    return c
                }

                x:      index * (encW + btnGap)
                y:      0
                width:  encW
                height: encH
                radius: 6
                color:  encRect.bgColor

                // Bank stripe at top
                // Rectangle {
                //     width:  encRect.width - 4
                //     height: 3
                //     radius: 2
                //     color:  configManager.activeBank === 0 ? "#44ff88" : "#ff4488"
                //     anchors.top: parent.top
                //     anchors.topMargin: 2
                //     anchors.horizontalCenter: parent.horizontalCenter
                // }

                Column {
                    anchors.centerIn: parent
                    spacing: 4

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text:  processLabel(
                            encRect.encConfig
                                ? encRect.encConfig.label
                                : "Enc " + index)
                        color: isLight(encRect.bgColor) ? "#333333" : "#aaaaaa"
                        font.pixelSize: encLabelSize
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        width: encW - 8
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: encoderWorker.values[index] !== undefined
                              ? encoderWorker.values[index] : "0"
                        color: isLight(encRect.bgColor) ? "#111111" : "#ffffff"
                        font.pixelSize: encValueSize
                        font.bold: true
                    }
                }
            }
        }
    }

    } // rotated content
}
