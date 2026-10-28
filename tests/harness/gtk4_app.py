#!/usr/bin/env python3
import sys

import gi

gi.require_version("Gtk", "4.0")
from gi.repository import GLib, Gtk  # noqa: E402

TEXT = "alpha beta gamma"


def build(app, widget_kind):
    win = Gtk.ApplicationWindow(application=app, title="tif-gtk4")
    if widget_kind == "entry":
        field = Gtk.Entry()
        field.set_text(TEXT)
    else:
        field = Gtk.TextView()
        buf = field.get_buffer()
        buf.set_text(TEXT)
        buf.place_cursor(buf.get_start_iter())
    win.set_child(field)
    win.present()

    def focus_at_start():
        if widget_kind == "entry":
            field.grab_focus_without_selecting()
            field.set_position(0)
        else:
            field.grab_focus()
        return GLib.SOURCE_REMOVE

    GLib.idle_add(focus_at_start)


def main():
    widget_kind = sys.argv[1] if len(sys.argv) > 1 else "entry"
    GLib.set_prgname("tif-gtk4")
    app = Gtk.Application(application_id="org.libtextinputflow.SpikeGtk4")
    app.connect("activate", lambda a: build(a, widget_kind))
    app.run([])


if __name__ == "__main__":
    main()
