# Meleys Sleep Timer native app

Nativni PocketBook aplikace pres InkView/PocketBook SDK.

Ucel: spustit casovac pro usinani u audioknihy a po zvolene dobe vypnout ctecku. Aplikace nabizi volby:

- 5 minut
- 10 minut
- 15 minut
- 20 minut
- 30 minut
- 45 minut
- 60 minut
- Cancel

Po zvoleni casu aplikace spusti samostatny proces na pozadi, ulozi jeho PID do `/mnt/ext1/system/config/meleys-sleep-timer.pid` a ukonci UI. Proces po uplynuti casu zkusi vypnout zarizeni prikazy `poweroff` / `shutdown`.

## Build

Je potreba PocketBook SDK s InkView knihovnou a ARM toolchain.

Priklad:

```sh
cd pocketbook-sleep-timer-native
SDK_ROOT=/path/to/SDK_6.3.0 ./build.sh
```

Vysledek:

```text
build/MeleysSleepTimer.app
```

Ten se kopiruje do slozky `applications` ve ctecce.

## Poznamka k InkPad 4

UI aplikace by melo jit postavit standardne pres SDK. Jedina nejistota je, zda firmware povoli bez root prav zavolat skutecne vypnuti zarizeni z aplikace. Pokud ne, aplikace bude casovac umet spustit, ale v logu se objevi, ze `poweroff` selhal:

```text
/mnt/ext1/system/config/meleys-sleep-timer.log
```

Pak bude potreba zjistit konkretni systemovy prikaz nebo sluzbu pro power-off na InkPad 4.

