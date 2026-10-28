#!/usr/bin/env python3
import os
import sys

from PyQt6.QtCore import Qt
from PyQt6.QtGui import QKeySequence, QShortcut, QTextCursor
from PyQt6.QtWidgets import QApplication, QLineEdit, QPlainTextEdit

TEXT = "alpha beta gamma"
SHORTCUT_CARET = 3


def main():
    widget_kind = sys.argv[1] if len(sys.argv) > 1 else "entry"
    app = QApplication(["tif-qt"])
    app.setApplicationName("tif-qt")
    if widget_kind == "entry":
        field = QLineEdit(TEXT)
        field.setCursorPosition(0)

        def move_caret():
            field.setCursorPosition(SHORTCUT_CARET)
    else:
        field = QPlainTextEdit(TEXT)
        field.moveCursor(QTextCursor.MoveOperation.Start)

        def move_caret():
            cursor = field.textCursor()
            cursor.setPosition(SHORTCUT_CARET)
            field.setTextCursor(cursor)

    field.setReadOnly(os.environ.get("TIF_QT_READ_ONLY") == "1")

    for keys in ("Alt+Right", "Ctrl+Right", "Alt+Backspace", "Shift+Ctrl+End", "Ctrl+Delete"):
        shortcut = QShortcut(QKeySequence(keys), field)
        shortcut.setContext(Qt.ShortcutContext.ApplicationShortcut)
        shortcut.activated.connect(move_caret)

    field.setWindowTitle("tif-qt")
    field.show()
    field.setFocus()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
