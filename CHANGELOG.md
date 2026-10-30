# Changelog

## 0.1.0

The first release. The proof of concept becomes a library that toolkits, apps and distros can build on.

### Library

- New: `tif_version()` gives the library version at run time. `TIF_API_VERSION` is now 1.
- The API, the soname `libtextinputflow.so.0`, and the compatibility rules are documented in [`docs/api.md`](docs/api.md).
- The bindings file format is specified in [`docs/bindings-format.md`](docs/bindings-format.md).

### Qt plugin

- `meson install` now installs the plugin in Qt's plugin folder. The `qt_plugin_dir` option overrides the folder.

### Packaging

- [`packaging/arch/PKGBUILD`](packaging/arch/PKGBUILD) builds two Arch packages: `libtextinputflow` (the library, header, pkg-config file and default bindings) and `libtextinputflow-qt6` (the Qt plugin, pinned to the Qt version it was built with).
