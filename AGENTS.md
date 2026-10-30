# AGENTS.md

Guidance for coding agents working in this repository.

## What this is

`libtextinputflow` is a proof of concept for macOS-style text-editing key bindings on Linux. One bindings file decides what text-editing keys (word moves, line ends, word deletes) do in every app. Each toolkit asks the library what a key means, then runs its own version of that command.

The POC covers GTK 4 and Qt 6, on Arch Linux with Hyprland (Omarchy). It is done: `tests/e2e/run.sh` passes 56 of 56.

- Spec of record: `docs/plans/2026-10-27-libtextinputflow-poc-trd.md`. Read the sections that touch your work before you change behavior.
- Background and decisions: `docs/poc-brief.md`.
- Spike results that the design rests on: `spikes/h1` to `spikes/h6`, each with a `README.md`.

## Layout

| Path | What it is |
|---|---|
| `include/textinputflow.h` | Public C API. It is the contract between the library and the adapters |
| `lib/` | The C99 library: `textinputflow.c` (API), `parse.c` (file format), `table.c` (merged bindings), `paths.c` (XDG file locations), `watch.c` (inotify live reload) |
| `data/bindings` | The default distro bindings file (normal Linux GUI keys) |
| `docs/api.md`, `docs/bindings-format.md` | The guide for app builders and the file format spec. Both are part of the contract |
| `packaging/arch/PKGBUILD` | Builds the `libtextinputflow` and `libtextinputflow-qt6` Arch packages from a release tag |
| `CHANGELOG.md` | One section per release |
| `adapters/qt/` | Qt platform theme plugin `textinputflow`. It wraps the base theme and replaces only the 21 text-editing `StandardKey`s |
| `adapters/gtk4/` | `textinputflow.patch` for GTK 4.24.1, and `build-gtk.sh` to download, patch and build GTK |
| `tests/lib/` | Library unit tests, run by `meson test` |
| `tests/harness/` | Headless GUI test harness: GTK and Qt test apps, an AT-SPI2 probe, a headless sway runner |
| `tests/e2e/` | End-to-end test that proves the POC |
| `tests/fixtures/stub/` | The 25-entry bindings fixture that the adapter checks use |
| `spikes/` | Findings of the six spikes, as READMEs only. The spike code was removed; `spikes/README.md` says where it lives now |

## Build and test

```sh
meson setup build -Dqt=enabled     # library, Qt plugin and unit tests
meson compile -C build
meson test -C build                # all unit tests
meson test -C build <test name>    # one test, e.g. qt_keybindings
```

The build must have no compiler warnings at `warning_level=2`.

GUI checks, which need a real session (see below):

```sh
adapters/qt/tests/check-theme.sh build              # Qt theme values match the wrapped gtk3 theme
adapters/qt/tests/check-keys.sh build               # Qt caret keys and app shortcuts
adapters/gtk4/build-gtk.sh <work dir> build [enabled|disabled]   # patched GTK, about 2 minutes the first time
adapters/gtk4/tests/check-keys.sh <work dir> build  # GTK caret keys and app shortcuts
tests/e2e/run.sh [build [build/gtk-work]]           # the POC proof, 56 checks
```

Run the narrowest test that covers your change. Run `tests/e2e/run.sh` before a PR that changes the library, either adapter, or the harness. It must stay at 56 of 56.

## GUI tests: traps

The harness starts a headless sway inside a private D-Bus session. Nothing reaches the desktop. Each item below cost a failed run in the spikes, so keep them:

- **Run outside the sandbox.** The private D-Bus session needs to create its socket in `/run/user/<uid>`.
- **Pass library paths as `H6_LD_LIBRARY_PATH`, never `LD_LIBRARY_PATH`.** Arch's `sway` has the `cap_sys_nice` file capability, so glibc strips `LD_*` variables from it and from every app it starts.
- **Keep the system folders on `XDG_DATA_DIRS`.** Use `<fixture or temp dir>:/usr/local/share:/usr/share`. With the fixture alone, `at-spi-bus-launcher` aborts and every probe fails. `XDG_CONFIG_HOME` may point at the fixture alone.
- **Use `/usr/bin/python3`.** Homebrew's and `mise`'s `python3` lack `gi` and `PyQt6`.
- The harness already handles these, so do not remove them: it starts `at-spi-bus-launcher` and `at-spi2-registryd` by hand, keeps one idle `wtype -s 600000` running for keyboard focus, sets `GSK_RENDERER=cairo`, and sets `QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1`.
- `H6_DUMP=1` logs each app's accessibility tree and which `libgtk-4`, `libQt6Gui` and `libtextinputflow` it loaded. Use it first when a check fails for no clear reason.

`tests/harness/README.md` documents every `H6_*` variable and the step file format.

## Rules

- **The public header is a contract.** Do not change `include/textinputflow.h` without the user's agreement. Both adapters build against it.
- **Keep the docs with the contract.** A change to the API, the file format or the adapter rules also updates `docs/api.md` or `docs/bindings-format.md`, and `CHANGELOG.md`. Follow the version rules in `docs/api.md`.
- **The bindings files decide text keys, and the library wins.** In a focused text field, a key bound to a command beats the toolkit's built-in binding and the app's own shortcut. A key set to `none` does nothing in text fields and leaves app shortcuts alone. On read-only text, delete commands leave app shortcuts alone. Keys the files do not mention keep the toolkit's behavior.
- **Use the toolkit's own commands.** The GTK patch emits GTK's existing signals and actions, and the Qt plugin only changes `StandardKey` lists. Do not add new movement code.
- **The Qt plugin uses Qt private API** (`QPlatformTheme`, `QPlatformThemeFactory`). It must be rebuilt for each Qt release.
- **The GTK change is a patch file** on a release tarball (`adapters/gtk4/textinputflow.patch`), not a GTK fork. Keep it applying cleanly with `patch -p1` to `gtk-4.24.1`.
- **Known differences are accepted, not bugs:** word moves land 1 character apart in GTK and Qt (#15), and Ctrl+End does nothing in Qt single-line fields (#35). Do not "fix" them without the user.
- C code is C99. Follow the style of the file you edit. Write no comments unless the code would mislead a reader without one.

## Workflow

- Tickets are GitHub issues on `dakotahp/libtextinputflow`. A ticket's "Read first" block and its "Update" bullets override the text below them.
- Work on a branch, never on `master`. Open a draft PR with `Closes #<n>` in the description. The user reviews and merges.
- When a ticket's spec looks wrong, check the claim against the source, then raise it with evidence. Several spec errors were caught this way during the POC.
- In this environment `gh` is a shell alias for `op plugin run -- gh` (1Password). Scripts do not see aliases, so call `op plugin run -- gh` inside scripts.
- On this machine, Homebrew tools come first on `PATH`. Build scripts strip `linuxbrew` and `mise` entries from `PATH`, so keep that when you add one.
