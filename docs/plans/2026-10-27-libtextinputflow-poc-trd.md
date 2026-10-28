# TRD: libtextinputflow proof of concept

**Date:** 2026-10-27
**Status:** Published
**Technical plan:** none. The approach comes from `docs/poc-brief.md` and the spike results in `spikes/h1` to `spikes/h6`.
**Planner session:** linux-text-caret-poc [a3e9aa]

---

## Scope

This document specifies the POC build. It covers four parts:

- `libtextinputflow`, a C99 library that reads bindings files and answers "what does this key mean?" and "which keys mean this command?"
- a Qt platform theme plugin that wraps the user's Qt theme and takes text-editing keys from the library
- a patch to GTK 4.24.1 that gives `GtkText` and `GtkTextView` one shortcut controller fed from the library
- an end-to-end test that proves one bindings file controls both toolkits and reloads live

Left for later: sandboxed apps and portal delivery, terminals, Chromium, Electron and Firefox, multi-key sequences, kill and yank, the modifier swap, mixed-direction text, field types (single-line, password), and matching word-move landing spots across toolkits (#15).

### Terms

- **Command:** one text-editing action, such as "move word forward" or "delete character backward". Defined in "Command vocabulary" below.
- **Binding:** one key plus modifiers, mapped to a command or to `none`.
- **Blocked key:** a key bound to `none`. In text fields it does nothing.
- **Distro file / user file:** the two bindings files. The user file overrides the distro file key by key.
- **Native binding:** a key that a toolkit maps to a command by itself, with no library.
- **Harness:** the headless test setup in `spikes/h6/`. It runs GTK 4 and Qt test apps in a headless sway with a private D-Bus session, sends keys with `wtype`, and reads the caret, selection and (after ticket #12) text through AT-SPI2. Read `spikes/h6/README.md` before using it.

## Interfaces and Contracts

### Repository layout (new)

```
meson.build                      root build: library, data, tests, optional Qt adapter
meson.options                    option('qt', type: 'feature', value: 'auto')
                                 option('tests', type: 'boolean', value: true)
LICENSE                          MIT
include/textinputflow.h          the public header (below)
lib/                             library sources and lib/meson.build
data/bindings                    the default distro file (ticket #8)
tests/lib/                       library unit tests (plain C, no framework)
adapters/qt/                     Qt platform theme plugin (ticket #10)
adapters/gtk4/                   GTK 4 patch and build script (ticket #11)
tests/harness/                   copy of spikes/h6, extended (ticket #12)
tests/e2e/                       end-to-end test (ticket #12)
```

`spikes/` stays as the record of the spikes. Code outside `spikes/` must not depend on files in `spikes/`, except that tickets #10 and #11 use `spikes/h6/run.sh` to check their acceptance criteria before #12 copies it.

### Build outputs (new)

- Shared library `libtextinputflow.so.0`, soname `libtextinputflow.so.0`, built with Meson's `library()` and `version: '0.0.0'`.
- Header `include/textinputflow.h`, installed to `<includedir>`.
- pkg-config file `textinputflow.pc`, generated with Meson's `pkgconfig` module, `name: 'textinputflow'`. Meson also writes `<builddir>/meson-uninstalled/textinputflow-uninstalled.pc`. Consumers in this repo build against the uninstalled file with `PKG_CONFIG_PATH=<builddir>/meson-uninstalled`.
- No system install is needed for the POC. Everything runs from build folders.

### Public header (new): `include/textinputflow.h`

This is the whole contract. #19 lands it exactly as written. Later tickets may not change it. A change needs the planner session.

```c
#ifndef TEXTINPUTFLOW_H
#define TEXTINPUTFLOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIF_API_VERSION 0

typedef enum {
    TIF_ACTION_MOVE = 1,
    TIF_ACTION_DELETE = 2,
    TIF_ACTION_SELECT_ALL = 3
} tif_action;

typedef enum {
    TIF_UNIT_NONE = 0,
    TIF_UNIT_CHARACTER = 1,
    TIF_UNIT_WORD = 2,
    TIF_UNIT_LINE_ENDS = 3,
    TIF_UNIT_DOCUMENT_ENDS = 4
} tif_unit;

typedef enum {
    TIF_DIRECTION_BACKWARD = -1,
    TIF_DIRECTION_NONE = 0,
    TIF_DIRECTION_FORWARD = 1
} tif_direction;

enum {
    TIF_MOD_SHIFT = 1u << 0,
    TIF_MOD_CONTROL = 1u << 1,
    TIF_MOD_ALT = 1u << 2,
    TIF_MOD_SUPER = 1u << 3
};
#define TIF_MOD_MASK 0xFu

typedef struct {
    tif_action action;
    tif_unit unit;
    tif_direction direction;
    bool extend;
} tif_command;

typedef struct {
    uint32_t keysym;
    uint32_t mods;
} tif_key;

typedef struct {
    tif_key key;
    bool blocked;
    tif_command command;
} tif_binding;

typedef enum {
    TIF_LOOKUP_UNBOUND = 0,
    TIF_LOOKUP_COMMAND = 1,
    TIF_LOOKUP_BLOCKED = 2
} tif_lookup_result;

typedef struct tif_ctx tif_ctx;

tif_ctx *tif_new(void);
tif_ctx *tif_new_for_paths(const char *distro_file, const char *user_file);
void tif_free(tif_ctx *ctx);

bool tif_reload(tif_ctx *ctx);
int tif_fd(const tif_ctx *ctx);
bool tif_refresh(tif_ctx *ctx);

tif_lookup_result tif_lookup(const tif_ctx *ctx, tif_key key, tif_command *out);
size_t tif_binding_count(const tif_ctx *ctx);
bool tif_binding_at(const tif_ctx *ctx, size_t index, tif_binding *out);
size_t tif_keys_for_command(const tif_ctx *ctx, const tif_command *command,
                            tif_key *out, size_t max);

#ifdef __cplusplus
}
#endif

#endif
```

Function contracts:

| Function | Contract |
|---|---|
| `tif_new()` | Creates a context for the standard paths (see "File locations"), loads both files, and starts watching. Returns `NULL` only when memory allocation fails. Missing or unreadable files are not errors: they count as empty. |
| `tif_new_for_paths(distro, user)` | Same as `tif_new()`, but with explicit file paths. Either may be `NULL`, which means "no such file". Unit tests and adapters' tests use this. |
| `tif_free(ctx)` | Frees the context and closes its file descriptor. `tif_free(NULL)` does nothing. |
| `tif_reload(ctx)` | Re-reads both files now. Returns `true` if the merged binding table differs from before, `false` if it is the same or `ctx` is `NULL`. |
| `tif_fd(ctx)` | Returns a non-blocking, close-on-exec file descriptor that becomes readable when a bindings file may have changed. Returns `-1` if watching is not available (inotify failed, the stub, or `ctx` is `NULL`). The caller must not read from or close it. |
| `tif_refresh(ctx)` | Never blocks. Drains pending change events from `tif_fd`. If any event concerns a bindings file or its folder, it reloads as `tif_reload` does. Returns `true` only if the merged binding table changed. Returns `false` when nothing is pending. Safe to call very often, for example on every key press. |
| `tif_lookup(ctx, key, out)` | Normalizes the key (see "Key matching"), then looks it up in the merged table. Returns `TIF_LOOKUP_COMMAND` and fills `*out`, or `TIF_LOOKUP_BLOCKED`, or `TIF_LOOKUP_UNBOUND`. `out` may be `NULL`. When the result is not `TIF_LOOKUP_COMMAND`, `*out` is not touched. With `ctx == NULL`, returns `TIF_LOOKUP_UNBOUND`. |
| `tif_binding_count(ctx)` | Number of entries in the merged table, blocked keys included. `0` for `NULL`. |
| `tif_binding_at(ctx, i, out)` | Copies entry `i` into `*out` and returns `true`, or returns `false` if `i >= count` or an argument is `NULL`. For a blocked entry, `blocked` is `true` and `command` is all zero. Order is stable until the table changes. |
| `tif_keys_for_command(ctx, cmd, out, max)` | Writes up to `max` keys bound to exactly `*cmd` (all four fields equal) into `out`, and returns the total number of such keys, which may be larger than `max`. `out` may be `NULL` when `max` is `0`. Returns `0` for an invalid command or a `NULL` argument. |

The library is not thread-safe. Each adapter uses one context from its GUI thread only.

### Command vocabulary (new)

Valid commands (any other combination is invalid):

| `action` | `unit` | `direction` | `extend` | File syntax |
|---|---|---|---|---|
| `MOVE` | `CHARACTER` | `BACKWARD` / `FORWARD` | either | `move character backward` / `move character forward` |
| `MOVE` | `WORD` | `BACKWARD` / `FORWARD` | either | `move word backward` / `move word forward` |
| `MOVE` | `LINE_ENDS` | `BACKWARD` / `FORWARD` | either | `move line-start` / `move line-end` |
| `MOVE` | `DOCUMENT_ENDS` | `BACKWARD` / `FORWARD` | either | `move document-start` / `move document-end` |
| `DELETE` | `CHARACTER` | `BACKWARD` / `FORWARD` | `false` | `delete character backward` / `delete character forward` |
| `DELETE` | `WORD` | `BACKWARD` / `FORWARD` | `false` | `delete word backward` / `delete word forward` |
| `SELECT_ALL` | `NONE` | `NONE` | `false` | `select-all` |

For `MOVE`, the word `extend` at the end of the line sets `extend = true`.

`move word` means the toolkit's own word move. GTK stops at the end of the word, and Qt at the start of the next word. This difference is accepted for the POC (#15).

### Bindings file format (new)

The file is UTF-8 text. Each line is one of:

- empty, or only spaces and tabs: ignored
- first non-blank character is `#`: a comment, ignored
- `<keyspec> = <command>`, or `<keyspec> = none`

Spaces and tabs around `=` and between words are ignored, and any amount is allowed. A trailing `#` comment on a binding line is an error.

`<keyspec>` is zero or more modifiers and one key, joined with `+`, with the key last and no spaces inside:

- Modifiers, case-insensitive: `Shift`, `Control` (or `Ctrl`), `Alt`, `Super`. Any order. A modifier given twice is an error.
- Keys, case-sensitive, with their X11 keysym values:

| Name | Keysym | Name | Keysym |
|---|---|---|---|
| `Left` | `0xff51` | `Home` | `0xff50` |
| `Up` | `0xff52` | `End` | `0xff57` |
| `Right` | `0xff53` | `Page_Up` | `0xff55` |
| `Down` | `0xff54` | `Page_Down` | `0xff56` |
| `BackSpace` | `0xff08` | `Delete` | `0xffff` |
| `a` to `z` | `0x61` to `0x7a` | | |

  Uppercase `A` to `Z` are an error. Write `Shift+a` instead.

Example:

```
# Linux defaults for word moves
Control+Left        = move word backward
Shift+Control+Left  = move word backward extend
Control+BackSpace   = delete word backward
Control+a           = select-all
Control+Right       = none
```

### Default distro file (new): `data/bindings`

Ticket #8 creates it with exactly these 21 bindings, the Linux GUI defaults that GTK and Qt already use:

```
Left                = move character backward
Right               = move character forward
Shift+Left          = move character backward extend
Shift+Right         = move character forward extend
Control+Left        = move word backward
Control+Right       = move word forward
Shift+Control+Left  = move word backward extend
Shift+Control+Right = move word forward extend
Home                = move line-start
End                 = move line-end
Shift+Home          = move line-start extend
Shift+End           = move line-end extend
Control+Home        = move document-start
Control+End         = move document-end
Shift+Control+Home  = move document-start extend
Shift+Control+End   = move document-end extend
BackSpace           = delete character backward
Delete              = delete character forward
Control+BackSpace   = delete word backward
Control+Delete      = delete word forward
Control+a           = select-all
```

It is installed to `<datadir>/textinputflow/bindings`.

### Stub table (#19 only)

Until ticket #8 lands, every context, from `tif_new()` or `tif_new_for_paths()`, ignores files and holds this fixed table. It is the 21 defaults above, except that `Control+Right` is blocked, plus four test bindings:

```
(the 21 defaults, with this one line changed:)
Control+Right       = none
(plus:)
Alt+Left            = move word backward
Alt+Right           = move word forward
Shift+Alt+Right     = move word forward extend
Alt+BackSpace       = delete word backward
```

That makes 25 entries. In the stub, `tif_fd` returns `-1`, and `tif_reload` and `tif_refresh` return `false`. The adapter tickets test against this table, so it must match exactly.

#### Stub fixture (#8)

Ticket #8 removes the stub, so it also adds the same 25 entries as a bindings file: `tests/fixtures/stub/textinputflow/bindings`, with exactly these lines:

```
Left                = move character backward
Right               = move character forward
Shift+Left          = move character backward extend
Shift+Right         = move character forward extend
Control+Left        = move word backward
Control+Right       = none
Shift+Control+Left  = move word backward extend
Shift+Control+Right = move word forward extend
Home                = move line-start
End                 = move line-end
Shift+Home          = move line-start extend
Shift+End           = move line-end extend
Control+Home        = move document-start
Control+End         = move document-end
Shift+Control+Home  = move document-start extend
Shift+Control+End   = move document-end extend
BackSpace           = delete character backward
Delete              = delete character forward
Control+BackSpace   = delete word backward
Control+Delete      = delete word forward
Control+a           = select-all
Alt+Left            = move word backward
Alt+Right           = move word forward
Shift+Alt+Right     = move word forward extend
Alt+BackSpace       = delete word backward
```

**"Against the stub table"** in the adapter tickets (#10, #11, #20) means:

- **Before #8 merges:** the #19 stub library, with no extra setup.
- **After #8 merges:** the real library, with these variables for the test apps:
  - `XDG_CONFIG_HOME=<repo>/tests/fixtures/stub`
  - `XDG_DATA_DIRS=<repo>/tests/fixtures/stub:/usr/local/share:/usr/share`

  The library takes the first folder in `XDG_DATA_DIRS` that has `textinputflow/bindings`, so the distro file and the user file are both the fixture, and the merged table is the same 25 entries. sway passes these variables through to the apps (only `LD_*` variables are stripped).

  Never set `XDG_DATA_DIRS` to the fixture folder alone. The rest of the desktop reads it too. Without `/usr/share`, `at-spi-bus-launcher` aborts with "Cannot get the default GSettingsSchemaSource", and every harness probe fails.

The expected caret results in those tickets are the same either way.

### File locations

- User file: `$XDG_CONFIG_HOME/textinputflow/bindings`. If `XDG_CONFIG_HOME` is unset or empty, use `$HOME/.config`.
- Distro file: the first existing `<dir>/textinputflow/bindings` for `<dir>` in `$XDG_DATA_DIRS`, in order. If `XDG_DATA_DIRS` is unset or empty, use `/usr/local/share:/usr/share`. If none exists, there is no distro file. `tif_new()` keeps the folder list and searches it again on every load, so a reload can find a distro file installed after start-up.
- Relative paths in `XDG_CONFIG_HOME` or `XDG_DATA_DIRS` are ignored, as the XDG Base Directory spec requires. Only regular files are read.
- `tif_new_for_paths()` always uses its two explicit paths.

### Qt plugin (new): `adapters/qt/`

- Plugin key: `textinputflow`. Built as `libtextinputflow-qt.so` in a `platformthemes/` subfolder of the build folder, so `QT_PLUGIN_PATH=<builddir>/adapters/qt` finds it.
- Turned on with `QT_QPA_PLATFORMTHEME=textinputflow`.
- The wrapped theme comes from `TEXTINPUTFLOW_QT_BASE_THEME`. If unset or empty, it is `gtk3`. If that theme cannot be created, the plugin uses Qt's plain `QPlatformTheme`.

### GTK patch (new): `adapters/gtk4/`

- `adapters/gtk4/textinputflow.patch`: one patch, applied with `patch -p1` to an unpacked `gtk-4.24.1` release tarball.
- `adapters/gtk4/build-gtk.sh <work dir> <textinputflow build dir>`: downloads and checks the tarball, applies the patch, and builds with the options from `spikes/h5/build-gtk.sh` plus `-Dtextinputflow=enabled`. It prints the folder that holds the built `libgtk-4.so.1`.
- New GTK build option in the patch's `meson.options`: `option('textinputflow', type: 'feature', value: 'disabled')`.

## Behavior

### Loading and merging

1. Parse the distro file, then the user file, line by line.
2. A line that fails to parse is skipped. The library writes one line to stderr: `textinputflow: <path>:<line number>: <reason>`. The rest of the file is still used.
3. Within one file, if a key appears twice, the later line wins, and the library writes a warning in the same format.
4. The merged table holds every key from the distro file, then the user file overrides key by key. A user `none` makes the key blocked even if the distro file binds it.
5. A file that is missing, unreadable or empty adds nothing, and the library prints no warning for it.
6. A line longer than 1024 bytes, or one with bytes outside printable ASCII, tab and the line ending, is an error line.
7. A reload builds a new table and then replaces the old one. A lookup never sees half a table.

### Text bindings and app shortcuts

When a text field has focus, the bindings file decides what its keys do, even when the app has its own shortcut on the same key:

| The bindings file says | Text field | App shortcut on the same key |
|---|---|---|
| a command | runs the command | does not run |
| `none` | ignores the key | runs |
| nothing | toolkit's native behavior | toolkit's native behavior |

Exception: while an input method is composing text (preedit text is showing), the input method gets the key first, as today.

Neither toolkit does this fully today. In GTK 4, app shortcuts run first, in the window's capture phase (`gtkwindow.c:2965`). In Qt, text widgets accept the `ShortcutOverride` event only for a fixed list of `StandardKey`s, and the list differs per widget (`qwidgetlinecontrol.cpp:1541`). So in Qt the text field wins for some commands, such as word moves, and the app wins for others, such as Alt+BackSpace (delete word backward) in `QLineEdit` and `QPlainTextEdit`. #20 makes GTK follow this rule, and #27 makes Qt follow it.

### Key matching

Before lookup, the library normalizes the key: `mods &= TIF_MOD_MASK`, and keysyms `0x41` to `0x5a` (`A` to `Z`) become `0x61` to `0x7a`. Then it needs an exact match on `keysym` and `mods`. So Caps Lock, Num Lock and other modifier bits never stop a match.

### Live reload

- The library watches the folders that hold the two files, not the files, so that editors which save by writing a new file and renaming it are caught.
- If the user file's folder does not exist, the library watches its parent (`$XDG_CONFIG_HOME` or `~/.config`). When `textinputflow/` is created there, it starts watching that folder too.
- If the parent is also missing, there is no live reload for the user file, and `tif_new` still succeeds.
- Change events for other files in the watched folders cause no reload.

### Qt plugin rules

The plugin forwards all 20 virtual `QPlatformTheme` functions to the base theme, as `spikes/h2/theme/tifwrap.cpp` does, except `keyBindings()`. For `keyBindings(k)`:

1. Call `tif_refresh(ctx)`. This is how the plugin picks up file changes. Qt asks the theme on every key press, so no event loop hook is needed.
2. Start with `base = baseTheme->keyBindings(k)`.
3. If `k` is one of the 21 text-editing `StandardKey`s in the mapping table below, remove from `base` every key sequence that the library binds to any command, or blocks. Compare single-key sequences only. Multi-key sequences in `base` stay. For every other `StandardKey` (such as `Back`, `Forward` or `Copy`), skip this step, so app-level keys keep working everywhere.
4. If `k` maps to a command in the table below, append the library's keys for that command (`tif_keys_for_command`), converted to `QKeySequence`. Skip duplicates.
5. Return the result.

So among the text-editing keys, the library wins. A blocked key does nothing in text fields. A key the library does not mention keeps Qt's native meaning.

Step 3 is limited to the text-editing keys because Qt's `StandardKey` lists are global: apps also use them for their own actions. Stripping BackSpace or Alt+Left from `Back` would break "go back" in every Qt app, even outside text fields. Known limit: if the library binds a key that a non-text `StandardKey` also uses (for example `Control+c`, which `Copy` uses), Qt's text widgets may still run that other action first. The POC does not bind such keys.

App shortcuts (`QAction`, `QShortcut`) are handled by #27, not here. Text widgets claim priority over them only for some keys, so #27 adds an application-wide event filter.

`StandardKey` to command mapping:

| `StandardKey` | Command |
|---|---|
| `MoveToPreviousChar` / `MoveToNextChar` | move character backward / forward |
| `MoveToPreviousWord` / `MoveToNextWord` | move word backward / forward |
| `MoveToStartOfLine` / `MoveToEndOfLine` | move line-start / line-end |
| `MoveToStartOfDocument` / `MoveToEndOfDocument` | move document-start / document-end |
| `SelectPreviousChar` / `SelectNextChar` | move character backward / forward, extend |
| `SelectPreviousWord` / `SelectNextWord` | move word backward / forward, extend |
| `SelectStartOfLine` / `SelectEndOfLine` | move line-start / line-end, extend |
| `SelectStartOfDocument` / `SelectEndOfDocument` | move document-start / document-end, extend |
| `Backspace` | delete character backward |
| `Delete` | delete character forward |
| `DeleteStartOfWord` / `DeleteEndOfWord` | delete word backward / forward |
| `SelectAll` | select-all |

Key conversion: modifiers map as Shift → `Qt::ShiftModifier`, Control → `Qt::ControlModifier`, Alt → `Qt::AltModifier`, Super → `Qt::MetaModifier`. Keysyms map with a fixed table: the ten named keys to `Qt::Key_Left`, `Key_Up`, `Key_Right`, `Key_Down`, `Key_Home`, `Key_End`, `Key_PageUp`, `Key_PageDown`, `Key_Backspace` and `Key_Delete`, and `a` to `z` to `Qt::Key_A` to `Qt::Key_Z`.

Known Qt limit (H1 finding 4): `QLineEdit` and Qt Quick `TextInput` hard-code Backspace and Ctrl+Backspace after the `StandardKey` checks. Blocking these two keys does not stop them in those widgets. The POC tests avoid relying on it.

### GTK patch rules

The patch adds `gtk/gtktextinputflow.c` and `gtk/gtktextinputflowprivate.h`. They are compiled only with `-Dtextinputflow=enabled`, which links `dependency('textinputflow')`. With the option disabled, the patched GTK must build and behave exactly as stock GTK.

The private API:

```c
void gtk_text_input_flow_attach (GtkWidget *widget);
```

- On the first call in a process, it creates one `tif_ctx` with `tif_new()` and one `GListStore` of `GtkShortcut`, built from the merged table. If `tif_fd()` is not `-1`, it adds a `g_unix_fd_add()` watch. On readiness, it calls `tif_refresh()`. If that returns `true`, it rebuilds the whole store with one `g_list_store_splice()`.
- Every call creates a `GtkShortcutController` with `gtk_shortcut_controller_new_for_model()` on that shared store, sets its static name to `gtk-textinputflow`, keeps the default `GTK_PHASE_BUBBLE`, and adds it to `widget`.
- `gtk_text_init` calls it just before `priv->key_controller = gtk_event_controller_key_new ();` (`gtktext.c:2103` in 4.24.1).
- `gtk_text_view_init` calls it just before `priv->key_controller = gtk_event_controller_key_new ();` (`gtktextview.c:2078` in 4.24.1).
- Resulting order. `GtkText`: the input method controller runs first because it uses `GTK_PHASE_TARGET`. `GtkTextView`: GTK runs later-added controllers first, so the input method (added after ours) runs before ours. Our controller runs before the built-in class bindings in both.

Each table entry becomes one `GtkShortcut`:

- Trigger: `gtk_keyval_trigger_new (keysym, mods)`, with Shift → `GDK_SHIFT_MASK`, Control → `GDK_CONTROL_MASK`, Alt → `GDK_ALT_MASK`, Super → `GDK_SUPER_MASK`.
- Action:

| Command | Action |
|---|---|
| `move <unit> <dir> [extend]` | `gtk_signal_action_new ("move-cursor")`, arguments `(iib)`: step, count, extend |
| `delete character backward` | `gtk_signal_action_new ("backspace")`, no arguments |
| `delete character forward` | `gtk_signal_action_new ("delete-from-cursor")`, arguments `(ii)`: `GTK_DELETE_CHARS`, `1` |
| `delete word backward` / `forward` | `gtk_signal_action_new ("delete-from-cursor")`, arguments `(ii)`: `GTK_DELETE_WORD_ENDS`, `-1` / `1` |
| `select-all` | `gtk_callback_action_new()` whose callback, for a `GtkText`, calls `gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1)`, and for a `GtkTextView`, emits its `select-all` signal with `TRUE`, as stock GTK's Ctrl+A does. It returns `TRUE`. Do not use the `selection.select-all` action: `GtkText` disables it until the cursor or selection changes, so after `gtk_editable_set_text` on an empty entry it can do nothing. |
| blocked key | `gtk_callback_action_new()` whose callback returns `TRUE` and does nothing |

  Move steps: character → `GTK_MOVEMENT_VISUAL_POSITIONS`, word → `GTK_MOVEMENT_WORDS`, line ends → `GTK_MOVEMENT_DISPLAY_LINE_ENDS`, document ends → `GTK_MOVEMENT_BUFFER_ENDS`. Count is the direction, `-1` or `1`.

  Character moves use `VISUAL_POSITIONS` because stock GTK binds Left and Right to it (`gtktext.c:1438`, `gtktextview.c:1703`), and the default bindings take over those keys in every field. `LOGICAL_POSITIONS` would reverse Left and Right in right-to-left text, and would lose GTK's edge behavior: the error bell and focus move in `GtkText` (`gtk_widget_keynav_failed`), and the focus move from a `GtkTextView` with a hidden cursor (`gtktextview.c:6518`).

These are the signals and actions GTK's own bindings use, so GTK's movement and deletion code runs unchanged. Both `delete-from-cursor` handlers delete the selection first when there is one (`gtktext.c:4189`, `gtktextview.c:7044`).

A key the library does not mention reaches the built-in class bindings as before. If the library has no bindings at all, GTK behaves as stock.

#### Beating app shortcuts (#20)

GTK 4 runs app shortcuts in a window controller named `gtk-application-shortcuts`, in `GTK_PHASE_CAPTURE`. `gtk_window_set_application()` adds it (`gtkwindow.c:2963` to `2967` in 4.24.1). A widget's controller cannot run before it, so the patch adds one more controller to the window:

```c
void gtk_text_input_flow_attach_window (GtkWindow *window);
```

- `gtk_window_set_application()` calls it on the line just after it adds the app shortcut controller. GTK runs later-added controllers first, so ours runs before the app's. It runs only for windows that have an application, because only they have app shortcuts.
- It adds a `GtkEventControllerKey` in `GTK_PHASE_CAPTURE`, static name `gtk-textinputflow-window`.
- On `key-pressed`, it returns `FALSE` (does nothing) unless all of these are true:
  1. The window's focus widget (`gtk_root_get_focus`) is a `GtkText` or a `GtkTextView`.
  2. That widget has no preedit text. The patch adds the private helpers `gtk_text_has_preedit (GtkText *)` and `gtk_text_view_has_preedit (GtkTextView *)`, which return `TRUE` when the widget's preedit string is not empty.
  3. `tif_lookup` gives `TIF_LOOKUP_COMMAND` for the key. A blocked or unbound key returns `FALSE`, so app shortcuts still run.
- When all are true, it activates the command's action from the table above on the focus widget with `gtk_shortcut_action_activate (action, GTK_SHORTCUT_ACTION_EXCLUSIVE, focus, args)`, and returns `TRUE`.
- It uses the same process-wide `tif_ctx` as `gtk_text_input_flow_attach`.

Not covered by tests in the POC: `Shift` plus a letter (`GtkKeyvalTrigger` matching for uppercase keyvals), and a real input method composing text.

## Ticket Breakdown

**Lanes:**

- First: #19
- After #19: #8, #10, #11
- After #8: #9
- After #11: #20
- After #10: #27
- After #9, #10 and #11: #12

#20, #27 and #12 are independent of each other.

### 0. #19: Land the library contract with a stub

**What:** Create the Meson project, the public header exactly as written in "Public header", and a stub library that always holds the fixed stub table. This lets the Qt plugin (#10) and the GTK patch (#11) build and test against the real header while the real library (#8, #9) is written. The library has no file parsing and no watching yet.
**Areas:** `meson.build`, `meson.options`, `LICENSE`, `include/textinputflow.h`, `lib/`, `tests/lib/`
**Depends on:** nothing
**Size:** S

**Acceptance criteria**

- [ ] `include/textinputflow.h` matches the "Public header" section byte for byte, apart from whitespace.
- [ ] `meson setup build && meson compile -C build && meson test -C build` passes with no warnings at `warning_level=2`.
- [ ] The build makes `libtextinputflow.so.0` with soname `libtextinputflow.so.0` (check with `readelf -d`).
- [ ] `PKG_CONFIG_PATH=build/meson-uninstalled pkg-config --cflags --libs textinputflow` prints flags that compile and link a one-line C program calling `tif_new()`.
- [ ] The header compiles in a C99 test file and in a C++17 test file. Both are part of `meson test`.
- [ ] A unit test checks the stub: `tif_binding_count` is 25. `Alt+Right` gives `MOVE`, `WORD`, `FORWARD`, `extend = false`. `Control+Right` gives `TIF_LOOKUP_BLOCKED`. `Control+Left` gives `MOVE`, `WORD`, `BACKWARD`. `Shift+Alt+Right` gives move word forward with `extend = true`. `tif_keys_for_command` for move word forward returns exactly the one key `Alt+Right`. `tif_fd` is `-1`. `tif_refresh` and `tif_reload` return `false`.
- [ ] A unit test checks the `NULL` contracts in the "Function contracts" table.
- [ ] `LICENSE` holds the MIT license text.

### 1. #8: Library core: parser, merging, lookup

**What:** Replace the stub table with real loading. Parse both bindings files with the format in "Bindings file format", merge them as in "Loading and merging", and answer lookups and reverse lookups from the result. Add the default distro file. File watching is ticket #9: here, `tif_fd` stays `-1` and `tif_refresh` returns `false`, but `tif_reload` works.
**Areas:** `lib/`, `data/bindings`, `data/meson.build`, `tests/lib/`
**Depends on:** contract (#19)
**Size:** M

**Acceptance criteria**

- [ ] `data/bindings` holds exactly the 21 lines in "Default distro file", and installs to `<datadir>/textinputflow/bindings`.
- [ ] `tif_new_for_paths("data/bindings", NULL)` gives 21 entries, and every line of the file looks up to its command.
- [ ] `tif_new()` with `XDG_DATA_DIRS=<dir A>:<dir B>` uses the first of the two folders that has `textinputflow/bindings`. With `XDG_DATA_DIRS` unset, it looks in `/usr/local/share`, then `/usr/share`. With `XDG_CONFIG_HOME` unset, the user file is `$HOME/.config/textinputflow/bindings`. Unit tests set `HOME` and the XDG variables to temp folders.
- [ ] A user file line overrides the same key from the distro file. A user `none` blocks a distro key. A key only in the user file is added.
- [ ] Each of these lines is skipped with one stderr line in the form `textinputflow: <path>:<n>: <reason>`, and the valid lines in the same file still load: unknown key name, uppercase letter key, unknown modifier, repeated modifier, missing `=`, unknown command, `extend` on a delete, trailing `# comment`, line over 1024 bytes, non-ASCII byte.
- [ ] Modifier names match in any case and any order (`ctrl+SHIFT+Left` equals `Shift+Control+Left`). `Ctrl` equals `Control`.
- [ ] A key repeated in one file: the later line wins, and one warning is printed.
- [ ] A missing file and an empty file each give zero entries and print nothing.
- [ ] `tif_lookup` with keysym `0x41` (`A`) and Control matches a `Control+a` binding. Extra modifier bits outside `TIF_MOD_MASK` do not stop a match.
- [ ] `tif_keys_for_command` returns every key bound to the command, the total count when `max` is smaller, and `0` for an invalid command such as `DELETE` with `extend = true`.
- [ ] `tif_reload` returns `true` after a file's content changes the table, and `false` after a change that leaves the table the same (for example, a new comment line).
- [ ] `tests/fixtures/stub/textinputflow/bindings` holds exactly the 25 lines in "Stub fixture (#8)". A unit test loads it with `tif_new_for_paths(NULL, <fixture>)` and checks every entry, as the #19 stub test does. A second check sets `XDG_CONFIG_HOME` and `XDG_DATA_DIRS` to `tests/fixtures/stub` and confirms `tif_new()` gives the same 25 entries.
- [ ] `meson test -C build` passes with no warnings at `warning_level=2`.

### 2. #9: Live reload: tif_fd and tif_refresh

**What:** Make binding changes reach running apps. Watch the bindings folders with inotify as in "Live reload". Expose the descriptor with `tif_fd` and handle events in `tif_refresh`. The adapters call `tif_refresh` from their own loops: Qt on every `keyBindings()` call, GTK from a GLib fd watch.
**Areas:** `lib/`, `tests/lib/`
**Depends on:** implementation (#8). Reload reuses #8's loading and merging, and its tests need real files.
**Size:** S

**Acceptance criteria**

- [ ] `tif_fd` returns a descriptor with `O_NONBLOCK` and `FD_CLOEXEC` set when inotify works.
- [ ] With no change pending, `tif_refresh` returns `false` within 1 ms.
- [ ] After the test rewrites the user file in place, the descriptor becomes readable within 1 s (checked with `poll`), and `tif_refresh` then returns `true` and the new binding looks up.
- [ ] After the test writes a new file and renames it over the user file, the same holds.
- [ ] After the test deletes the user file, `tif_refresh` returns `true`, and the user's keys are gone from the table.
- [ ] When the user folder does not exist at `tif_new()`, creating `textinputflow/bindings` later is picked up by `tif_refresh`.
- [ ] Creating an unrelated file in a watched folder makes `tif_refresh` return `false`.
- [ ] `meson test -C build` passes.

### 3. #10: Qt platform theme plugin

**What:** Build the `textinputflow` Qt platform theme plugin. It wraps the base theme as `spikes/h2/theme/tifwrap.cpp` does, and answers `keyBindings()` with the rules in "Qt plugin rules". It builds against the #19 stub, so it can be finished before the real library exists.
**Areas:** `adapters/qt/`, `meson.build` (the `qt` feature option)
**Depends on:** contract (#19)
**Size:** M

**Acceptance criteria**

- [ ] With `-Dqt=enabled`, Meson builds the plugin with the `qt6` module (`Core` and `Gui`, `private_headers: true`), using `moc` through the module. With `-Dqt=disabled`, nothing Qt-related builds.
- [ ] A theme dump, ported from `spikes/h2/themedump.cpp` into `adapters/qt/tests/`, gives the same output under `QT_QPA_PLATFORMTHEME=gtk3` and under `QT_QPA_PLATFORMTHEME=textinputflow`, except for `keys` lines. Run it in a headless sway as `spikes/h2/run.sh` does.
- [ ] Against the stub table, with text `alpha beta gamma` and the caret at 0, in both `QLineEdit` and `QPlainTextEdit` (run with `spikes/h6/run.sh`, `H6_ONLY=qt`, `H6_QT_THEME=textinputflow`, and `QT_PLUGIN_PATH` set to the plugin folder):
  - Alt+Right: caret 0 → 6
  - Ctrl+Right: caret stays at 0
  - Home, then Shift+Alt+Right: caret 6, selection 0–6
  - Home, then End: caret 16
- [ ] `keyBindings(QKeySequence::Copy)` through the plugin equals the base theme's answer, because the stub binds no Copy keys.
- [ ] With `TEXTINPUTFLOW_QT_BASE_THEME=nonexistent`, apps still start, and text keys still follow the stub table.
- [ ] App shortcuts follow "Text bindings and app shortcuts". Add `H6_QT_APP` to `spikes/h6/session.sh`, the same way `H6_GTK_APP` works, and a test app `adapters/qt/tests/qt_shortcut_app.py`. It is `spikes/h6/qt_app.py` plus two `QShortcut`s with `Qt::ApplicationShortcut` context, on Alt+Right and on Ctrl+Right, whose slot moves the caret to the end (16). Against the stub table, in both widgets, from caret 0:
  - Alt+Right (bound to a command): caret 6. The text binding beat the app shortcut.
  - Home, then Ctrl+Right (blocked): caret 16. The app shortcut ran.

  Other keys, such as Alt+BackSpace, may still lose to app shortcuts in this ticket. #27 fixes that.
- [ ] `adapters/qt/README.md` states how to build and turn on the plugin, and the Backspace limit from "Qt plugin rules".

### 4. #11: GTK 4 patch

**What:** Write the GTK 4.24.1 patch from "GTK patch rules", and a build script that applies it and builds GTK against this repo's library. `GtkText` (inside `GtkEntry`) and `GtkTextView` then take their text keys from the library. It builds against the #19 stub, so it can be finished before the real library exists.
**Areas:** `adapters/gtk4/textinputflow.patch`, `adapters/gtk4/build-gtk.sh`, `adapters/gtk4/README.md`
**Depends on:** contract (#19)
**Size:** L

**Acceptance criteria**

- [ ] `patch -p1 --dry-run < adapters/gtk4/textinputflow.patch` succeeds on an unpacked `gtk-4.24.1` release tarball (sha256 checked against GNOME's `.sha256sum` file).
- [ ] `adapters/gtk4/build-gtk.sh` builds the patched GTK with `-Dtextinputflow=enabled`, linking this repo's `libtextinputflow` through `PKG_CONFIG_PATH=<textinputflow build dir>/meson-uninstalled`. It removes Homebrew and `mise` entries from `PATH`, as `spikes/h5/build-gtk.sh` does.
- [ ] The same patched source builds with `-Dtextinputflow=disabled`. With that build, Ctrl+Right in the harness moves the caret 0 → 5, as stock GTK does.
- [ ] In the patch, `gtk_text_input_flow_attach` is called in `gtk_text_init` and in `gtk_text_view_init`, each on the line just before `priv->key_controller = gtk_event_controller_key_new ();`.
- [ ] Against the stub table, with text `alpha beta gamma` and the caret at 0, in both the entry and the text view (run with `spikes/h6/run.sh`, `H6_ONLY=gtk4`, and `H6_LD_LIBRARY_PATH` set to both the patched GTK's `gtk/` folder and this repo's library folder, `:`-separated):
  - Alt+Right: caret 0 → 5
  - Ctrl+Right: caret stays at 0
  - Home, then Shift+Alt+Right: caret 5, selection 0–5
  - Home, then End: caret 16
- [ ] With `H6_DUMP=1`, the log shows that the test apps loaded the patched `libgtk-4.so` and this repo's `libtextinputflow.so.0`.
- [ ] `adapters/gtk4/README.md` lists the needed packages (`meson`, `ninja`, `glib2-devel`), the build command, the `H6_LD_LIBRARY_PATH` trap (sway strips `LD_LIBRARY_PATH`), and the input method ordering from "GTK patch rules".

### 5. #12: End-to-end POC test

**What:** Prove the POC success criteria from `docs/poc-brief.md`. One bindings file controls caret keys the same way in Qt and GTK 4, and an edit applies to running apps. Copy the harness from `spikes/h6/` to `tests/harness/`, add a shell step and text reading, and write `tests/e2e/`, which builds everything, runs the steps against real files, and compares the results with an expected file.
**Areas:** `tests/harness/`, `tests/e2e/`
**Depends on:** implementation (#9), implementation (#10), implementation (#11). It needs real file loading, live reload and both adapters.
**Size:** M

The harness changes in `tests/harness/`:

- `probe.py` also returns `"text"`: the field's full text from `Atspi.Text.get_text(text, 0, -1)`.
- A step value starting with `@sh ` runs the rest with `bash -c`, then waits 1 s. Example: `reload:@sh cp "$E2E/user-v2" "$XDG_CONFIG_HOME/textinputflow/bindings"`.

The e2e setup:

- `XDG_DATA_DIRS` is `<temp folder>:/usr/local/share:/usr/share`, where the temp folder holds `textinputflow/bindings`, copied from `data/bindings`. Keep the system folders after it: see the warning in "Stub fixture (#8)".
- `XDG_CONFIG_HOME` points at a temp folder. Its `textinputflow/bindings` starts as `tests/e2e/user-v1`:

  ```
  Alt+Right       = move word forward
  Alt+Left        = move word backward
  Shift+Alt+Right = move word forward extend
  Alt+BackSpace   = delete word backward
  Control+Right   = none
  ```

- `tests/e2e/user-v2`:

  ```
  Alt+Right = move line-end
  ```

- Qt apps run with `QT_QPA_PLATFORMTHEME=textinputflow` and the plugin on `QT_PLUGIN_PATH`. GTK apps load the patched GTK and the library through `H6_LD_LIBRARY_PATH`.

**Acceptance criteria**

- [ ] `tests/e2e/run.sh` builds the library and the Qt plugin, builds or reuses the patched GTK, runs the steps below in all four widgets, compares the results with `tests/e2e/expected.jsonl`, prints each mismatch, and exits non-zero if there is any mismatch.
- [ ] The steps and expected results, with text `alpha beta gamma` and the caret at 0. GTK covers the entry and the text view, and Qt covers `QLineEdit` and `QPlainTextEdit`:

  | # | Step | GTK caret, selection | Qt caret, selection |
  |---|---|---|---|
  | 1 | start | 0 | 0 |
  | 2 | Alt+Right | 5 | 6 |
  | 3 | Alt+Right | 10 | 11 |
  | 4 | Alt+Left | 6 | 6 |
  | 5 | Ctrl+Right | 6 | 6 |
  | 6 | Home | 0 | 0 |
  | 7 | Shift+Alt+Right | 5, sel 0–5 | 6, sel 0–6 |
  | 8 | Home | 0 | 0 |
  | 9 | reload: copy `user-v2` over the user file | 0 | 0 |
  | 10 | Alt+Right | 16 | 16 |
  | 11 | Home | 0 | 0 |
  | 12 | Ctrl+Right | 5 | 6 |
  | 13 | reload: copy `user-v1` over the user file | 0 | 0 |
  | 14 | End, then Alt+BackSpace | text `alpha beta ` | text `alpha beta ` |

  Steps 2 and 12 differ between the toolkits by design (#15). Step 12 shows that removing the block brings back the distro binding.
- [ ] Two runs in a row both pass.
- [ ] `tests/README.md` explains how to run the test, the needed packages, and why steps 2, 3, 7 and 12 differ between the toolkits.
- [ ] `tests/harness/README.md` documents the `@sh` step and the `text` field.

### 6. #20: GTK 4: text bindings beat app shortcuts

**What:** In stock GTK 4, an app's own shortcut runs before a focused text field sees the key. Extend the GTK patch from #11 so that, in a focused `GtkText` or `GtkTextView`, a key bound to a command runs the command instead, as "Text bindings and app shortcuts" requires. This makes GTK match Qt. Follow "Beating app shortcuts (#20)" in "GTK patch rules".
**Areas:** `adapters/gtk4/textinputflow.patch`, `adapters/gtk4/tests/`, `adapters/gtk4/README.md`
**Depends on:** implementation (#11). It extends #11's patch and reuses its shared context and action table.
**Size:** M

**Acceptance criteria**

- [ ] In the patch, `gtk_text_input_flow_attach_window` is called in `gtk_window_set_application()` on the line just after `gtk_widget_add_controller (GTK_WIDGET (window), priv->application_shortcut_controller);`.
- [ ] The patch still applies to `gtk-4.24.1` with `patch -p1 --dry-run`, and builds with `-Dtextinputflow=enabled` and with `-Dtextinputflow=disabled`.
- [ ] A test app `adapters/gtk4/tests/gtk4_accel_app.py` is `spikes/h6/gtk4_app.py` plus one app action whose handler moves the caret to the end (16). It is bound with `Gtk.Application.set_accels_for_action` to `<Alt>Right` and `<Control>Right`. Against the stub table, with `H6_GTK_APP` set to it, in both the entry and the text view, from caret 0:
  - Alt+Right (bound to a command): caret 5. The text binding beat the app shortcut.
  - Home, then Ctrl+Right (blocked): caret 16. The app shortcut ran.
- [ ] The same test app with the `-Dtextinputflow=disabled` build: Alt+Right gives caret 16, so the app shortcut runs, as in stock GTK.
- [ ] `gtk_text_has_preedit` and `gtk_text_view_has_preedit` return `TRUE` only when the widget's preedit string is not empty. A comment in the patch states that the window controller must not take keys while preedit text shows, so that input methods keep working.
- [ ] `adapters/gtk4/README.md` describes the window controller and states that input method composition is not covered by an automated test.

### 7. #27: Qt: text bindings beat app shortcuts

**What:** Qt text widgets claim priority over app shortcuts (by accepting the `ShortcutOverride` event) only for a fixed list of `StandardKey`s, and the list differs per widget. So today an app's `QAction` or `QShortcut` beats some text bindings: Alt+BackSpace (delete word backward) loses in `QLineEdit` and `QPlainTextEdit`, and Shift+Ctrl+End loses in `QLineEdit`. Add an application-wide event filter to the Qt plugin from #10, so that in a focused text widget, every key the library binds to a command beats app shortcuts, as "Text bindings and app shortcuts" requires. This is the Qt twin of #20.
**Areas:** `adapters/qt/`, `adapters/qt/tests/`, `adapters/qt/README.md`
**Depends on:** implementation (#10). It extends #10's plugin and reuses its context and key conversion.
**Size:** M

**Design**

- The plugin installs one event filter on the application object (`QCoreApplication::instance()->installEventFilter`). Install it on the first `keyBindings()` call, because the application object may not exist yet when the theme is created.
- On `QEvent::ShortcutOverride` only, the filter acts when all of these are true:
  1. The receiver is the application's focus object, and it inherits one of `QLineEdit`, `QTextEdit`, `QPlainTextEdit`, `QQuickTextInput` or `QQuickTextEdit`. Check with `QObject::inherits()`, so the plugin does not link Qt Widgets or Qt Quick.
  2. The key, converted to a `tif_key` with the plugin's key table, gives `TIF_LOOKUP_COMMAND` from `tif_lookup`.
- Then it calls `event->accept()` and returns `false`, so Qt does not run the app shortcut, and the key press goes on to the text widget. The widget then runs the command through the `StandardKey` lists that the plugin already supplies.
- For a blocked or unbound key, or any other event, the filter does nothing and returns `false`, so app shortcuts still run.

**Acceptance criteria**

- [ ] A test app `adapters/qt/tests/qt_shortcut_app.py` (extend #10's) has `QShortcut`s with `Qt::ApplicationShortcut` context on Alt+Right, Ctrl+Right, Alt+BackSpace and Shift+Ctrl+End. Each one's slot sets the caret to position 3, which no text command produces from these start points.
- [ ] Against the stub table (with the fixture setup in "Stub fixture (#8)"), with text `alpha beta gamma`, in both `QLineEdit` and `QPlainTextEdit`:
  - from caret 0, Alt+Right: caret 6 (text binding won)
  - from caret 0, Ctrl+Right (blocked): caret 3 (app shortcut ran)
  - from caret 16, Alt+BackSpace: caret 11 (text binding won, the word `gamma` is deleted)
  - from caret 0, Shift+Ctrl+End: caret 16, selection 0 to 16 (text binding won)
- [ ] With `QT_QPA_PLATFORMTHEME` unset (plugin off), the same app gives caret 3 for Alt+BackSpace, which shows the test detects the problem.
- [ ] The filter acts only on `ShortcutOverride`. It does not change any other event, and it does nothing when the focus object is not one of the five text classes.
- [ ] `adapters/qt/README.md` describes the filter, and states that input method composition is not covered by an automated test.

## Test Strategy

- **Library unit tests (#19, #8, #9)** are plain C programs run by `meson test`. They create temp folders and files, and set `HOME` and the XDG variables. Parsing is where implementations most often differ, so every error case in #8 has its own test that checks both the skip and the stderr line. Capture stderr by redirecting `fd 2` to a temp file around the call.
- **Watching (#9)** is timing-sensitive. Tests wait with `poll()` on `tif_fd` with a 1 s timeout, never with a fixed sleep, and they test the rename-over case, because that is how most editors save.
- **Adapter checks (#10, #11)** run the existing harness against the stub table. The stub's non-default keys (Alt+arrows, blocked Ctrl+Right) prove the library is in control: a toolkit left at its native bindings fails those steps.
- **Qt wrapping (#10)** needs the theme dump comparison, because a missed forwarding function shows up only as a wrong font, icon or dialog, which no caret test catches.
- **GTK disabled build (#11, #20)** keeps the patch honest: with `-Dtextinputflow=disabled`, the patched source must act as stock GTK.
- **App shortcut tests (#10, #20)** use test apps whose own shortcut moves the caret to the end (16). The caret position shows which side won: the library's word move (5 or 6), or the app (16).
- **Input method composition** (#20's preedit check, #11's controller order) has no automated test, because the headless session has no input method. Check it by hand with a real input method before shipping beyond the POC.
- **End-to-end (#12)** is the POC's proof. It is the only test that combines real files, live reload, and both toolkits.
- **Harness traps** from the spikes apply to every GUI test: start AT-SPI by hand, keep one idle `wtype` running, use `/usr/bin/python3`, set `GSK_RENDERER=cairo`, set `QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1`, and pass library paths with `H6_LD_LIBRARY_PATH`. `spikes/h6/README.md` explains each.

