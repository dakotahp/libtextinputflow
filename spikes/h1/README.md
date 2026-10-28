# Spike H1: Qt reads text keys from the platform theme

**Result: yes.** A platform theme plugin decides which keys Qt text fields treat as caret commands. This holds in the Qt 6.12.0 source, and a test plugin proves it in running apps.

## Call chain (Qt 6.12.0)

1. Text widgets compare each key event with a `StandardKey`: `event == QKeySequence::MoveToNextWord`.
2. That operator calls `QKeyEvent::matches()` (`qevent.h:455`).
3. `matches()` calls `QKeySequence::keyBindings(key)` (`qevent.cpp:1579`).
4. `keyBindings()` calls `platformTheme()->keyBindings(key)` (`qkeysequence.cpp:880`).
5. `QPlatformTheme::keyBindings()` is `virtual` (`qplatformtheme.h:320`).

All four Qt text widgets use this path for caret moves: `QLineEdit` (`qwidgetlinecontrol.cpp`), `QTextEdit` and `QPlainTextEdit` (`qwidgettextcontrol.cpp`), and Qt Quick `TextInput` and `TextEdit`.

Qt asks the theme on every key press, with no cache. So live reload only needs the plugin to update its own table.

## Test

`theme/tifspike.cpp` is a 35-line theme plugin. It binds `MoveToNextWord` to Alt+Right and `MoveToPreviousWord` to Alt+Left, and it leaves everything else at Qt's defaults. `run.sh` builds it and runs the H6 harness with `steps.txt`, once with the default theme and once with the plugin.

Text: `alpha beta gamma`. Both `QLineEdit` and `QPlainTextEdit` gave the same results, and a repeat run was identical.

| Key | Default theme | Spike theme |
|---|---|---|
| Ctrl+Right | 0 → 6 | 0 → 0 |
| Alt+Right | no move | 0 → 6 |
| Alt+Right | no move | 6 → 11 |
| Alt+Left | no move | 11 → 6 |
| Shift+Alt+Right | no move | no move |

```
./run.sh
```

Needs: the H6 requirements, plus `gcc` and the Qt 6 private headers (part of `qt6-base`). No CMake.

## Findings for the adapter and the TRD

1. **Qt asks "which keys mean this command?", not "what does this key mean?".** The library needs a reverse lookup, from command to keys, in addition to `tif_lookup()`.
2. **Each extend variant is its own `StandardKey`.** Shift+Alt+Right did nothing, because the plugin rebound `MoveToNextWord` but not `SelectNextWord`. The adapter must map "extend" to the `Select*` keys.
3. **The theme's `KeyboardScheme` hint also changes behavior.** `QLineEdit` reads it, for example to decide what Left does when text is selected. Qt's default bindings are also filtered by it. A real plugin must pass the hint through from the theme it wraps (see H2, #2).
4. **Some keys are hard-coded.** `QLineEdit` and Qt Quick `TextInput` handle Backspace and Ctrl+Backspace in code, as a fallback after the `StandardKey` checks. A theme can add more keys for these commands, but it cannot fully unbind the defaults.
