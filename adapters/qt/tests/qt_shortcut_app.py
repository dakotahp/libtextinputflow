#!/usr/bin/env python3
import sys

from PyQt6.QtCore import Qt
from PyQt6.QtGui import QKeySequence, QShortcut, QTextCursor
from PyQt6.QtWidgets import QApplication, QLineEdit, QPlainTextEdit

TEXT = "alpha beta gamma"


def main():
    widget_kind = sys.argv[1] if len(sys.argv) > 1 else "entry"
    app = QApplication(["tif-qt"])
    app.setApplicationName("tif-qt")
    if widget_kind == "entry":
        field = QLineEdit(TEXT)
        field.setCursorPosition(0)

        def move_to_end():
            field.end(False)
    else:
        field = QPlainTextEdit(TEXT)
        field.moveCursor(QTextCursor.MoveOperation.Start)

        def move_to_end():
            field.moveCursor(QTextCursor.MoveOperation.End)

    for keys in ("Alt+Right", "Ctrl+Right"):
        shortcut = QShortcut(QKeySequence(keys), field)
        shortcut.setContext(Qt.ShortcutContext.ApplicationShortcut)
        shortcut.activated.connect(move_to_end)

    field.setWindowTitle("tif-qt")
    field.show()
    field.setFocus()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
