# Termetris

A classic Tetris game for the terminal, written in C with ncurses.

## Features

- 5 tetromino types (I, S, O, T, L) with color-coded blocks
- Hold piece (`c`)
- Hard drop (Space)
- Ghost placement preview
- Wall kick rotation
- Progressive speed and difficulty (level up every 10 lines)
- Scoring with multi-line bonuses
- Start menu with controls reference
- Terminal resize handling

## Dependencies

- `make`
- `ncurses` (with development headers)
- `pkg-config`

## Build

```
make
```

Run with `./termetris`. Optionally install to `/usr/local/bin` with `sudo make install`.

## Controls

| Key       | Action       |
|-----------|--------------|
| ← →       | Move         |
| ↓         | Soft drop    |
| Space     | Hard drop    |
| Z         | Rotate left  |
| X         | Rotate right |
| C         | Hold         |
| Q / Esc   | Quit         |

## Scoring

| Lines cleared | Points (× level) |
|---------------|------------------|
| 1             | 40               |
| 2             | 100              |
| 3             | 300              |
| 4             | 1200             |

Level increases every 10 lines. Speed maxes out at level 20.

## License

MIT
