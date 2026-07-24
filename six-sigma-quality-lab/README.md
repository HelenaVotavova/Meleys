# AI Six Sigma Quality Lab

Webova aplikace pro rychlou analyzu procesnich dat:

- SPC individual chart
- histogram
- Cp/Cpk
- PPM mimo specifikaci
- Pareto vad
- volitelny AI konzultant pres Hermes

## Spusteni

```sh
cd six-sigma-quality-lab
python3 server.py --host 0.0.0.0 --port 8090
```

## Vstupni data

CSV musi obsahovat sloupec `value`. Volitelne muze obsahovat `defect`.

```csv
value,defect
10.12,
9.88,scratch
10.03,
10.51,burr
```

Limity procesu se zadaji v UI jako LSL, target a USL.

