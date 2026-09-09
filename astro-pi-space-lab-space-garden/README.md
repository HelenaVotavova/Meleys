# Astro Pi Mission Space Lab: Space Garden

## Výzkumná otázka

Jak stabilní jsou během části oběhu ISS teplota, vlhkost, tlak a světlo a jak
souvisí jejich změny s pohybem a orientací stanice?

Program každých 5 sekund měří:

- teplotu, vlhkost a tlak,
- RGB barvu a intenzitu světla,
- magnetické pole,
- zrychlení, rotaci a orientaci.

Výsledky ukládá do `space_garden.csv`. Běh trvá 8 minut 30 sekund, takže má
rezervu před soutěžním limitem 10 minut.

Každých 30 sekund také vyfotí Zemi. Vysvětlitelná analýza barev odhadne podíl
moře, mraků a pevniny a zapíše jej do `earth_analysis.csv`. Program uchová
nejvýše 18 fotografií a vytvoří `earth_panorama.jpg` ze šesti nejlepších záběrů
a časový barevný pás `colours_of_earth.png`.

Nakonec vytvoří také anglický textový výstup `result.txt`. Ten komentuje
průměry, rozsahy a stabilitu měření a stručně shrne obsah snímků.

Barevné rozpoznání je pracovní hypotéza. Před odesláním je potřeba výsledky
porovnat s fotografiemi v Replay Toolu a doladit prahové hodnoty.

## Test

1. Otevři [Astro Pi Replay Online](https://rpf.io/replay).
2. Nahraj `main.py`.
3. Spusť test.
4. Ve výstupech otevři nebo stáhni `space_garden.csv`.

Před soutěžním odesláním musí tým doplnit vlastní hypotézu, popis vyhodnocení a
otestovat finální ZIP pomocí Replay Toolu.
