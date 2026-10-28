# Qt platform theme plugin

This plugin makes Qt text fields follow the textinputflow bindings files. It is a Qt platform theme named `textinputflow`. It wraps your normal Qt theme and changes only the text-editing key bindings.

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

The plugin wraps the theme named in `TEXTINPUTFLOW_QT_BASE_THEME`. If that variable is unset or empty, it wraps `gtk3`. If the plugin cannot create that theme, it uses Qt's plain theme. Text keys follow the bindings files in all three cases.

## What it changes

For each Qt standard key, such as `MoveToNextWord`, the plugin starts from the base theme's keys. It removes every key that the bindings files bind or block, then adds the keys bound to the matching command. So:

- A key bound to a command runs that command in text fields. It also beats an app shortcut on the same key.
- A key bound to `none` does nothing in text fields. An app shortcut on that key still runs.
- A key the files do not name keeps Qt's own meaning.

The plugin checks for file changes each time Qt asks it for key bindings, which Qt does on every key press.

## Known limit: Backspace

`QLineEdit` and Qt Quick `TextInput` handle Backspace and Ctrl+Backspace in their own code, after they check the theme's keys. So a bindings file cannot block these two keys in those widgets.

## Checks

`meson test -C build qt_keybindings` loads the plugin without a display and checks its key bindings against `tests/fixtures/stub`.

Two more checks start a headless sway with a private D-Bus session. They do not work inside a sandbox. Read `spikes/h6/README.md` first.

- `adapters/qt/tests/check-theme.sh build` compares a dump of all theme values under `gtk3` and under `textinputflow`. Only the `keys` lines may differ.
- `adapters/qt/tests/check-keys.sh build` sends keys to `QLineEdit` and `QPlainTextEdit` through `spikes/h6/run.sh` and compares the caret results with `keys.expected` and `shortcuts.expected`.
