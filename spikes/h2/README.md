# Spike H2: a Qt theme plugin can wrap the gtk3 theme

**Result: yes.** A wrapper plugin loads Qt's `gtk3` theme, forwards every theme function to it, and replaces only the key bindings. Across 129 theme values, the only difference from plain `gtk3` is the key binding the wrapper meant to change. A repeat run was identical.

## How it works

`theme/tifwrap.cpp` (85 lines) does what Qt's own `xdgdesktopportal` theme does. It creates the base theme with `QPlatformThemeFactory::create()`, then forwards all 20 virtual `QPlatformTheme` functions to it. Only `keyBindings()` has its own answer. The spike binds word moves to Alt+Left and Alt+Right, as in H1.

The base theme comes from `TIF_BASE_THEME` (default `gtk3`), so the user sets `QT_QPA_PLATFORMTHEME=tifwrap`.

The Qt `gtk3` source never reaches back to the active theme (no casts of `platformTheme()`), so a wrapper cannot confuse it.

## Test

`run.sh` builds the wrapper and `themedump`, then runs both in a headless sway:

1. `themedump` prints every observable theme value: style, icon theme, color scheme, contrast, motion, all theme hints, all palettes, all fonts, dialog helpers, the menu bar, button texts, file icons, and some key bindings. It runs once with `gtk3` and once with `tifwrap`, and the two outputs are compared.
2. The H6 harness runs the H1 key steps in the Qt apps, through `tifwrap`.

```
./run.sh
```

### Theme values

The only difference:

```
< keys 32 Ctrl+Right      (gtk3)
> keys 32 Alt+Right       (tifwrap)
```

The values that prove the real `gtk3` theme was loaded are the same under both: GTK file, color and font dialogs (`QGtk3FileDialogHelper` and others), the `Yaru-magenta` icon theme, the `Adwaita Sans` font, and dark mode. Qt's plain fallback theme has none of these.

### Caret keys through the wrapper

The results match H1 for `QLineEdit` and `QPlainTextEdit`: Ctrl+Right no longer moves, Alt+Right moves 0 → 6 → 11, and Alt+Left moves back to 6.

## Findings for #10 and #7

1. **The `KeyboardScheme` hint passes through unchanged.** H1 flagged this as a risk. The wrapper forwards `themeHint()`, so text widgets behave as they do under `gtk3`.
2. **The plugin uses Qt's private API.** `QPlatformThemeFactory` and `QGuiApplicationPrivate` have no compatibility promise. The plugin must be rebuilt for each Qt release, as `qt6ct` already is. Omarchy would rebuild its package with each Qt update.
3. **The plugin needs its own setting for the base theme.** It replaces `QT_QPA_PLATFORMTHEME`, so the wrapped theme must come from elsewhere: an environment variable, as in this spike, or the bindings config.
4. **The plugin should fall back to the base theme's answer.** For any command the library does not bind, `keyBindings()` returns the base theme's keys, so Copy and Paste keep working.

## Not tested

- Live theme changes, for example switching dark mode while an app runs. All theme functions are forwarded, so it should work, but no test covers it.
- `QPlatformTheme::name()`. It is not virtual, so it reports the wrapper's name. Nothing in the tested paths reads it.
