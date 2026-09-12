# Astro Pi Mission Space Lab: Space Garden

## Výzkumná otázka

Jak stabilní jsou během části oběhu ISS teplota, vlhkost, tlak a světlo a jak
souvisí jejich změny s pohybem a orientací stanice?

Program každých 5 sekund měří:

- teplotu, vlhkost a tlak,
- RGB barvu a intenzitu světla,
- magnetické pole,
- zrychlení, rotaci a orientaci.

Výsledky ukládá do `space_garden.csv`. Osm minut sbírá data a celkový běh je
nastaven na devět minut, takže má rezervu před soutěžním limitem 10 minut.

Každých 12 sekund také vyfotí Zemi. Vysvětlitelná analýza barev odhadne podíl
moře, mraků a pevniny a zapíše jej do `earth_analysis.csv`. Program uchová
nejvýše 40 fotografií a vytvoří `earth_panorama.jpg` ze šesti nejlepších záběrů
a časový barevný pás `colours_of_earth.jpg`.

U každé fotografie také změří barevný rozsah jako průměr rozdílu mezi 2. a 98.
percentilem červeného, zeleného a modrého kanálu. Výsledek uloží do CSV, takže
lze po misi vybrat nejbarevnější původní JPEGy a provést analýzu jednotlivých
kanálů bez dalšího obrazového výstupu. Celkem program uchová nejvýše 42 obrázků,
všechny ve formátu JPEG, což je maximum povolené pravidly.

Ke každé fotografii uloží do `earth_analysis.csv` také zeměpisnou šířku a
délku aktuální polohy ISS. První a poslední souřadnice shrne v `result.txt`.

Nakonec vytvoří také anglický textový výstup `result.txt`. Ten komentuje
průměry, rozsahy a stabilitu měření a stručně shrne obsah snímků.

Z dvojic po sobě jdoucích fotografií vyhledá společné body metodou ORB a
odhadne rychlost ISS. Jednotlivé výpočty uloží do `iss_speed.csv`; medián
věrohodných výsledků zapíše také do `result.txt`. Převod používá orientační
GSD 126,48 metru na pixel, proto jde o odhad, ne přesnou telemetrii.

Barevné rozpoznání je pracovní hypotéza. Před odesláním je potřeba výsledky
porovnat s fotografiemi v Replay Toolu a doladit prahové hodnoty.

## Test

1. Otevři [Astro Pi Replay Online](https://rpf.io/replay).
2. Nahraj `main.py`.
3. Spusť test.
4. Ve výstupech otevři nebo stáhni `space_garden.csv`.

Před soutěžním odesláním musí tým doplnit vlastní hypotézu, popis vyhodnocení a
otestovat finální ZIP pomocí Replay Toolu.
