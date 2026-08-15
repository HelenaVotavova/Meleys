# Helcino pocasi pro PocketBook

Nativni hodinova predpoved pro Brno z otevrenych dat CHMI ALADIN. Zobrazuje teplotu, srazky, vitr, narazy, ikony pocasi a doporuceni obleceni. Predpoved se obnovuje rucne pres WiFi a ulozi se pro offline prohlizeni.

1. Stahnete artefakt `HelcinoPocasi-pocketbook-app` z GitHub Actions.
2. Soubor `HelcinoPocasi.app` zkopirujte do `applications` v interni pameti ctecky.
3. Na ctecce zapnete WiFi, spustte aplikaci a klepnete na `OBNOVIT`.

Serverovy prevodnik stahuje oficialni ALADIN `CZ_1km`, vybere bod vzdaleny priblizne 0,4 km od stredu Brna a vytvori maly soubor vhodny pro ctecku. Doporuceni obleceni je lokalni pravidlova logika a nespotrebovava AI tokeny.
