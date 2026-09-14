const people = [
  [
    "Jessica Meir",
    "us",
    "USA",
    "49 let",
    "2026-02-14",
    "jessica",
    "nasa",
    "NASA",
  ],
  [
    "Jack Hathaway",
    "us",
    "USA",
    "přibl. 43 let",
    "2026-02-14",
    "jack",
    "nasa",
    "NASA",
  ],
  [
    "Sophie Adenot",
    "fr",
    "Francie",
    "44 let",
    "2026-02-14",
    "sophie",
    "esa",
    "ESA",
  ],
  [
    "Andrey Fedyaev",
    "ru",
    "Rusko",
    "45 let",
    "2026-02-14",
    "andrey",
    "roscosmos",
    "Roskosmos",
  ],
  ["Anil Menon", "us", "USA", "49 let", "2026-07-14", "anil", "nasa", "NASA"],
  [
    "Pyotr Dubrov",
    "ru",
    "Rusko",
    "48 let",
    "2026-07-14",
    "pyotr",
    "roscosmos",
    "Roskosmos",
  ],
  [
    "Anna Kikina",
    "ru",
    "Rusko",
    "42 let",
    "2026-07-14",
    "anna",
    "roscosmos",
    "Roskosmos",
  ],
];
const crew = document.querySelector("#crew");
for (const [
  name,
  flag,
  country,
  age,
  arrival,
  photo,
  agency,
  agencyName,
] of people) {
  const days = Math.floor(
    (Date.now() - new Date(arrival + "T00:00:00Z")) / 864e5,
  );
  crew.insertAdjacentHTML(
    "beforeend",
    `<article class="person"><div class="portrait"><img src="assets/crew/${photo}.webp" alt="${name}" loading="lazy"><span class="flag"><img src="assets/flags/${flag}.svg" alt="Vlajka: ${country}" title="${country}"></span><span class="agency"><img src="assets/logos/${agency}.svg" alt="${agencyName}" title="Vysílající organizace: ${agencyName}"></span></div><div class="person-info"><b>${name}</b><span>${country} · ${age}</span><span>na ISS ${days} dní</span></div></article>`,
  );
}
const map = L.map("map", { worldCopyJump: true, zoomControl: true }).setView(
  [20, 0],
  2,
);
L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
  attribution: "© OpenStreetMap",
  maxZoom: 8,
}).addTo(map);
let nightLayer;
function updateNightLayer() {
  if (nightLayer) map.removeLayer(nightLayer);
  if (L.terminator) {
    nightLayer = L.terminator({
      fillColor: "#071521",
      fillOpacity: 0.36,
      color: "#f0b34a",
      weight: 1,
    }).addTo(map);
  }
}
updateNightLayer();
setInterval(updateNightLayer, 60000);
const icon = L.divIcon({
  className: "",
  html: '<div style="background:#e24632;color:#fff;border:3px solid white;border-radius:50%;width:48px;height:48px;display:grid;place-items:center;font-weight:900;box-shadow:0 2px 8px #0007">ISS</div>',
  iconSize: [48, 48],
  iconAnchor: [24, 24],
});
const marker = L.marker([0, 0], { icon }).addTo(map),
  track = L.polyline([], { color: "#08736a", weight: 3 }).addTo(map),
  dayTrack = L.polyline([], {
    color: "#68777d",
    weight: 2,
    opacity: 0.65,
  }).addTo(map),
  forecast = L.polyline([], {
    color: "#d44835",
    weight: 3,
    dashArray: "8 7",
  }).addTo(map),
  cameraFootprint = L.circle([0, 0], {
    radius: 210000,
    color: "#e6a23c",
    weight: 2,
    dashArray: "5 5",
    fillColor: "#f2c46d",
    fillOpacity: 0.08,
  }).addTo(map);
let points = [],
  lastCameraUpdate = 0;
const saaOutline = Array.from({ length: 73 }, (_, index) => {
  const angle = (index / 72) * Math.PI * 2;
  return [-25 + 18 * Math.sin(angle), -45 + 38 * Math.cos(angle)];
});
L.polygon(saaOutline, {
  color: "#d44835",
  weight: 2,
  fillColor: "#d44835",
  fillOpacity: 0.12,
})
  .bindTooltip("Jihoatlantská anomálie (SAA)")
  .addTo(map);

async function updateCamera(d) {
  if (Date.now() - lastCameraUpdate < 300000) return;
  lastCameraUpdate = Date.now();
  const query = `lat=${d.latitude.toFixed(3)}&lon=${d.longitude.toFixed(3)}`;
  const image = document.querySelector("#camera-image");
  image.src = `/api/camera-view?${query}&t=${Date.now()}`;
  try {
    const response = await fetch(`/api/camera-analysis?${query}`);
    const result = await response.json();
    if (!response.ok) throw Error(result.error);
    document.querySelector("#cloud-percent").textContent = `${result.cloud} %`;
    document.querySelector("#land-percent").textContent = `${result.land} %`;
    document.querySelector("#water-percent").textContent = `${result.water} %`;
    document.querySelector("#camera-date").textContent =
      `mozaika NASA ${result.date}`;
    document.querySelector("#camera-position").textContent =
      `Střed záběru: ${d.latitude.toFixed(1)}°, ${d.longitude.toFixed(1)}°`;
  } catch (error) {
    lastCameraUpdate = 0;
    document.querySelector("#camera-date").textContent =
      "snímek dočasně nedostupný";
  }
}
async function update() {
  try {
    const r = await fetch("/api/position");
    const d = await r.json();
    if (!r.ok) throw Error(d.error);
    const p = [d.latitude, d.longitude];
    marker.setLatLng(p);
    cameraFootprint.setLatLng(p);
    points.push(p);
    if (points.length > 180) points.shift();
    track.setLatLngs(points);
    document.querySelector("#speed").textContent = Math.round(
      d.velocity,
    ).toLocaleString("cs-CZ");
    document.querySelector("#altitude").textContent = Math.round(d.altitude);
    document.querySelector("#coords").textContent =
      `${d.latitude.toFixed(2)}°, ${d.longitude.toFixed(2)}°`;
    document.querySelector("#light").textContent =
      d.visibility === "daylight" ? "den" : "noc";
    document.querySelector("#state").textContent =
      "Živá data · " + new Date(d.timestamp * 1000).toLocaleTimeString("cs-CZ");
    document.querySelector("#radiation-dose").textContent =
      d.radiation.toFixed(1);
    document.querySelector("#magnetic-field").textContent =
      d.magnetic.toFixed(1);
    document.querySelector("#radiation-state").textContent =
      d.saa > 0.38
        ? "Zvýšená zátěž: průlet oblastí SAA."
        : d.radiation > 16
          ? "Zvýšená geomagnetická šířka."
          : "Běžný odhad pro tuto část dráhy.";
    document.querySelector("#magnetic-state").textContent =
      `Magnetická šířka ${Math.abs(d.magnetic_latitude).toFixed(0)}° ${d.magnetic_latitude >= 0 ? "s." : "j."}`;
    updateCamera(d);
    if (d.heading != null) {
      document.querySelector("#station").style.transform =
        `rotate(${d.heading - 90}deg)`;
      document.querySelector("#heading").textContent =
        `kurz ${Math.round(d.heading)}°`;
    }
  } catch (e) {
    document.querySelector("#state").textContent =
      "Živá data dočasně nedostupná";
  }
}
update();
setInterval(update, 5000);

function segments(rows) {
  const out = [[]];
  for (const p of rows) {
    const point = [p.latitude, p.longitude],
      last = out[out.length - 1].at(-1);
    if (last && Math.abs(last[1] - point[1]) > 180) out.push([]);
    out.at(-1).push(point);
  }
  return out;
}
async function updateForecast() {
  try {
    const r = await fetch("/api/forecast");
    const rows = await r.json();
    if (r.ok) forecast.setLatLngs(segments(rows));
  } catch (e) {}
}
updateForecast();
setInterval(updateForecast, 300000);
async function updatePastTrack() {
  try {
    const r = await fetch("/api/past-track");
    const rows = await r.json();
    if (r.ok) dayTrack.setLatLngs(segments(rows));
  } catch (e) {}
}
updatePastTrack();
setInterval(updatePastTrack, 3600000);
async function updateOrbitEvents() {
  try {
    const response = await fetch("/api/orbit-events");
    const data = await response.json();
    if (!response.ok) throw Error(data.error);
    document.querySelector("#ascending-nodes").innerHTML = data.ascending_nodes
      .map((node) => {
        const time = new Date(node.timestamp * 1000).toLocaleString("cs-CZ", {
          day: "numeric",
          month: "numeric",
          hour: "2-digit",
          minute: "2-digit",
          second: "2-digit",
        });
        return `<span><b>${time}</b>${Math.abs(node.longitude).toFixed(1)}° ${node.longitude >= 0 ? "v. d." : "z. d."}</span>`;
      })
      .join("");
    document.querySelector("#nodes-updated").textContent =
      `výpočet ${new Date(data.generated * 1000).toLocaleTimeString("cs-CZ")}`;
  } catch (error) {
    document.querySelector("#ascending-nodes").textContent =
      "Predikci se nepodařilo načíst.";
  }
}
updateOrbitEvents();
setInterval(updateOrbitEvents, 1800000);

const chartOptions = (label, unit) => ({
  responsive: true,
  maintainAspectRatio: false,
  animation: false,
  plugins: {
    legend: { display: false },
    title: { display: true, text: label, align: "start" },
  },
  scales: {
    x: { ticks: { maxTicksLimit: 7 } },
    y: { title: { display: true, text: unit } },
  },
});
let altitudeChart, speedChart, radiationChart, magneticChart;
async function updateCharts() {
  try {
    const r = await fetch("/api/history");
    const rows = await r.json();
    const labels = rows.map((x) =>
      new Date(x.timestamp * 1000).toLocaleString("cs-CZ", {
        day: "numeric",
        month: "numeric",
        hour: "2-digit",
        minute: "2-digit",
      }),
    );
    const altitude = rows.map((x) => x.altitude),
      speed = rows.map((x) => x.velocity / 3600),
      radiation = rows.map((x) => x.radiation),
      magnetic = rows.map((x) => x.magnetic);
    if (!altitudeChart) {
      altitudeChart = new Chart(document.querySelector("#altitudeChart"), {
        type: "line",
        data: {
          labels,
          datasets: [
            {
              data: altitude,
              borderColor: "#08736a",
              backgroundColor: "#08736a22",
              fill: true,
              pointRadius: 0,
            },
          ],
        },
        options: chartOptions("Průměrná výška", "km"),
      });
      speedChart = new Chart(document.querySelector("#speedChart"), {
        type: "line",
        data: {
          labels,
          datasets: [{ data: speed, borderColor: "#d44835", pointRadius: 0 }],
        },
        options: chartOptions("Průměrná rychlost", "km/s"),
      });
      radiationChart = new Chart(document.querySelector("#radiationChart"), {
        type: "line",
        data: {
          labels,
          datasets: [
            {
              data: radiation,
              borderColor: "#d44835",
              backgroundColor: "#d4483522",
              fill: true,
              pointRadius: 0,
              spanGaps: true,
            },
          ],
        },
        options: chartOptions("Odhad absorbované dávky", "µGy/h"),
      });
      magneticChart = new Chart(document.querySelector("#magneticChart"), {
        type: "line",
        data: {
          labels,
          datasets: [
            {
              data: magnetic,
              borderColor: "#315f92",
              pointRadius: 0,
              spanGaps: true,
            },
          ],
        },
        options: chartOptions("Odhad magnetického pole", "µT"),
      });
    } else {
      for (const [chart, data] of [
        [altitudeChart, altitude],
        [speedChart, speed],
        [radiationChart, radiation],
        [magneticChart, magnetic],
      ]) {
        chart.data.labels = labels;
        chart.data.datasets[0].data = data;
        chart.update();
      }
    }
  } catch (e) {}
}
updateCharts();
setInterval(updateCharts, 60000);
