# Spike H5: a patched GTK 4 runs without touching the system GTK

**Result: yes.** GTK 4.24.1 builds from source in about 2 minutes. Test apps load the built library only when asked, and the system GTK stays in use for everything else. A patch in the built library changes behavior in the test apps.

## Numbers (8 CPUs)

| | |
|---|---|
| Full build | 121 s |
| Rebuild after editing `gtktext.c` | about 4 s (recompile one file, relink) |
| Build folder | 244 MB |
| Source folder | 134 MB |

## Test

`run.sh` downloads the GTK tarball and checks its checksum, unpacks it, and applies `marker.patch`. Then it builds with `build-gtk.sh` and runs the H6 harness twice: once with the system GTK, once with the built one.

`marker.patch` is a one-line change: in `GtkText` only, Ctrl+Right moves to the line end instead of the word end. Without it, the built library acts exactly like the system one, and the test cannot tell which was loaded.

| | Loaded library | Entry (`GtkText`), Ctrl+Right | Text view, Ctrl+Right |
|---|---|---|---|
| System GTK | `/usr/lib/libgtk-4.so.1.2400.1` | 0 → 5 | 0 → 5 |
| Built GTK | `<build>/gtk/libgtk-4.so.1.2400.1` | 0 → **16** | 0 → 5 |

The text view does not use `GtkText`, so it is unchanged. That shows the patched code, not something else, made the difference.

```
./run.sh [work dir]
```

Needs: `meson`, `ninja`, `glib2-devel`, plus the H6 requirements. Everything else GTK needs is already installed with the system `gtk4` package.

## The build

`build-gtk.sh` builds only what the tests use: the Wayland backend, with no docs, introspection, Vulkan, media, printing, demos, or tests. Arch's recipe needs many more tools (`sassc`, `gi-docgen`, `gobject-introspection` and others). This build does not:

- `sassc`: the release tarball ships the theme CSS already built.
- `gobject-introspection`: Python apps use the system `Gtk-4.0` typelib. It works with the built library because the version is the same.
- `glslc` and `vulkan-headers`: only for the Vulkan renderer.

## Traps

- **sway strips `LD_LIBRARY_PATH`.** Arch's `sway` has the `cap_sys_nice` file capability. For such programs, glibc removes `LD_LIBRARY_PATH` from the environment, so the apps sway starts never see it. The H6 harness now takes `H6_LD_LIBRARY_PATH` and sets the real variable inside the session. The first run hit this: the apps loaded the system GTK, and the patch seemed to do nothing.
- **Homebrew tools on `PATH`.** On this machine, Homebrew's `python3`, `glib-mkenums`, `gdbus-codegen` and `glib-compile-resources` came before the system ones. `build-gtk.sh` removes Homebrew and `mise` from `PATH`.

## Findings for #11

1. **The dev loop is cheap.** A 4-second rebuild and `LD_LIBRARY_PATH` are enough. No container, VM, or system install is needed.
2. **The patch should be a patch file on a release tarball**, as `marker.patch` is. Omarchy can ship it the same way, in a GTK package build.
3. **The Python test apps need no rebuild**, because the system typelib works with a same-version built library. If the patch adds new public API, the tests must either build introspection or call the new API from C.
