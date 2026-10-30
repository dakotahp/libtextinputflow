# The bindings file format

This is the spec of the bindings file, version 0.1. A bindings file says what each text-editing key does. The library reads two of them and merges them. Every adapter (the Qt plugin, the GTK patch, an app that links the library) sees the same merged result.

For a short user guide, see "Change a key" in the [README](../README.md).

## Where the files are

| File | Path | Who writes it |
|---|---|---|
| Distro file | The first `<dir>/textinputflow/bindings` that exists, for each `<dir>` in `$XDG_DATA_DIRS`. If `XDG_DATA_DIRS` is unset or empty, the list is `/usr/local/share:/usr/share`. Relative folders in the list are skipped | The distro or the package. The default is [`data/bindings`](../data/bindings) |
| User file | `$XDG_CONFIG_HOME/textinputflow/bindings` if `XDG_CONFIG_HOME` is an absolute path, else `$HOME/.config/textinputflow/bindings` | The user |

Only one distro file is read: the first one found. Distro files in later folders are not merged in.

A missing file counts as an empty file. So does a path that is not a regular file. A file that exists but cannot be read also counts as empty.

## Merge rule

The library reads the distro file, then the user file. For each key, the user file's line wins. Keys that only one file names keep that file's line.

So a user file needs only the lines that change something. To switch off a key that the distro file binds, write `<key> = none` in the user file.

## Lines

The file is printable ASCII text. Lines end in LF; a CR before the LF is ignored.

Each line is one of:

- **Empty**, or only spaces and tabs.
- **A comment**: the first character after any spaces is `#`. A `#` anywhere else on a line is an error, so a comment cannot follow a binding on the same line.
- **A binding**: `<key> = <command>`. Spaces and tabs around `=` and at the ends of the line are optional.

A line may be at most 1024 bytes. A tab or a byte from 0x20 to 0x7e is allowed. Any other byte makes the line an error.

If one file names the same key twice, the later line wins.

### Errors

A line with an error is skipped. The rest of the file still loads. The library prints one warning per bad line to standard error:

```
textinputflow: /home/me/.config/textinputflow/bindings:3: unknown command 'move paragraph forward'
```

This also means a file written for a newer library still loads in an older one: the lines it does not know are skipped with a warning.

## Keys

A key is zero or more modifiers, then a key name, joined by `+`, with no spaces. Examples: `Left`, `Control+Left`, `Shift+Control+End`, `Control+a`.

**Modifiers** are `Shift`, `Control` (or `Ctrl`), `Alt` and `Super`. Case does not matter, and the order does not matter. Each modifier may appear once.

**Key names** are case-sensitive. They are the XKB keysym names:

| Name | Key |
|---|---|
| `a` to `z` | A letter key. Write it in lowercase; for the shifted letter, add `Shift` |
| `Left`, `Right`, `Up`, `Down` | Arrow keys |
| `Home`, `End`, `Page_Up`, `Page_Down` | Navigation keys |
| `BackSpace`, `Delete` | Delete keys |

An uppercase letter such as `A` is an error, so `Shift+a` and `A` cannot mean two different things.

## Commands

| Command | What it does |
|---|---|
| `move character backward` / `forward` | Move the caret one character |
| `move word backward` / `forward` | Move the caret one word |
| `move line-start` / `line-end` | Move the caret to the start or end of the line |
| `move document-start` / `document-end` | Move the caret to the start or end of the text |
| `delete character backward` / `forward` | Delete one character |
| `delete word backward` / `forward` | Delete one word |
| `select-all` | Select all the text |
| `none` | In a text field, the key does nothing |

Add `extend` to the end of any `move` command to extend the selection as the caret moves, for example `move word forward extend`. `extend` is not allowed on other commands.

Each toolkit runs the command with its own code. So where a "word" ends, and what a "line" is in wrapped text, follow the toolkit. Known differences are listed in the [README](../README.md#known-differences).

### What `none` means

A key set to `none` does nothing in a focused text field. It does not stop the app's own shortcut on that key: that shortcut still runs.

A key that no file names is not the same as `none`. An unnamed key keeps the toolkit's normal behavior.

### Not in this version

There is no command for these yet: delete to the start or end of the line, move by paragraph or page, move up or down a line, transpose, and kill and yank. A line that uses one is an error, so it is skipped.

## Live changes

The library watches both files, and the folders they are in, so an app sees a change without a restart. This includes creating, replacing, renaming and deleting a file. A file that is a symbolic link is read through the link, but a change to the link's target is not seen.
