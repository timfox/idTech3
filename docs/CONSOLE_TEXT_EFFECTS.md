# Console text effects

The client console accepts optional inline RuneScape Classic-style effects when
`con_textEffects 1` (the default).

Color tags use `@xxx@` and are consumed before the text is stored in console
history:

`red`, `dre`, `lre`, `ora`, `or1`, `or2`, `or3`, `yel`, `gr1`, `gre`, `gr2`,
`gr3`, `blu`, `cya`, `mag`, `bla`, `whi`, and `ran`.

`@ran@` cycles through the bright palette while the console redraws. Position
tags use `~ddd~`, where `ddd` is `000` through `999`; the value is an absolute
console character column and is clamped to the current console width.

Example:

```text
@red@Danger@whi@: @gre@online ~020~server status
```

Tags do not appear in stored history or word-wrap calculations. The console
input cursor adopts the active inline color while editing a message.
