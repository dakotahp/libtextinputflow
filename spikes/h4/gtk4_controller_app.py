#!/usr/bin/env python3
import signal
import sys

import gi

gi.require_version("Gtk", "4.0")
from gi.repository import Gio, GLib, Gtk  # noqa: E402

TEXT = "alpha beta gamma"

FIRST = [
    ("<Control>Right", Gtk.MovementStep.DISPLAY_LINE_ENDS, 1),
    ("<Alt>Right", Gtk.MovementStep.WORDS, 1),
]
SECOND = [
    ("<Alt>Right", Gtk.MovementStep.DISPLAY_LINE_ENDS, 1),
]


def shortcut(trigger, step, count):
    item = Gtk.Shortcut.new(Gtk.ShortcutTrigger.parse_string(trigger), Gtk.SignalAction.new("move-cursor"))
    item.set_arguments(GLib.Variant("(iib)", (int(step), count, False)))
    return item


def fill(store, bindings):
    store.splice(0, store.get_n_items(), [shortcut(*b) for b in bindings])


def build(app, widget_kind):
    win = Gtk.ApplicationWindow(application=app, title="tif-gtk4")
    if widget_kind == "entry":
        outer = Gtk.Entry()
        outer.set_text(TEXT)
        target = outer.get_delegate()
    else:
        outer = Gtk.TextView()
        buf = outer.get_buffer()
        buf.set_text(TEXT)
        buf.place_cursor(buf.get_start_iter())
        target = outer

    store = Gio.ListStore(item_type=Gtk.Shortcut)
    fill(store, FIRST)
    target.add_controller(Gtk.ShortcutController.new_for_model(store))

    GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGUSR1, lambda: fill(store, SECOND) or True)

    win.set_child(outer)
    win.present()

    def focus_at_start():
        if widget_kind == "entry":
            outer.grab_focus_without_selecting()
            outer.set_position(0)
        else:
            outer.grab_focus()
        return GLib.SOURCE_REMOVE

    GLib.idle_add(focus_at_start)


def main():
    widget_kind = sys.argv[1] if len(sys.argv) > 1 else "entry"
    GLib.set_prgname("tif-gtk4")
    app = Gtk.Application(application_id="org.libtextinputflow.SpikeH4")
    app.connect("activate", lambda a: build(a, widget_kind))
    app.run([])


if __name__ == "__main__":
    main()
