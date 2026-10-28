#!/usr/bin/env python3
import argparse
import json
import sys
import time

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402


def find_app(name):
    desktop = Atspi.get_desktop(0)
    for i in range(desktop.get_child_count()):
        app = desktop.get_child_at_index(i)
        if app is not None and app.get_name() == name:
            return app
    return None


def find_focused_text(node, depth=0):
    if node is None or depth > 40:
        return None
    states = node.get_state_set()
    if states.contains(Atspi.StateType.FOCUSED) and node.get_text_iface() is not None:
        return node
    for i in range(node.get_child_count()):
        found = find_focused_text(node.get_child_at_index(i), depth + 1)
        if found is not None:
            return found
    return None


def dump(node, depth=0):
    if node is None or depth > 40:
        return
    states = node.get_state_set().get_states()
    names = sorted(Atspi.StateType(s).value_nick for s in states)
    has_text = node.get_text_iface() is not None
    print(f"{'  ' * depth}{node.get_role_name()} text={has_text} states={','.join(names)}")
    for i in range(node.get_child_count()):
        dump(node.get_child_at_index(i), depth + 1)


def read(app_name):
    app = find_app(app_name)
    if app is None:
        return {"error": f"app {app_name!r} not on the accessibility bus"}
    field = find_focused_text(app)
    if field is None:
        return {"error": "no focused text field"}
    text = field.get_text_iface()
    # The Python binding resolves text.get_selection to the deprecated Accessible method.
    ranges = [Atspi.Text.get_selection(text, i) for i in range(Atspi.Text.get_n_selections(text))]
    selections = [(r.start_offset, r.end_offset) for r in ranges]
    return {
        "role": field.get_role_name(),
        "caret": text.get_caret_offset(),
        "selection": selections,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("app")
    parser.add_argument("--wait", type=float, default=0, help="seconds to wait for a focused text field")
    parser.add_argument("--dump", action="store_true", help="print the app's accessibility tree")
    args = parser.parse_args()

    Atspi.init()
    if args.dump:
        dump(find_app(args.app))
        return

    deadline = time.monotonic() + args.wait
    result = read(args.app)
    while "error" in result and time.monotonic() < deadline:
        time.sleep(0.2)
        result = read(args.app)
    print(json.dumps(result))
    sys.exit(1 if "error" in result else 0)


if __name__ == "__main__":
    main()
