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

- A key bound to a command runs that command in text fields. It also beats an app shortcut on the same key. See "App shortcuts".
- A key bound to `none` does nothing in text fields. An app shortcut on that key still runs.
- A key the files do not name keeps Qt's own meaning.

The plugin checks for file changes each time Qt asks it for key bindings, which Qt does on every key press.

## Known limit: Backspace

`QLineEdit` and Qt Quick `TextInput` handle Backspace and Ctrl+Backspace in their own code, after they check the theme's keys. So a bindings file cannot block these two keys in those widgets.

## Known limit: document ends in `QLineEdit`

`QLineEdit` has no move to document start or end. It claims those keys (Ctrl+Home, Ctrl+End, with or without Shift) before app shortcuts, then does nothing with them. Use the line-start and line-end commands instead.

## App shortcuts

Before Qt runs an app shortcut (`QAction`, `QShortcut`), it sends a `ShortcutOverride` event to the focus object. If the event comes back accepted, Qt skips the shortcut and delivers the key press. Qt's text widgets accept it only for a fixed list of standard keys, so on their own they lose some text keys, such as Alt+BackSpace (delete word backward), to app shortcuts.

The plugin adds one event filter to the application object. On a `ShortcutOverride` event, it accepts the event when all of these are true:

- The receiver is the focus object, and it is a `QLineEdit`, `QTextEdit`, `QPlainTextEdit`, `QQuickTextInput` or `QQuickTextEdit` (or a subclass).
- The bindings files bind the key to a command.
- The widget can run the command. Delete commands need editable text. Moves and select-all need keyboard selection, where the widget has that setting: `textInteractionFlags` in `QTextEdit` and `QPlainTextEdit`, and `selectByKeyboard` in `QQuickTextEdit`. A read-only `QLineEdit` or `QQuickTextInput` still moves its caret. So in a read-only log view, bound keys stay with the app shortcut.

When it accepts the event, the filter also stops it, because the Qt Quick text items would ignore it again. It never changes any other event. So a blocked key, an unbound key, or a key outside a text field still runs the app shortcut. The text widget then runs the command through the standard keys that the plugin supplies.

The plugin installs the filter when Qt creates the theme. If the application object does not exist yet, it installs it on the first key bindings request.

A spin box (`QAbstractSpinBox`) and an editable `QComboBox` keep the focus themselves, not their inner `QLineEdit`. So in those fields, the filter does nothing, and Qt's own rules still decide.

Input method composition (preedit text) is not covered by an automated test, because the headless test session has no input method. Check it by hand with a real input method.

## Checks

`meson test -C build qt_keybindings` loads the plugin without a display and checks its key bindings against `tests/fixtures/stub`.

`meson test -C build qt_shortcut_filter qt_shortcut_filter_quick` checks which `ShortcutOverride` events the filter accepts, in widgets and in Qt Quick text items. Each test builds only when Qt Widgets or Qt Quick is found.

Two more checks start a headless sway with a private D-Bus session. They do not work inside a sandbox. Read `tests/harness/README.md` first.

- `adapters/qt/tests/check-theme.sh build` compares a dump of all theme values under `gtk3` and under `textinputflow`. Only the `keys` lines may differ.
- `adapters/qt/tests/check-keys.sh build` sends keys to `QLineEdit` and `QPlainTextEdit` through `tests/harness/run.sh` and compares the results with the `.expected` files. The shortcut checks use `qt_shortcut_app.py`, whose app shortcuts move the caret to 3. They also run it with a read-only `QLineEdit`, and with the plugin off, where Alt+BackSpace runs the app shortcut.
