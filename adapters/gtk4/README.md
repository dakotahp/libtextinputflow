# GTK 4 patch

This patch makes GTK 4 text fields follow the textinputflow bindings files. It changes `GtkText`, which is inside `GtkEntry`, and `GtkTextView`. It applies to the GTK 4.24.1 release tarball.

## Build

Needed packages: `meson`, `ninja`, `glib2-devel`. Everything else GTK needs comes with the system `gtk4` package.

First build this repo's library. Then build the patched GTK against it:

```
meson setup build
meson compile -C build
adapters/gtk4/build-gtk.sh ~/gtk-work build
```

The script downloads `gtk-4.24.1.tar.xz` from download.gnome.org into the work folder and checks it against GNOME's `.sha256sum` file. Then it unpacks it, applies `textinputflow.patch`, and builds it with `-Dtextinputflow=enabled`. It finds the library through `PKG_CONFIG_PATH=build/meson-uninstalled`. A full build takes about 2 minutes. After a change to the patch, the script swaps the old patch for the new one, so only the changed files compile again.

The script prints the folder that holds the built `libgtk-4.so.1`, for example `~/gtk-work/build-enabled/gtk`. Build output goes to stderr.

An optional third argument, `disabled`, builds the same patched source with `-Dtextinputflow=disabled` in `build-disabled/`. That build does not use the library and acts exactly as stock GTK.

The build has only the Wayland backend, and no docs, introspection, Vulkan, media or printing. Python apps still work, because they use the system `Gtk-4.0` typelib, which matches a 4.24.1 library.

The script removes Homebrew and `mise` entries from `PATH`. Their `python3` and glib tools break the GTK build.

## Use it

Nothing is installed. Point `LD_LIBRARY_PATH` at both the patched GTK and the library:

```
LD_LIBRARY_PATH=~/gtk-work/build-enabled/gtk:build/lib gtk4-demo
```

**Trap: sway strips `LD_LIBRARY_PATH`.** Arch's `sway` has a file capability, so glibc removes `LD_LIBRARY_PATH` before sway starts. The apps that sway starts never see it, and they load the system GTK. In the harness, use `H6_LD_LIBRARY_PATH` instead. The harness sets the real variable inside the session.

## What the patch does

The patch adds a GTK build option, `textinputflow`, which is `disabled` by default. With it enabled, GTK links `libtextinputflow` and compiles `gtk/gtktextinputflow.c`.

`GtkText` and `GtkTextView` each get one more shortcut controller, named `gtk-textinputflow`. All controllers share one list of shortcuts, built from the library's binding table:

- A key bound to a command emits the widget's own `move-cursor`, `backspace` or `delete-from-cursor` signal, with the same arguments as GTK's own bindings. So GTK's own movement and deletion code runs. Select all selects the whole text, as stock Ctrl+A does.
- A key bound to `none` does nothing.
- A key the bindings files do not name reaches GTK's built-in bindings, as in stock GTK.

When a bindings file changes, the library tells GTK, and GTK rebuilds the list. Open text fields use the new bindings at once.

### Order of the key handlers

A text field handles a key in this order:

1. The input method. While it composes text, it gets every key first.
2. The textinputflow bindings.
3. GTK's built-in bindings.

The patch adds its controller just before the line that creates the widget's input method key controller (`priv->key_controller = gtk_event_controller_key_new ();`), in both `gtk_text_init` and `gtk_text_view_init`. That gives the order above:

- `GtkText`: the input method controller runs in `GTK_PHASE_TARGET`, which comes before our controller's `GTK_PHASE_BUBBLE`.
- `GtkTextView`: both controllers run in `GTK_PHASE_BUBBLE`. GTK runs controllers that were added later first, so the input method controller, added after ours, runs first.
- Both: the built-in bindings controller is added when the widget is created, before ours, so ours runs first.

No automated test covers input method order, because the headless test session has no input method. Check it by hand with a real input method.

### App shortcuts

In stock GTK, an app shortcut runs before the focused text field sees the key. The window's shortcut manager runs app shortcuts from its own controller, `gtk-shortcut-manager-capture`, in the capture phase, before any controller on the text field. GTK adds that controller when it creates the window.

The patch adds one more controller to each window that has an application, named `gtk-textinputflow-window`. `gtk_window_set_application()` adds it just after the app shortcut controller. GTK runs later-added controllers first, and ours is added after the window was created, so it runs before the shortcut manager.

It takes a key only when all of these are true:

1. The window's focus widget is a `GtkText` or a `GtkTextView`.
2. That widget shows no preedit text. While an input method composes text, it must get every key first.
3. The bindings files bind the key to a command.

Then it runs the command on the focus widget, with the same action as the widget's own controller, and the app shortcut does not run. A key bound to `none`, or not named in the bindings files, goes on to the app shortcut, as in stock GTK.

No automated test covers input method composition with this controller, because the headless test session has no input method. Check it by hand with a real input method.

## Checks

`adapters/gtk4/tests/check-keys.sh <work dir> <library build dir>` builds GTK with the option enabled and disabled, then sends keys to `GtkEntry` and `GtkTextView` through `tests/harness/run.sh`. It uses the bindings in `tests/fixtures/stub`.

- Enabled build: the caret results must match `tests/keys.expected`. Alt+Right moves a word, and the blocked Ctrl+Right does nothing, so the library is in control.
- Disabled build: Ctrl+Right must move 0 → 5, as in stock GTK (`tests/stock.expected`).
- Both: the apps must load the GTK that was just built. With the enabled build they must also load this repo's `libtextinputflow.so.0`, and with the disabled build they must not.
- App shortcuts: `tests/gtk4_accel_app.py` binds an app action to Alt+Right and Ctrl+Right. The action moves the caret to the end (16). With the enabled build, Alt+Right moves a word (caret 5), because the text binding wins, and the blocked Ctrl+Right runs the app action (caret 16). With the disabled build, Alt+Right runs the app action (caret 16), as in stock GTK. The results must match `tests/accel.expected` and `tests/accel-stock.expected`.

The check starts a headless sway with a private D-Bus session, so it does not work inside a sandbox. Read `tests/harness/README.md` first.
