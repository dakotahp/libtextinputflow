#!/usr/bin/env python3
import sys

from PyQt6.QtCore import QByteArray
from PyQt6.QtGui import QGuiApplication
from PyQt6.QtQml import QQmlApplicationEngine

TEXT = "alpha beta gamma"

QML = """
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    width: 400
    height: 200
    title: "tif-qtquick"

    %(type)s {
        anchors.fill: parent
        text: "%(text)s"
        focus: true
        Component.onCompleted: cursorPosition = 0
    }
}
"""


def main():
    widget_kind = sys.argv[1] if len(sys.argv) > 1 else "entry"
    app = QGuiApplication(["tif-qtquick"])
    app.setApplicationName("tif-qtquick")
    engine = QQmlApplicationEngine()
    field_type = "TextField" if widget_kind == "entry" else "TextArea"
    engine.loadData(QByteArray((QML % {"type": field_type, "text": TEXT}).encode()))
    if not engine.rootObjects():
        sys.exit(1)
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
