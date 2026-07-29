# Helcin ctenarsky denik

Nativni offline aplikace pro PocketBook InkPad. Eviduje knihy, e-knihy a
audioknihy. Umi rucne pridavat tituly, nacist metadata pouze z oznacenych
souboru a importovat nebo exportovat CSV.

## Soubory ve ctecce

- databaze: `/mnt/ext1/system/config/helcin-ctenarsky-denik.csv`
- CSV pro import: `/mnt/ext1/HelcinDenik-import.csv`
- CSV export: `/mnt/ext1/HelcinDenik-export.csv`

CSV pouziva strednik a hlavicku:

```text
typ;nazev;autor;interpret;stav;hodnoceni;postup;zacatek;dokonceni;poznamka;soubor
```

## Instalace

Stahnete artefakt z GitHub Actions a soubor `HelcinCtenarskyDenik.app`
zkopirujte do adresare `applications` ve ctecce.
