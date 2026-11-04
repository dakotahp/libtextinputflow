# libtextinputflow

One settings file for the keys you use to edit text, shared by every app on a Linux desktop.

This is a **proof of concept** at version 0.1.0. It works for two of the main families of Linux apps (GTK 4 and Qt).

## The problem

**On Linux, the keys that edit text do not work the same way in every app, and you cannot change them in one place.**

These are the keys that move the text cursor (the blinking line, also called the *caret*) or delete text, instead of typing a letter. For example:

- Ctrl + Right arrow jumps one word to the right.
- Home and End jump to the start and end of the line.
- Ctrl + Backspace deletes the whole previous word.

### Why Linux has this problem

Linux apps are built with different **toolkits**. A toolkit is the set of building blocks an app uses for its windows, buttons and text boxes. The two biggest are **GTK** (most GNOME apps) and **Qt** (most KDE apps). Browsers, terminals and code editors often bring their own.

Each toolkit has its own built-in list of text keys, and nothing makes the lists agree. So:

- **The same key does different things.** Put the cursor at the start of `alpha beta` and press Ctrl + Right. A GTK app stops at the end of `alpha`. A Qt app stops at the start of `beta`.
- **You cannot change the keys in one place.** GTK 4 apps do not let you change their text keys at all. Qt apps allow it only through an add-on. Every other toolkit has its own method, or none.

Because the keys already disagree, changing them per toolkit only adds more confusion. Consistency has to come first.

### How macOS avoids it

On macOS, one system file lists what these keys do, and every text box in every app reads that same file. So the keys work the same way everywhere, and you can change a key once and every app follows. This project brings that model to Linux.

## The idea

Copy what macOS does: **one file decides what the text keys mean, and every toolkit asks it.**

```
             ~/.config/textinputflow/bindings
             "Ctrl+Left = move word backward"
                            │
                            ▼
                  ┌───────────────────┐
                  │  libtextinputflow │   reads the file,
                  │  (small library)  │   answers "what does this key mean?"
                  └─────────┬─────────┘
               ┌────────────┴────────────┐
               ▼                         ▼
       GTK 4 apps                   Qt apps
       (small change to GTK)        (add-on, no change to Qt)
               │                         │
               ▼                         ▼
     GTK moves the cursor        Qt moves the cursor
     its own way                 its own way
```

There are three parts:

1. **The bindings file.** A plain text file with one line per key, such as `Control+Left = move word backward`. The system ships a default file. Your own file in `~/.config` overrides it, one key at a time.
2. **The library.** A small piece of code that reads the files and answers one question: "the user pressed this key in a text box, what should happen?" It also notices when you edit the file, so open apps pick up the change at once.
3. **A small connector for each toolkit.** When a key is pressed in a text box, the connector asks the library. Then the toolkit runs its *own* version of that action, for example its own "move one word back". The library never moves the cursor itself. Only the app knows exactly where the words and lines are on its screen.

### Two rules that keep it predictable

- **In a text box, the file wins.** If the file says what a key does, that beats both the toolkit's built-in key and the app's own shortcut on the same key. Keys the file does not mention behave as before.
- **`none` switches a key off in text boxes**, and leaves the app's own shortcut on that key working.

### Why this approach

Other approaches exist. For example, a background program could watch the keyboard and move the cursor for you, or the window manager could do it. Both reach into an app from outside, so they break easily. This approach works inside each app, the way macOS does, so the app's own cursor logic still does the moving.

## What works today

| | Status |
|---|---|
| Qt apps | Supported, through an add-on (a Qt "platform theme" plugin). Qt itself is not changed. Covers Qt Widgets and Qt Quick (QML) apps |
| GTK 4 apps | Supported, with a small patch to GTK |
| Live changes | An edit to the bindings file reaches open apps at once |
| App shortcuts | In a text box, the file's keys beat an app's own shortcuts, in both toolkits |
| Default keys | The normal Linux keys: Ctrl + arrows for words, Home and End for lines |
| Chromium, Electron, Firefox, terminals, GTK 3 apps | Not covered yet |

An automated test sends real key presses to GTK, Qt and Qt Quick (QML) text boxes and checks where the cursor lands. All 84 checks pass. Nothing touches your desktop while the test runs.

### Known differences

These come from the toolkits themselves, not from this project:

- **Word moves land one letter apart.** After Ctrl + Right, GTK stops at the end of a word and Qt at the start of the next one. Neither toolkit has the other's move. This is left for later ([#15](https://github.com/dakotahp/libtextinputflow/issues/15)).
- **Ctrl + End does nothing in Qt's single-line text boxes.** Qt has no such move there ([#35](https://github.com/dakotahp/libtextinputflow/issues/35)).

## Change a key

Create `~/.config/textinputflow/bindings` and add the lines you want to change:

```
# Alt + arrows move by word
Alt+Left  = move word backward
Alt+Right = move word forward

# Ctrl + Right does nothing in text boxes
Control+Right = none
```

Each line is `keys = action`. The default file, [`data/bindings`](data/bindings), lists every action with the normal Linux keys.

The actions are:

| Action | Example |
|---|---|
| `move character backward` / `forward` | Left / Right |
| `move word backward` / `forward` | Ctrl + Left / Right |
| `move line-start` / `line-end` | Home / End |
| `move document-start` / `document-end` | Ctrl + Home / End |
| add `extend` to any move to select text as you go | Shift + any of the above |
| `delete character backward` / `forward` | Backspace / Delete |
| `delete word backward` / `forward` | Ctrl + Backspace / Delete |
| `select-all` | Ctrl + A |

## Try it

There are no prebuilt packages, so you build from source either way. The steps assume Arch Linux. First get the code:

```sh
git clone https://github.com/dakotahp/libtextinputflow.git
cd libtextinputflow
```

For Qt apps, choose one way. Install the packages to use the add-on in your session, or run from a build folder to try it in one app. GTK 4 apps need a patched GTK in both cases.

### Install on Arch

[`packaging/arch/PKGBUILD`](packaging/arch/PKGBUILD) builds the library and the Qt add-on from the `v0.1.0` tag, then installs them as two packages:

```sh
cd packaging/arch && makepkg -si
```

Then turn the Qt add-on on for your session, for example with a file in `~/.config/environment.d/`:

```
QT_QPA_PLATFORMTHEME=textinputflow
```

The add-on keeps the look of the `gtk3` Qt theme. To wrap another theme, set `TEXTINPUTFLOW_QT_BASE_THEME` to its name.

### Run from a build folder

This installs nothing.

1. Build the library and the Qt add-on:

   ```sh
   meson setup build -Dqt=enabled
   meson compile -C build
   ```

2. Run a Qt app with the add-on:

   ```sh
   QT_PLUGIN_PATH=$PWD/build/adapters/qt QT_QPA_PLATFORMTHEME=textinputflow <your Qt app>
   ```

### GTK 4 apps

The packages do not include GTK. Build the patched GTK (about 2 minutes) and run a GTK app against it. [`adapters/gtk4/README.md`](adapters/gtk4/README.md) has the steps.

## For app builders

- [`docs/api.md`](docs/api.md): how to use the library from a toolkit or an app, the rules every adapter keeps, and the version promises.
- [`docs/bindings-format.md`](docs/bindings-format.md): the full spec of the bindings file.
- [`CHANGELOG.md`](CHANGELOG.md): what changed in each release.

## For contributors

- [`docs/poc-brief.md`](docs/poc-brief.md): the goals and the decisions behind them.
- [`docs/plans/2026-10-27-libtextinputflow-poc-trd.md`](docs/plans/2026-10-27-libtextinputflow-poc-trd.md): the full technical spec.
- [`AGENTS.md`](AGENTS.md): layout, build and test commands, and project rules.
- [`spikes/`](spikes/README.md): findings from the six early experiments that showed the idea could work.
- [`adapters/qt/README.md`](adapters/qt/README.md) and [`adapters/gtk4/README.md`](adapters/gtk4/README.md): how each toolkit connector works.
- [`tests/README.md`](tests/README.md): how the tests work.

## License

MIT. See [`LICENSE`](LICENSE).
