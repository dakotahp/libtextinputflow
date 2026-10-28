# Qt platform theme plugin

This plugin makes Qt text fields follow the textinputflow bindings files. It is a Qt platform theme named `textinputflow`. It wraps another Qt theme, `gtk3` by default, and changes only the key bindings.

## Build

The plugin needs Qt 6 with its private headers (`qt6-base` on Arch).

```
meson setup build -Dqt=enabled
meson compile -C build
```

The plugin is `build/adapters/qt/platformthemes/libtextinputflow-qt.so`. With `-Dqt=auto` (the default), Meson builds it only when Qt 6 is found. With `-Dqt=disabled`, nothing Qt-related builds.

The plugin uses Qt's private API, so rebuild it after each Qt update.

## Turn it on

```
export QT_PLUGIN_PATH=$PWD/build/adapters/qt
export QT_QPA_PLATFORMTHEME=textinputflow
```

The plugin wraps the theme named in `TEXTINPUTFLOW_QT_BASE_THEME`. If that variable is unset or empty, it wraps `gtk3`. If the plugin cannot create that theme, it prints a warning and uses Qt's plain theme. Text keys follow the bindings files in all three cases.

## What it changes

The plugin changes only the 21 text-editing standard keys, such as `MoveToNextWord` and `DeleteStartOfWord`. For each of them, it starts from the base theme's keys. It removes every key that the bindings files bind or block, then adds the keys bound to the matching command. All other standard keys, such as `Copy`, `Back` and `Forward`, keep the base theme's keys. So:

- A key bound to a command runs that command in text fields. For some commands it also beats an app shortcut on the same key. See "Known limit: app shortcuts".
- A key bound to `none` does nothing in text fields. An app shortcut on that key still runs.
- A key the files do not name keeps Qt's own meaning.

The plugin checks for file changes each time Qt asks it for key bindings, which Qt does on every key press.

## Known limit: Backspace

`QLineEdit` and Qt Quick `TextInput` handle Backspace and Ctrl+Backspace in their own code, after they check the theme's keys. So a bindings file cannot block these two keys in those widgets.

## Known limit: app shortcuts

A text field beats an app shortcut only for the standard keys that the widget claims in its `ShortcutOverride` handling. Word moves pass in both `QLineEdit` and `QPlainTextEdit`. Delete word backward (Alt+BackSpace in the stub table) does not: the app shortcut runs, in both widgets. Select to document end (Shift+Ctrl+End) passes in `QPlainTextEdit` but not in `QLineEdit`.

## Checks

`meson test -C build qt_keybindings` loads the plugin without a display and checks its key bindings against `tests/fixtures/stub`.

Two more checks start a headless sway with a private D-Bus session. They do not work inside a sandbox. Read `tests/harness/README.md` first.

- `adapters/qt/tests/check-theme.sh build` compares a dump of all theme values under `gtk3` and under `textinputflow`. Only the `keys` lines may differ.
- `adapters/qt/tests/check-keys.sh build` sends keys to `QLineEdit` and `QPlainTextEdit` through `tests/harness/run.sh` and compares the caret results with `keys.expected` and `shortcuts.expected`.
