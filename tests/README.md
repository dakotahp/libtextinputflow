# Tests

- `lib/`: library unit tests. Run them with `meson test -C build`.
- `e2e/`: the end-to-end test. It proves the POC.
- `harness/`: the headless setup that sends keys and reads text fields. Read `harness/README.md` first.
- `fixtures/stub/`: the bindings that the adapter checks use.

## End-to-end test

The test shows that one bindings file controls caret keys in both GTK 4 and Qt, and that an edit to the file applies to running apps.

```
tests/e2e/run.sh [<library build dir> [<GTK work dir>]]
```

The defaults are `build` and `build/gtk-work`. The script:

1. Sets up the library build with `-Dqt=enabled` if it does not exist, and compiles it. That builds the library and the Qt plugin.
2. Builds the patched GTK with `adapters/gtk4/build-gtk.sh`, or reuses an earlier build in the work folder. The first build downloads GTK and takes about 2 minutes.
3. Makes a distro file from `data/bindings` and a user file from `e2e/user-v1`, in `<build dir>/e2e/`.
4. Runs `e2e/steps` in `GtkEntry`, `GtkTextView`, `QLineEdit` and `QPlainTextEdit` through the harness. Two steps copy `e2e/user-v2` and then `e2e/user-v1` over the user file while the apps run.
5. Compares the results with `e2e/expected.jsonl`, prints each mismatch, and exits 1 if there is any.

The results and the harness log stay in `<build dir>/e2e/` after the run.

The test starts a headless sway with a private D-Bus session, so it does not work inside a sandbox.

### Needed packages

On Arch: `meson`, `ninja`, `curl`, `glib2-devel`, `gtk4`, `qt6-base`, `sway`, `wtype`, `at-spi2-core`, `python-gobject`, `python-pyqt6`.

### Why GTK and Qt give different carets

The text is `alpha beta gamma`. In steps 2, 3, 7 and 12 the two toolkits stop at different places, and the expected file says so.

These steps are word moves forward. The bindings file maps the key to `move word forward`, and each toolkit then runs its own word move. GTK stops at the end of the current word (`alpha|` is 5). Qt stops at the start of the next word (`|beta` is 6). Matching the two is left for later (#15).

| Step | Key | GTK | Qt |
|---|---|---|---|
| 2 | Alt+Right from 0 | 5 | 6 |
| 3 | Alt+Right again | 10 | 11 |
| 7 | Shift+Alt+Right from 0 | 5, selects 0–5 | 6, selects 0–6 |
| 12 | Ctrl+Right from 0, after the reload removes the block | 5 | 6 |

Step 13 copies `user-v1` back and sends no key, so it keeps the caret from step 12: 5 in GTK and 6 in Qt. This also shows that a reload does not move the caret.

The other steps give the same result in both toolkits. A word move backward lands at the start of a word in both (step 4), and a blocked key does nothing in both (step 5).
