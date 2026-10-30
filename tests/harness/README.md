# Test harness: keys in, caret out

This harness sends keys to GTK 4 and Qt text fields and reads the caret, the selection and the text through AT-SPI2. The end-to-end test (`tests/e2e/`) and the adapter checks (`adapters/*/tests/check-keys.sh`) use it.

It started as spike H6. `spikes/h6/README.md` keeps the spike's findings.

The tests run in a headless sway inside a private D-Bus session. No window opens on the desktop, and no key reaches it. This does not work inside a sandbox.

## Run it

```
./run.sh [results.jsonl]
```

Needs: `sway`, `wtype`, `at-spi2-core`, `python-gobject`, `python-pyqt6`, `gtk4`.

The script runs four cases, one per widget: `gtk4-entry` (`GtkEntry`), `gtk4-textview` (`GtkTextView`), `qt-lineedit` (`QLineEdit`) and `qt-plaintextedit` (`QPlainTextEdit`). Each test app shows the text `alpha beta gamma` with the caret at 0. Each case runs every step, then reads the field.

## Output

One JSON line per case and step:

```
{"toolkit":"gtk4","widget":"entry","step":"Home","result":{"role":"text","caret":0,"selection":[],"text":"alpha beta gamma"}}
```

- `caret`: the caret offset, in characters.
- `selection`: a list of `[start, end]` pairs. It is empty when nothing is selected.
- `text`: the field's full text, from `Atspi.Text.get_text(text, 0, -1)`.

If the probe cannot read the field, `result` is `{"error": "..."}` instead.

## Steps

`H6_STEPS=steps.txt` replaces the default key steps. Each line is `label:value`. The label must not contain `:`. The first line is usually `start:`, which sends nothing. The value is one of:

- empty: send nothing, only read the field.
- `wtype` arguments, for example `-M ctrl -k Right -m ctrl`. The harness waits 0.4 s after the keys.
- `@USR1`: send `SIGUSR1` to the app instead of keys.
- `@sh <command>`: run `<command>` with `bash -c`, then wait 1 s. The command sees the harness environment, so it can use `XDG_CONFIG_HOME` and any variable that the caller exports. Its output goes to the log. Example:

  ```
  reload:@sh cp "$E2E/user-v2" "$XDG_CONFIG_HOME/textinputflow/bindings"
  ```

## Switches

- `H6_ONLY=gtk4-entry` runs the cases whose name starts with this value. `H6_ONLY=gtk4` runs both GTK cases.
- `H6_GTK_APP=app.py` replaces the GTK test app. It must take `entry` or `textview` as its argument.
- `H6_QT_APP=app.py` replaces the Qt test app. It must take `entry` or `textview` as its argument, and name itself `tif-qt`.
- `H6_LD_LIBRARY_PATH=dir` sets `LD_LIBRARY_PATH` for the apps, for example to load a locally built GTK. A plain `LD_LIBRARY_PATH` does not work: see "Traps".
- `H6_QT_THEME=name` sets `QT_QPA_PLATFORMTHEME` for the Qt apps. Set `QT_PLUGIN_PATH` too, for a theme outside the system plugin folder.
- `H6_DUMP=1` writes each app's accessibility tree to `results.jsonl.log`. It also logs which `libgtk-4` or `libQt6Gui`, and which `libtextinputflow`, each app loaded.

## Traps

Each item below cost a failed run in the spike. Keep them.

- **Start AT-SPI2 by hand.** The accessibility bus runs on dbus-broker, which starts the registry through systemd. A private session bus has no systemd, so `session.sh` starts `at-spi-bus-launcher` and `at-spi2-registryd` itself.
- **Keep the system folders in `XDG_DATA_DIRS`.** Without `/usr/share`, `at-spi-bus-launcher` aborts ("Cannot get the default GSettingsSchemaSource"), and every probe fails. Add a test folder in front: `XDG_DATA_DIRS=<test folder>:/usr/local/share:/usr/share`.
- **Keep one virtual keyboard alive.** The headless seat has no keyboard, so no window gets keyboard focus, and AT-SPI2 reports no focused field. Each `wtype` run adds a keyboard and removes it on exit. One idle `wtype -s 600000` keeps a keyboard on the seat.
- **Use the system Python.** A `mise` or venv `python3` on `PATH` lacks `gi` and `PyQt6`.
- **Force GTK's cairo renderer** (`GSK_RENDERER=cairo`). The Vulkan renderer loses its surface on sway's software backend.
- **Pass library paths under another name.** Arch's `sway` has the `cap_sys_nice` file capability, so glibc removes `LD_LIBRARY_PATH` from its environment, and the apps never see it. Use `H6_LD_LIBRARY_PATH`.
- **Turn on Qt accessibility** with `QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1`.
- **Call `Atspi.Text.get_selection(text, i)` directly.** In Python, `text.get_selection` resolves to a deprecated method with a different signature.
