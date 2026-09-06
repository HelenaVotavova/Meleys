# Helciny podcasty pro PocketBook InkPad 4

Nativni katalog pro poslech dvou poradu: Karlikovy minecrafticke pohadky a Minecraft pribehy na dobrou noc. Epizody `BONUS` a `SOUTEZ` jsou u Karlikovych pohadek skryte. Obrazky se ukladaji do male lokalni cache. Tlacitko `STAHNOUT CHYBEJICI` ulozi vsechny dosud nestazene epizody vybraneho poradu. Nove dily se doplni az pri dalsim rucnim spusteni tohoto prikazu.

1. Z GitHub Actions stahnete artefakt `HelcinyPodcasty-pocketbook-app`.
2. `HelcinyPodcasty.app` zkopirujte do slozky `applications`.
3. Zapnete WiFi, spustte aplikaci a zvolte `OBNOVIT KATALOG`.

Aplikace pouziva vlastni ALSA prehravac. Vedle `HelcinyPodcasty.app` proto musi byt take
`HelcinyAlsaWorker.bin` a slozka `HelcinyAlsaWorker-libs` z Qt6 audio artefaktu.
Epizody se ukladaji samostatne do `/mnt/ext1/Podcasts/HelcinyPodcasty`. Autoplay
prechazi pouze mezi jiz ulozenymi dily a aplikace vzdy ponecha nejmene 200 MB volneho mista.
