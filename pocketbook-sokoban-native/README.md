# Helcin Sokoban pro PocketBook

Nativni logicka hra Sokoban pro PocketBook InkPad.

## Ovládání

- klepnutí nad / pod / vlevo / vpravo od hráče posune hráče daným směrem
- šipky zařízení fungují také, pokud je čtečka posílá aplikaci
- po dokončení levelu se automaticky načte další
- klepnutí do horní části obrazovky otevře menu:
  - Undo
  - Restart levelu
  - Další level
  - Předchozí level
  - Zavřít

## Build

```sh
cd pocketbook-sokoban-native
SDK_ROOT=/path/to/SDK ./build.sh
```

Výsledek:

```text
build/HelcinSokoban.app
```

Soubor zkopíruj do složky `applications` ve čtečce.

