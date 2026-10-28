# Spike H6: automated caret tests

**Result: yes.** A script can send keys to GTK 4 and Qt text fields and read the caret and selection through AT-SPI2. Three runs gave identical results. One run takes about 18 seconds.

The tests run in a headless sway inside a private D-Bus session. No window opens on the desktop, and no key reaches it. So the claim was tested on sway, not on Hyprland itself. That is a better fit for automated tests anyway.

## Run it

```
./run.sh [results.jsonl]
```

Needs: `sway`, `wtype`, `at-spi2-core`, `python-gobject`, `python-pyqt6`, `gtk4`.

Debug switches:

- `H6_ONLY=gtk4-entry` runs one case (`gtk4-entry`, `gtk4-textview`, `qt-lineedit`, `qt-plaintextedit`).
- `H6_DUMP=1` writes each app's accessibility tree to `results.jsonl.log`.
- `H6_STEPS=steps.txt` replaces the default key steps. Each line is `label:wtype args`, and the first line is usually `start:`.
- `H6_QT_THEME=name` sets `QT_QPA_PLATFORMTHEME` for the Qt apps. Set `QT_PLUGIN_PATH` too, for a theme outside the system plugin folder.

## Results

Text: `alpha beta gamma` (16 characters). The caret starts at 0.

| Step | GTK 4 entry | GTK 4 text view | Qt line edit | Qt plain text edit |
|---|---|---|---|---|
| Ctrl+Right | 5 | 5 | **6** | **6** |
| Ctrl+Right | 10 | 10 | **11** | **11** |
| Shift+Ctrl+Right | 16, sel 10–16 | 16, sel 10–16 | 16, sel 11–16 | 16, sel 11–16 |
| Home | 0 | 0 | 0 | 0 |
| End | 16 | 16 | 16 | 16 |
| Ctrl+A | 16, sel 0–16 | **0**, sel 0–16 | 16, sel 0–16 | 16, sel 0–16 |

## Findings

1. **"Word forward" differs between toolkits today.** GTK stops at the end of the current word. Qt stops at the start of the next word. The same key gives different results, which is the problem this project exists to fix. The vocabulary must name both moves (the design's "word" and "word end" units), and the TRD must pick one default for Ctrl+Right.
2. **The caret after Select All differs.** The GTK text view puts it at the start. The other three put it at the end. This is minor, but the end-to-end test must not depend on it.

## What it took

Each item below cost a failed run. Later test code must keep them.

- **Start AT-SPI2 by hand.** The accessibility bus runs on dbus-broker, which starts the registry through systemd. A private session bus has no systemd, so `session.sh` starts `at-spi-bus-launcher` and `at-spi2-registryd` itself.
- **Keep one virtual keyboard alive.** The headless seat has no keyboard, so no window gets keyboard focus, and AT-SPI2 reports no focused field. Each `wtype` run adds a keyboard and removes it on exit. One idle `wtype -s 600000` keeps a keyboard on the seat.
- **Use the system Python.** A `mise` or venv `python3` on `PATH` lacks `gi` and `PyQt6`.
- **Force GTK's cairo renderer** (`GSK_RENDERER=cairo`). The Vulkan renderer loses its surface on sway's software backend.
- **Turn on Qt accessibility** with `QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1`.
- **Call `Atspi.Text.get_selection(text, i)` directly.** In Python, `text.get_selection` resolves to a deprecated method with a different signature.
