# Spike H4: a shortcut controller beats GTK 4's built-in bindings

**Result: yes.** A `GtkShortcutController` added to `GtkText` (inside `GtkEntry`) or `GtkTextView` runs before the widget's built-in key bindings. A controller fed from a `GListModel` picks up changes while the app runs. A repeat run was identical.

The test needs no GTK patch. The test app adds the controller that the patch would add.

## Why it works (GTK 4.24.1)

- Built-in bindings live in one shortcut controller per widget, named `gtk-widget-class-shortcuts`. `gtk_widget_init` creates it (`gtkwidget.c:2529`).
- `gtk_widget_add_controller()` prepends to the widget's list (`gtkwidget.c:12062`).
- Controllers in the same phase run in list order (`gtkwidget.c:4879`). So a controller added later runs earlier.
- Both are in the bubble phase by default, so ours handles the key first, and the built-in one never sees it.

## Test

`gtk4_controller_app.py` adds a shortcut controller built from a `Gio.ListStore`. Each shortcut emits the widget's own `move-cursor` signal, so GTK's movement code runs unchanged. On `SIGUSR1`, the app replaces the store's contents.

| Bindings | Before reload | After reload |
|---|---|---|
| Ctrl+Right | line end (overrides the built-in word move) | none of ours (built-in word move) |
| Alt+Right | word forward | line end |

`run.sh` runs `steps.txt` in the H6 harness. Text: `alpha beta gamma`.

| Step | Entry | Text view | Shows |
|---|---|---|---|
| Ctrl+Right | 0 → 16 | 0 → 16 | Our binding beats the built-in one (which gives 5) |
| Alt+Right | 0 → 5 | 0 → 5 | A new key works |
| reload | | | The model changes while the app runs |
| Ctrl+Right | 0 → 5 | 0 → 5 | The built-in binding is back |
| Alt+Right | 0 → 16 | 0 → 16 | The new binding is live |

```
./run.sh
```

## Findings for the GTK 4 patch (#11)

1. **Insert the controller before the input method controller, not at the end.** `GtkText` and `GtkTextView` create their key controller, which feeds the input method, in their own init (`gtktext.c:2103`, `gtktextview.c:2078`). That is after the built-in shortcuts, so today the input method sees keys first. A controller added after init, as in this test, runs before the input method. The patch should create it just before those lines. Then the order is: input method, our bindings, built-in bindings. This is the order the design asks for.
2. **The patch can be small.** One controller per widget, fed from one shared `GListModel` that the library adapter updates. Each item emits the existing `move-cursor`, `delete-from-cursor` or `select-all` signal. No movement code changes.
3. **Live reload is free.** Changing the model is enough. The widgets need no rebuild and no signal.
4. **For `GtkEntry`, the target is the inner `GtkText`.** `move-cursor` is a signal on `GtkText`, which `gtk_editable_get_delegate()` returns.

## Not tested

- Behavior with a real input method composing text. The headless session has none. Finding 1 comes from reading the source.
- App-level shortcuts (`gtk_application_set_accels_for_action`). These run at the window and may take keys before the text widget. They are a policy question for the TRD, not a patch question.
