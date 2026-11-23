# Solitaire

This is Klondike solitaire in **C++20** with [raylib](https://www.raylib.com/). A pure C++20 core library (`solitaire_core`) holds the rules. The card art comes from the public domain [Public Domain Playing Cards](https://github.com/AustinGabriel/Public-Domain-and-CC0-Playing-Cards) deck. The game synthesizes all of its sounds in code.

## Highlights

- **Classic Klondike rules**: Build the four foundations up by suit from Ace to King. Build the seven columns down in alternate colours. Only a King can go on an empty column.
- **Draw 1 and Draw 3**: Draw 1 turns over one card at a time. Draw 3 turns over three, and only the top card can be played.
- **Drag and drop**: Drag a card or a run of cards to a new pile. A blue outline shows where the cards will land. An illegal drop slides the cards back.
- **One-click moves**: Click a card to send it to the best legal pile. A foundation comes first.
- **Undo**: Take back any number of draws and moves.
- **Hints**: A hint shows a useful move in gold, or points at the stock.
- **Auto-complete**: When every card is face up and the stock is empty, the game plays the remaining cards to the foundations.
- **Animations**: The game deals the cards one by one, flips cards as they turn over, and slides every move. A win sends every card into the air in a shower of confetti.
- **Standard scoring**: See [Points](#points).
- **Records**: The game saves games played, wins, best time, fewest moves, high score, and win streaks for each draw mode to `~/.solitaire.cfg`. It also saves the card back, the draw mode, and the sound setting.
- **Four card backs**: Click **BACK** or push `B` to change the design.

## Build in CLion

1. Open this folder in CLion.
2. Make sure that the computer has an internet connection. On the first load, CMake downloads raylib 5.5 with `FetchContent`.
3. Select the `solitaire` run configuration.
4. Push Run.

To run the core test suite, select the `solitaire_tests` run configuration and push Run.

## Build and Test from the Command Line

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/solitaire
```

Each build copies the `assets/` folder next to the executable. The game loads the card images from there.

## Controls

| Action | Mouse | Keyboard |
| --- | --- | --- |
| Move cards | Drag them to a pile | — |
| Send a card to its best pile | Click it | — |
| Draw from the stock | Click the stock | `Space` or `D` |
| Undo | Click **UNDO** | `U`, `Z`, or `Backspace` |
| Show a hint | Click **HINT** | `H` |
| Start a new game | Click **NEW GAME** | `N` or `F2` |
| Change the draw mode | Click **DRAW 1** or **DRAW 3** | `1` or `3` |
| Change the card back | Click **BACK** | `B` |
| Sound on or off | Click **SOUND** | `M` |
| Quit | Close the window | `Esc` |

A change of draw mode deals a new game. A game that you leave after one or more moves counts as a game played.

## Points

| Event | Points |
| --- | --- |
| A waste card goes to a column | +5 |
| A card goes to a foundation | +10 |
| A column card turns face up | +5 |
| A card comes back off a foundation | −15 |
| The waste turns back into the stock (Draw 1) | −100 |
| The waste turns back into the stock (Draw 3, after the third pass) | −20 |

The score never goes below zero.

## Project Layout

- `src/card.*`, `src/klondike.*`, `src/storage.*`: The pure C++20 core (`solitaire_core`). It has no raylib dependency.
- `src/layout.*`, `src/theme.h`, `src/textures.*`, `src/effects.*`, `src/audio.*`, `src/renderer.*`, `src/game.*`, `src/main.cpp`: The raylib table layout, the colours, the card images, the card animations and win celebration, the synthesized sound, the draw code, and the game loop.
- `assets/cards/`: The 52 card faces and 4 card backs, scaled down to 360x504 from the original 1500x2100 images. `assets/cards/LICENSE` holds the CC0 public domain dedication of the art.
- `tests/solitaire_tests.cpp`: The headless CTest suite. It tests the deal, Draw 1 and Draw 3, the turn of the waste back into the stock, the foundation and column rules, run moves, scoring, undo, one-click moves, hints, auto-complete, the win, 60 games of play that follow the hints, and the save file.

## Credits

The card art is from [Public Domain Playing Cards](https://github.com/AustinGabriel/Public-Domain-and-CC0-Playing-Cards) by AustinGabriel, released under CC0 1.0. Credit is not required, but this project gives it with thanks.
