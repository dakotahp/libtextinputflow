# Using libtextinputflow in a toolkit or an app

This guide is for people who connect a text widget to the library: a toolkit adapter, or an app with its own text widget. The header is [`include/textinputflow.h`](../include/textinputflow.h). The file format is in [`bindings-format.md`](bindings-format.md).

Most apps do not need this. A Qt app gets the bindings from the Qt plugin, and a GTK 4 app from the GTK patch, with no code change. You need the library directly when your text widget handles keys itself, for example in a Rust UI toolkit, or when your app checks raw key codes instead of the toolkit's standard keys.

## Build against it

```sh
pkg-config --cflags --libs textinputflow
```

In meson: `dependency('textinputflow')`. The library is C99 with no dependencies beyond libc, so any language that calls C can use it.

## The model

The library answers two questions:

- **What does this key mean?** `tif_lookup(ctx, key, &command)`. Use it when a key press reaches your text widget.
- **Which keys mean this command?** `tif_keys_for_command(ctx, &command, keys, max)`, or walk every binding with `tif_binding_count` and `tif_binding_at`. Use it when your toolkit wants a key table up front, as Qt's `StandardKey` lists and GPUI's key bindings do.

Your adapter then runs the command with the widget's own code. The library never moves a caret itself.

## Calls

| Call | What it does |
|---|---|
| `tif_version()` | The library version as text, for example `"0.1.0"`. The string is static: do not free it |
| `tif_new()` | Read the distro and user files from their normal places, and start watching them. Returns `NULL` only if memory runs out |
| `tif_new_for_paths(distro, user)` | The same, with given file paths. Either may be `NULL`. Use it in tests |
| `tif_free(ctx)` | Free the context. `NULL` is allowed |
| `tif_lookup(ctx, key, &out)` | `TIF_LOOKUP_COMMAND` and the command, `TIF_LOOKUP_BLOCKED` for a `none` key, or `TIF_LOOKUP_UNBOUND`. `out` may be `NULL` |
| `tif_keys_for_command(ctx, &cmd, out, max)` | Writes up to `max` keys bound to `cmd`, and returns how many there are in all. Call with `max` 0 to get the count. Blocked keys are never returned |
| `tif_binding_count(ctx)`, `tif_binding_at(ctx, i, &out)` | Walk the merged table, blocked keys included |
| `tif_fd(ctx)` | A file descriptor that becomes readable when a bindings file may have changed. `-1` if live reload is not available |
| `tif_refresh(ctx)` | Call when `tif_fd` is readable. Reads the change and reloads. Returns `true` only if the merged table changed |
| `tif_reload(ctx)` | Reload now, without the file descriptor. Returns `true` if the table changed |

Every call accepts a `NULL` context and then does nothing.

### Keys

A `tif_key` is an XKB keysym and a set of `TIF_MOD_*` bits. Most Linux toolkits already have the keysym. Pass the modifiers that are held; the library ignores bits outside `TIF_MOD_MASK`. For a letter, pass the lowercase keysym and `TIF_MOD_SHIFT` when Shift is held. The library also accepts the uppercase keysym, which some toolkits report for a shifted letter.

### Commands

A `tif_command` is an action (`TIF_ACTION_MOVE`, `TIF_ACTION_DELETE`, `TIF_ACTION_SELECT_ALL`), a unit (character, word, line ends, document ends), a direction, and `extend`. Map each one to your widget's own command. For example, `{MOVE, WORD, FORWARD, extend=true}` is Qt's `SelectNextWord` and GPUI's `SelectToNextWordEnd`.

## Watch the files

Add `tif_fd(ctx)` to your event loop. When it becomes readable, call `tif_refresh(ctx)`. If it returns `true`, rebuild any key table you built from the library.

- GLib: `g_unix_fd_add(tif_fd(ctx), G_IO_IN, callback, ctx)`.
- Qt: a `QSocketNotifier` on `tif_fd(ctx)`.
- Rust: wrap the descriptor for your async runtime, for example `async_io::Async` or `tokio::io::unix::AsyncFd`.

If `tif_fd` returns `-1`, the bindings still work, but changes need a restart or a call to `tif_reload`.

## Rules an adapter must keep

These rules make every app behave the same. The Qt plugin and the GTK patch both keep them, and the end-to-end test checks them.

1. **In a focused, editable text field, a bound key runs its command.** It beats the widget's built-in binding and the app's own shortcut on that key.
2. **A `none` key does nothing in a text field**, and the app's shortcut on that key still runs.
3. **In read-only text, delete commands do nothing**, and the app's shortcut on that key still runs. Move and select commands still work, if the widget allows a caret.
4. **A key the files do not name keeps the toolkit's normal behavior.**
5. **Outside text fields, the library changes nothing.**
6. **While an input method is composing text, leave keys to the input method.**
7. **Skip what you do not know.** Treat a command with an action or unit your adapter does not know as unbound. Later versions add commands, and an adapter built today must keep working with them.

## Threads

A context has no locks. Use each context from one thread at a time. Separate contexts share nothing, so each thread may have its own.

## Versions and compatibility

The library uses semantic versions. `tif_version()` gives the version at run time.

- **Before 1.0**, a minor release (0.1 to 0.2) may change the API. Each change is listed in the [changelog](../CHANGELOG.md).
- **The ABI** is the binary interface. The soname is `libtextinputflow.so.0`. Any change that breaks programs built against an older release raises the soname number. Adding a function or an enum value does not break the ABI.
- **The public structs are part of the ABI.** `tif_key`, `tif_command` and `tif_binding` pass by value, so their fields do not change without a new soname.
- **`TIF_API_VERSION`** goes up by one each time the header adds something. Check it at compile time to use a newer call, for example `#if TIF_API_VERSION >= 1` for `tif_version()`.
- **The file format** keeps old files working: a new library reads every file an older one read. A line that a library does not know is skipped with a warning.

## Bindings for other languages

None ship yet. The API is small, so a binding is a thin layer: in Rust, a `-sys` crate generated from the header, linked through pkg-config, plus a safe wrapper that frees the context on drop. A binding must keep the rules above, and must not parse the files itself.
