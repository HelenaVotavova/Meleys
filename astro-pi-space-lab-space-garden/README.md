# Astro Pi Mission Space Lab: Space Garden

## Výzkumná otázka

Jak stabilní jsou během části oběhu ISS teplota, vlhkost, tlak a světlo a jak
souvisí jejich změny s pohybem a orientací stanice?

Program každých 5 sekund měří:

- teplotu, vlhkost a tlak,
- RGB barvu a intenzitu světla,
- magnetické pole,
- zrychlení, rotaci a orientaci.

Výsledky ukládá do `space_garden.csv`. Běh trvá 9 minut 30 sekund, takže má
rezervu před soutěžním limitem 10 minut.

## Test

1. Otevři [Astro Pi Replay Online](https://rpf.io/replay).
2. Nahraj `main.py`.
3. Spusť test.
4. Ve výstupech otevři nebo stáhni `space_garden.csv`.

Před soutěžním odesláním musí tým doplnit vlastní hypotézu, popis vyhodnocení a
otestovat finální ZIP pomocí Replay Toolu.
