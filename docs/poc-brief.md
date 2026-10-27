# libtextinputflow: proof of concept brief

## Goal

Prove that one shared library and one bindings file can control text caret moves in more than one Linux toolkit.

The long-term idea is the macOS model. Every app asks one library what a key means in a text field. The app then runs its own version of that command. Nothing outside the app moves the caret.

The long-term target is Omarchy. It polishes the Linux desktop and controls how some apps are built.

## What "proven" means

The POC passes when all of these are true:

1. One bindings file changes caret behavior the same way in a Qt app and a GTK 4 app.
2. An edit to the bindings file applies to running apps, with no restart.
3. An automated test sends keys and reads the caret position in both apps, and the results match.

The default bindings match what GTK and Qt already do. So the test uses a non-default binding, for example `Alt+Left = move word backward`. A changed result proves the library is in control.

## Decisions

| Topic | Decision | Why |
|---|---|---|
| Targets | Qt platform theme plugin and a patched GTK 4 | Fast to build. Qt tests the "no source change" path. GTK 4 is the toolkit with no lever today, so it is the hardest proof. |
| Later targets | Chromium (and so Electron), then Firefox | Chromium is the highest value for Omarchy, but one build takes hours and about 100 GB. |
| Default bindings | Keep Linux habits (Ctrl+arrow for words, Home/End for line ends) | The goal is consistency, not a new key layout. |
| Language | C99, with a C header as the stable contract | GTK (C) and the Qt plugin (C++) link it with no extra toolchain. The inside can move to Rust later without a change to consumers. |
| Command names | Neutral names: action, unit, direction, extend | GTK names carry GTK habits. Neutral names let any toolkit or app map them. |
| Name | `libtextinputflow`, C prefix `tif_` | |
| Upstream intent | Local use first, then an Omarchy proposal | Patches do not need upstream polish yet. |

## Out of scope for the POC

- Sandboxed apps (Flatpak, Snap) and portal delivery
- Terminals (VTE, Konsole)
- Multi-key sequences such as `Control+x Control+s`
- Kill and yank (the Emacs kill ring)
- Moving app shortcuts off Ctrl (the macOS-style modifier swap)
- Chromium, Electron and Firefox
- Visual (left/right) moves in mixed-direction text. The POC uses text order only.

## Environment

The POC targets the author's machine:

| Part | Version |
|---|---|
| Desktop | Omarchy on Hyprland (Wayland) |
| GTK 4 | 4.24.1 |
| Qt | 6.12.0, with `QT_QPA_PLATFORMTHEME=gtk3` |
| AT-SPI2 | 2.62.1 |
| Virtual keyboard | `wtype` 0.4 |

## v0 command vocabulary

One command is an action, a unit, a direction, and an extend flag.

| Command | Default key |
|---|---|
| `move character backward` / `forward` | Left / Right |
| `move word backward` / `forward` | Ctrl+Left / Ctrl+Right |
| `move line-start` / `line-end` | Home / End |
| `move document-start` / `document-end` | Ctrl+Home / Ctrl+End |
| Any move above, plus `extend` | The same key plus Shift |
| `delete character backward` / `forward` | Backspace / Delete |
| `delete word backward` / `forward` | Ctrl+Backspace / Ctrl+Delete |
| `select-all` | Ctrl+A |

Spike H3 may cut this list down to what Qt's `StandardKey` can express.

## v0 bindings file

- Distro file: `/usr/share/textinputflow/bindings`
- User file: `~/.config/textinputflow/bindings`
- The user file overrides the distro file key by key. The value `none` removes a binding.

```
Control+Left        = move word backward
Shift+Control+Left  = move word backward extend
Control+BackSpace   = delete word backward
Control+a           = select-all
```

Keys match by keysym after the keyboard layout applies, not by raw keycode.

## v0 API sketch

```c
int  tif_lookup(tif_ctx *ctx, uint32_t keysym, uint32_t mods,
                tif_field field, tif_command *out, int max_out);
void tif_watch(tif_ctx *ctx, void (*changed)(void *data), void *data);
```

The full API is frozen in the POC TRD, after the spikes.

## Hypotheses

Each spike tests one claim. A "no" changes the later work, so the spikes come first.

| ID | Claim | If false |
|---|---|---|
| H1 | Qt's `QKeySequence::keyBindings()` reads `QPlatformTheme::keyBindings()`. | The Qt path needs a Qt patch, not a plugin. |
| H2 | A custom Qt platform theme can wrap the `gtk3` theme and keep its other features. | Users lose GTK styling in Qt apps, or the plugin must copy it. |
| H3 | `QKeySequence::StandardKey` covers enough of the v0 vocabulary. | Qt supports a subset, or it needs a patch. |
| H4 | A `GtkShortcutController` added to `GtkText` and `GtkTextView` takes priority over their built-in class bindings. | The GTK 4 patch must replace the class bindings. That patch is larger. |
| H5 | A patched GTK 4 runs from its build folder against one test app, with no change to the system GTK. | The dev loop needs a container or a VM. |
| H6 | On Hyprland, `wtype` sends keys and AT-SPI2 reads the caret offset in GTK 4 and Qt apps. | No automated proof on Hyprland. Tests run by hand or on GNOME. |

Run H6 first. Every later test depends on it.

## Sequence

1. **Spikes:** H1 to H6. Each spike ends with a written result on its issue.
2. **Decision gate:** write the POC TRD from the spike results. It freezes the API and the per-target design, and it refines the build issues.
3. **Build:** the library, then file watching, then the Qt plugin and the GTK 4 patch in parallel, then the end-to-end test.

## Issues

| Issue | Work | Blocked by |
|---|---|---|
| #1 | Spike H1: Qt reads text keys from the platform theme | |
| #2 | Spike H2: Qt theme plugin wraps `gtk3` | |
| #3 | Spike H3: `StandardKey` covers the v0 vocabulary | |
| #4 | Spike H4: GTK 4 shortcut controller beats class bindings | |
| #5 | Spike H5: patched GTK 4 runs without system changes | |
| #6 | Spike H6: `wtype` and AT-SPI2 test harness on Hyprland | |
| #7 | Write the POC TRD (decision gate) | #1 to #6 |
| #8 | Library core: parser and `tif_lookup` | #7 |
| #9 | Live reload: `tif_watch` | #8 |
| #10 | Qt platform theme plugin | #7, #8 |
| #11 | GTK 4 patch | #7, #8 |
| #12 | End-to-end POC test | #10, #11 |

## Background

The design notes live in the author's Obsidian vault, under `1_Projects/Linux Text Caret Layer/`:

- `Linux Text Caret Layer`: the problem, the macOS model, and three possible approaches.
- `Design A - Shared Text Command Library`: the full design this POC tests.
