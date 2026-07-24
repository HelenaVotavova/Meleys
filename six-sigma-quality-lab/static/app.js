const $ = id => document.getElementById(id);
let lastAnalysis = null;

function rndNormal(mean, sd) {
  let u = 0, v = 0;
  while (u === 0) u = Math.random();
  while (v === 0) v = Math.random();
  return mean + sd * Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v);
}

function demoCsv() {
  const defects = ["", "", "", "", "scratch", "burr", "wrong setup", "tool wear", "surface mark"];
  const rows = ["value,defect"];
  for (let i = 0; i < 120; i++) {
    const drift = i > 78 ? 0.10 : 0;
    const value = rndNormal(10.01 + drift, 0.13);
    const defect = Math.random() < 0.18 ? defects[Math.floor(Math.random() * defects.length)] : "";
    rows.push(`${value.toFixed(3)},${defect}`);
  }
  return rows.join("\n");
}

function parseCsv(text) {
  const lines = text.trim().split(/\r?\n/).filter(Boolean);
  if (!lines.length) return [];
  const header = lines[0].split(",").map(s => s.trim().toLowerCase());
  const valueIdx = header.indexOf("value");
  const defectIdx = header.indexOf("defect");
  if (valueIdx < 0) throw new Error("CSV musí obsahovat sloupec value.");
  return lines.slice(1).map((line, i) => {
    const cells = line.split(",").map(s => s.trim());
    const value = Number(cells[valueIdx]);
    if (!Number.isFinite(value)) return null;
    return { index: i + 1, value, defect: defectIdx >= 0 ? cells[defectIdx] : "" };
  }).filter(Boolean);
}

function mean(xs) { return xs.reduce((a, b) => a + b, 0) / xs.length; }
function sampleSd(xs, m = mean(xs)) {
  if (xs.length < 2) return 0;
  return Math.sqrt(xs.reduce((a, x) => a + (x - m) ** 2, 0) / (xs.length - 1));
}
function fmt(x, digits = 2) { return Number.isFinite(x) ? x.toFixed(digits) : "-"; }

function analyze() {
  const rows = parseCsv($("csvInput").value);
  const values = rows.map(r => r.value);
  if (values.length < 2) throw new Error("Zadej aspoň dvě hodnoty.");
  const lsl = Number($("lsl").value);
  const usl = Number($("usl").value);
  const target = Number($("target").value);
  const m = mean(values);
  const sd = sampleSd(values, m);
  const ucl = m + 3 * sd;
  const lcl = m - 3 * sd;
  const cp = sd > 0 ? (usl - lsl) / (6 * sd) : NaN;
  const cpk = sd > 0 ? Math.min((usl - m) / (3 * sd), (m - lsl) / (3 * sd)) : NaN;
  const outside = rows.filter(r => r.value < lsl || r.value > usl).length;
  const ppm = outside / rows.length * 1_000_000;
  const spcSignals = rows.filter(r => r.value > ucl || r.value < lcl).length;
  const defects = {};
  rows.forEach(r => { if (r.defect) defects[r.defect] = (defects[r.defect] || 0) + 1; });
  const pareto = Object.entries(defects).map(([name, count]) => ({ name, count })).sort((a, b) => b.count - a.count);

  const metrics = { process: $("processName").value, n: rows.length, lsl, target, usl, mean: m, sigma: sd, lcl, ucl, cp, cpk, ppm, outside, spcSignals };
  lastAnalysis = { rows, values, metrics, pareto };
  render(metrics, rows, pareto);
}

function render(k, rows, pareto) {
  $("kN").textContent = k.n;
  $("kMean").textContent = fmt(k.mean, 3);
  $("kSigma").textContent = fmt(k.sigma, 3);
  $("kCp").textContent = fmt(k.cp, 2);
  $("kCpk").textContent = fmt(k.cpk, 2);
  $("kPpm").textContent = fmt(k.ppm, 0);

  const status = $("spcStatus");
  const cpkClass = k.cpk >= 1.33 ? "good" : k.cpk >= 1 ? "warn" : "bad";
  status.className = cpkClass;
  status.textContent = k.spcSignals ? `${k.spcSignals} SPC signálů` : "bez bodů mimo 3 sigma";

  drawSpc($("spcChart"), rows, k);
  drawHistogram($("histChart"), rows.map(r => r.value), k);
  drawPareto($("paretoChart"), pareto);
}

function chartFrame(canvas) {
  const ctx = canvas.getContext("2d");
  const w = canvas.width, h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = "#fff";
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = "#d7dee8";
  ctx.lineWidth = 1;
  ctx.strokeRect(48, 20, w - 70, h - 58);
  return { ctx, w, h, left: 48, top: 20, right: w - 22, bottom: h - 38 };
}

function drawLine(ctx, x1, y1, x2, y2, color, width = 1) {
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(x2, y2);
  ctx.stroke();
}

function drawSpc(canvas, rows, k) {
  const c = chartFrame(canvas), { ctx, left, right, top, bottom } = c;
  const vals = rows.map(r => r.value);
  const minY = Math.min(...vals, k.lsl, k.lcl);
  const maxY = Math.max(...vals, k.usl, k.ucl);
  const pad = (maxY - minY) * 0.10 || 1;
  const yMin = minY - pad, yMax = maxY + pad;
  const x = i => left + i / Math.max(1, rows.length - 1) * (right - left);
  const y = v => bottom - (v - yMin) / (yMax - yMin) * (bottom - top);

  [
    [k.mean, "#1d4ed8", "mean"],
    [k.ucl, "#b7791f", "UCL"],
    [k.lcl, "#b7791f", "LCL"],
    [k.usl, "#b42318", "USL"],
    [k.lsl, "#b42318", "LSL"]
  ].forEach(([v, color, label]) => {
    const yy = y(v);
    drawLine(ctx, left, yy, right, yy, color, 1.2);
    ctx.fillStyle = color;
    ctx.font = "12px sans-serif";
    ctx.fillText(label, right - 36, yy - 4);
  });

  ctx.strokeStyle = "#0f766e";
  ctx.lineWidth = 2;
  ctx.beginPath();
  rows.forEach((r, i) => i ? ctx.lineTo(x(i), y(r.value)) : ctx.moveTo(x(i), y(r.value)));
  ctx.stroke();

  rows.forEach((r, i) => {
    ctx.fillStyle = (r.value > k.usl || r.value < k.lsl || r.value > k.ucl || r.value < k.lcl) ? "#b42318" : "#162033";
    ctx.beginPath();
    ctx.arc(x(i), y(r.value), 2.6, 0, Math.PI * 2);
    ctx.fill();
  });
}

function drawHistogram(canvas, values, k) {
  const c = chartFrame(canvas), { ctx, left, right, top, bottom } = c;
  const bins = 12;
  const min = Math.min(...values, k.lsl), max = Math.max(...values, k.usl);
  const step = (max - min) / bins || 1;
  const counts = Array(bins).fill(0);
  values.forEach(v => counts[Math.min(bins - 1, Math.max(0, Math.floor((v - min) / step)))]++);
  const maxC = Math.max(...counts, 1);
  const bw = (right - left) / bins;
  counts.forEach((n, i) => {
    const h = n / maxC * (bottom - top);
    ctx.fillStyle = "#0f766e";
    ctx.fillRect(left + i * bw + 2, bottom - h, bw - 4, h);
  });
}

function drawPareto(canvas, pareto) {
  const c = chartFrame(canvas), { ctx, left, right, top, bottom } = c;
  const data = pareto.slice(0, 8);
  if (!data.length) {
    ctx.fillStyle = "#657184";
    ctx.font = "16px sans-serif";
    ctx.fillText("Žádné vady v CSV.", left + 12, top + 34);
    return;
  }
  const maxC = Math.max(...data.map(d => d.count), 1);
  const bw = (right - left) / data.length;
  data.forEach((d, i) => {
    const h = d.count / maxC * (bottom - top);
    ctx.fillStyle = i === 0 ? "#b42318" : "#1d4ed8";
    ctx.fillRect(left + i * bw + 5, bottom - h, bw - 10, h);
    ctx.save();
    ctx.translate(left + i * bw + bw / 2, bottom + 5);
    ctx.rotate(-Math.PI / 4);
    ctx.fillStyle = "#657184";
    ctx.font = "11px sans-serif";
    ctx.fillText(d.name.slice(0, 14), 0, 0);
    ctx.restore();
  });
}

async function consult() {
  if (!lastAnalysis) analyze();
  const btn = $("consultBtn");
  btn.disabled = true;
  $("consultant").textContent = "Hermes přemýšlí...";
  try {
    const r = await fetch("/api/consult", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        metrics: lastAnalysis.metrics,
        pareto: lastAnalysis.pareto,
        notes: $("processName").value
      })
    });
    const data = await r.json();
    $("consultant").textContent = data.text || "Bez odpovědi.";
  } catch (e) {
    $("consultant").textContent = "AI konzultace selhala: " + e.message;
  } finally {
    btn.disabled = false;
  }
}

$("demoBtn").onclick = () => { $("csvInput").value = demoCsv(); analyze(); };
$("analyzeBtn").onclick = () => { try { analyze(); } catch (e) { alert(e.message); } };
$("consultBtn").onclick = () => { consult(); };
$("csvInput").value = demoCsv();
analyze();

