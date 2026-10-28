# Spike H3: Qt StandardKey covers the v0 vocabulary

**Result: yes, with one gap in meaning.** Every v0 command has a `QKeySequence::StandardKey`. But Qt's word move goes to the start of the next word, and Qt has no "word end" move. GTK is the opposite. See finding 1.

Source: Qt 6.12.0 `qkeysequence.h` and the four text widgets listed in `spikes/h1/README.md`.

## Mapping

| v0 command | Qt `StandardKey` | Qt default on Linux |
|---|---|---|
| `move character backward` / `forward` | `MoveToPreviousChar` / `MoveToNextChar` | Left / Right |
| `move word backward` / `forward` | `MoveToPreviousWord` / `MoveToNextWord` | Ctrl+Left / Ctrl+Right |
| `move line-start` / `line-end` | `MoveToStartOfLine` / `MoveToEndOfLine` | Home / End |
| `move document-start` / `document-end` | `MoveToStartOfDocument` / `MoveToEndOfDocument` | Ctrl+Home / Ctrl+End |
| any move above, plus `extend` | the matching `Select*` key, for example `SelectNextWord` | the same key plus Shift |
| `delete character backward` | `Backspace` | Backspace |
| `delete character forward` | `Delete` | Delete |
| `delete word backward` / `forward` | `DeleteStartOfWord` / `DeleteEndOfWord` | Ctrl+Backspace / Ctrl+Delete |
| `select-all` | `SelectAll` | Ctrl+A |

`StandardKey` also has commands outside v0: line up and down, page up and down, paragraph (`Block`) start and end, `DeleteEndOfLine`, and `DeleteCompleteLine`.

## Findings

1. **Word moves differ, and neither toolkit can copy the other.** Qt's `MoveToNextWord` goes to the start of the next word (0 → 6 → 11 in H6). GTK's `GTK_MOVEMENT_WORDS` goes to the end of the word (0 → 5 → 10). Qt has no "word end" `StandardKey`, and GTK has no "next word start" movement. Matching them needs a patch on one side. The GTK 4 patch (#11) is already planned, so adding a "next word start" move there is the cheaper option. The TRD (#7) must decide.
2. **Qt moves follow the paragraph direction.** In right-to-left text, Qt swaps "next" and "previous". This fits v0, which uses text order only and leaves mixed-direction text out of scope.
3. **Not every widget checks every key.** `QLineEdit` and Qt Quick `TextInput` do not check the `Backspace` `StandardKey`; they hard-code the Backspace key. So "delete character backward" cannot move to another key in those two widgets. This does not matter for v0, which keeps Backspace.
4. **No transpose, kill, or yank.** These are already out of scope for the POC.
