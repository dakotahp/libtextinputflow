# Spikes

Six early experiments tested the riskiest assumptions before the POC was built. Each folder keeps its `README.md`: the question, the result, the evidence (with source line references), and the traps found. The TRD and `AGENTS.md` refer to these findings.

The spike code was removed after the POC, because maintained versions now live elsewhere. The "Run it" sections in these READMEs refer to the old code. To see or run it, check out commit `88968c6`, the last commit that has it.

| Spike | Question | Result | Code now lives in |
|---|---|---|---|
| [h1](h1/README.md) | Does Qt read text keys from the platform theme? | Yes | `adapters/qt/` |
| [h2](h2/README.md) | Can a Qt theme plugin wrap the `gtk3` theme? | Yes | `adapters/qt/`, `adapters/qt/tests/themedump.cpp` |
| [h3](h3/README.md) | Does Qt's `StandardKey` cover the v0 commands? | Yes, except word-end moves | (no code) |
| [h4](h4/README.md) | Does a GTK 4 shortcut controller beat the built-in bindings? | Yes | `adapters/gtk4/textinputflow.patch` |
| [h5](h5/README.md) | Can a patched GTK 4 run without touching the system GTK? | Yes | `adapters/gtk4/build-gtk.sh` |
| [h6](h6/README.md) | Can a script send keys and read the caret in GTK and Qt? | Yes | `tests/harness/` |
