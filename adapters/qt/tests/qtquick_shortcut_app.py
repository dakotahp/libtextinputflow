#!/usr/bin/env python3
import os
import sys

from PyQt6.QtCore import QByteArray
from PyQt6.QtGui import QGuiApplication
from PyQt6.QtQml import QQmlApplicationEngine

TEXT = "alpha beta gamma"
SHORTCUT_CARET = 3

QML = """
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    width: 400
    height: 200
    title: "tif-qtquick"

    %(type)s {
        id: field
        anchors.fill: parent
        text: "%(text)s"
        readOnly: %(read_only)s
        focus: true
        Component.onCompleted: cursorPosition = 0
    }

    Shortcut {
        sequences: ["Alt+Right", "Ctrl+Right", "Alt+Backspace", "Shift+Ctrl+End", "Ctrl+Delete"]
        context: Qt.ApplicationShortcut
        onActivated: field.cursorPosition = %(caret)d
    }
}
"""


def main():
    widget_kind = sys.argv[1] if len(sys.argv) > 1 else "entry"
    app = QGuiApplication(["tif-qtquick"])
    app.setApplicationName("tif-qtquick")
    engine = QQmlApplicationEngine()
    qml = QML % {
        "type": "TextField" if widget_kind == "entry" else "TextArea",
        "text": TEXT,
        "read_only": "true" if os.environ.get("TIF_QT_READ_ONLY") == "1" else "false",
        "caret": SHORTCUT_CARET,
    }
    engine.loadData(QByteArray(qml.encode()))
    if not engine.rootObjects():
        sys.exit(1)
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
