#!/usr/bin/env python3
import sys

from PyQt6.QtGui import QTextCursor
from PyQt6.QtWidgets import QApplication, QLineEdit, QPlainTextEdit

TEXT = "alpha beta gamma"


def main():
    widget_kind = sys.argv[1] if len(sys.argv) > 1 else "entry"
    app = QApplication(["tif-qt"])
    app.setApplicationName("tif-qt")
    if widget_kind == "entry":
        field = QLineEdit(TEXT)
        field.setCursorPosition(0)
    else:
        field = QPlainTextEdit(TEXT)
        field.moveCursor(QTextCursor.MoveOperation.Start)
    field.setWindowTitle("tif-qt")
    field.show()
    field.setFocus()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
