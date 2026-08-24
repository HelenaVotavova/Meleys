# Helciny podcasty pro PocketBook InkPad 4

Nativni katalog pro poslech tri poradu: Karlikovy minecrafticke pohadky, Minecraft pribehy na dobrou noc a Same otazky. Epizody `BONUS` jsou u Karlikovych pohadek skryte. Obrazky se ukladaji jen do male lokalni cache. Vybrana epizoda se stahne do jedineho docasneho souboru a prehraje systemovym prehravacem; dalsi volba soubor prepise.

1. Z GitHub Actions stahnete artefakt `HelcinyPodcasty-pocketbook-app`.
2. `HelcinyPodcasty.app` zkopirujte do slozky `applications`.
3. Zapnete WiFi, spustte aplikaci a zvolte `OBNOVIT KATALOG`.

Aplikace pouziva vlastni ALSA prehravac. Vedle `HelcinyPodcasty.app` proto musi byt take
`HelcinyAlsaWorker.bin` a slozka `HelcinyAlsaWorker-libs` z Qt6 audio artefaktu.
Vybrana epizoda se uklada pouze jako jedna docasna lokalni kopie.
