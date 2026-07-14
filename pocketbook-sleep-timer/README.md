# PocketBook Sleep Timer

Sleep timer pro PocketBook InkPad 4 a podobne PocketBook ctecky.

Prvni verze je sada `.app` shell skriptu, ktere se na PocketBooku zobrazi v aplikacich. Po spusteni si vyberes pevny casovac, napr. 5, 10, 15, 20, 30, 45 nebo 60 minut. Skript pocka na pozadi a potom zkusi ctecku vypnout. Tim se zastavi i prehravana audiokniha.

## Instalace

1. Pripoj PocketBook k pocitaci pres USB.
2. Otevri slozku `applications` ve ctecce.
3. Zkopiruj do ni vsechny soubory ze slozky `applications/` v tomto projektu.
4. Bezpecne odpoj ctecku.
5. V PocketBooku otevri `Aplikace` / `User's` a spust pozadovany casovac.

## Casovace

- `Sleep Timer 5.app`
- `Sleep Timer 10.app`
- `Sleep Timer 15.app`
- `Sleep Timer 20.app`
- `Sleep Timer 30.app`
- `Sleep Timer 45.app`
- `Sleep Timer 60.app`
- `Sleep Timer Cancel.app`

## Jak to funguje

Kazdy casovac spusti na pozadi spolecny skript `sleep-timer-runner.app` s poctem minut. Runner si ulozi PID do `/mnt/ext1/system/config/meleys-sleep-timer.pid`, aby sel pozdeji zrusit pres `Sleep Timer Cancel.app`.

Po vyprseni casu zkusi postupne bezne prikazy dostupne na linuxovych PocketBoocich:

- `/sbin/poweroff`
- `/bin/poweroff`
- `poweroff`
- `/sbin/shutdown -h now`
- `shutdown -h now`

## Poznamky

PocketBook aplikace mohou byt normalni binarky pres SDK, ale i shell skripty s priponou `.app`. Tato varianta je proto nejrychlejsi na otestovani bez kompilace.

Pokud konkretni firmware InkPad 4 nepovoli vypnuti z uzivatelskeho `.app` skriptu, dalsi krok bude nativni aplikace pres PocketBook SDK nebo uprava prikazu podle realneho logu z pristroje.
