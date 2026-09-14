const $ = (selector) => document.querySelector(selector);
const weatherNames = {
  0: "Jasno",
  1: "Převážně jasno",
  2: "Polojasno",
  3: "Zataženo",
  45: "Mlha",
  48: "Mrznoucí mlha",
  51: "Slabé mrholení",
  53: "Mrholení",
  55: "Silné mrholení",
  61: "Slabý déšť",
  63: "Déšť",
  65: "Silný déšť",
  71: "Slabé sněžení",
  73: "Sněžení",
  75: "Silné sněžení",
  80: "Přeháňky",
  81: "Dešťové přeháňky",
  82: "Silné přeháňky",
  95: "Bouřka",
};
function time(value) {
  return new Date(value).toLocaleTimeString("cs-CZ", {
    hour: "2-digit",
    minute: "2-digit",
  });
}
function aqiName(value) {
  if (value <= 20) return "Dobrá";
  if (value <= 40) return "Přijatelná";
  if (value <= 60) return "Zhoršená";
  if (value <= 80) return "Špatná";
  if (value <= 100) return "Velmi špatná";
  return "Extrémně špatná";
}
function renderOccupancy(rows) {
  const names = {
    "Bazény Lužánky": "Lužánky · bazény",
    "Bazény Lužánky – wellness": "Lužánky · sauna a wellness",
    "Lázně Rašínova – bazén": "Rašínova · bazén",
    "Lázně Rašínova – wellness": "Rašínova · sauna a wellness",
  };
  $("#occupancy").innerHTML = rows
    .map((row) => {
      const percent = Math.round((row.occupied / row.capacity) * 100);
      return `<article class="${percent >= 70 ? "busy" : ""}"><div class="occupancy-head"><h3>${names[row.name] || row.name}</h3><b>${row.occupied}/${row.capacity}</b></div><span>${percent} % obsazeno · ${row.free} míst volných · dnes ${row.visits_today} návštěv</span><div class="occupancy-meter"><i style="width:${percent}%"></i></div></article>`;
    })
    .join("");
}
function clothingAdvice(feels, rainChance, rain, gust) {
  let clothes;
  if (feels < 0) clothes = "Zimní bundu, čepici, šálu a rukavice.";
  else if (feels < 7) clothes = "Teplou bundu a mikinu, hodí se i čepice.";
  else if (feels < 13) clothes = "Bundu a mikinu, ráno raději dlouhé kalhoty.";
  else if (feels < 18) clothes = "Lehkou bundu nebo mikinu a dlouhé kalhoty.";
  else if (feels < 24) clothes = "Tričko, lehkou mikinu s sebou a dlouhé kalhoty.";
  else clothes = "Tričko a lehké oblečení, nezapomeň na pití.";
  if (rainChance >= 40 || rain >= 0.2) clothes += " Přibal nepromokavou bundu nebo deštník.";
  if (gust >= 35) clothes += " Bude nárazový vítr, zvol vrstvu, která neprofoukne.";
  return clothes;
}
function renderWeatherAdvice(weather) {
  const hourly = weather.hourly;
  const now = new Date();
  const localHour = Number(new Intl.DateTimeFormat("cs-CZ", { timeZone: "Europe/Prague", hour: "numeric", hourCycle: "h23" }).format(now));
  const localDate = new Intl.DateTimeFormat("sv-SE", { timeZone: "Europe/Prague" }).format(now);
  const target = new Date(`${localDate}T12:00:00`);
  if (localHour >= 8) target.setDate(target.getDate() + 1);
  const date = new Intl.DateTimeFormat("sv-SE", { timeZone: "Europe/Prague" }).format(target);
  const indices = (hours) => hourly.time.map((value, index) => ({ value, index })).filter(({ value }) => value.startsWith(date) && hours.includes(Number(value.slice(11, 13)))).map(({ index }) => index);
  const summarize = (selected) => ({
    feels: selected.reduce((sum, i) => sum + hourly.apparent_temperature[i], 0) / selected.length,
    rainChance: Math.max(...selected.map((i) => hourly.precipitation_probability[i])),
    rain: Math.max(...selected.map((i) => hourly.precipitation[i])),
    gust: Math.max(...selected.map((i) => hourly.wind_gusts_10m[i])),
  });
  const morning = summarize(indices([7, 8]));
  const afternoon = summarize(indices([12, 13, 14, 15]));
  const label = target.toLocaleDateString("cs-CZ", { weekday: "long", day: "numeric", month: "numeric" });
  $("#advice-day").textContent = label;
  $("#morning-label").textContent = `RÁNO · 7:30 · ${label}`;
  $("#afternoon-label").textContent = `ODPOLEDNE · 12:00–15:00 · ${label}`;
  $("#morning-weather").textContent = `${Math.round(morning.feels)} °C pocitově · déšť ${morning.rainChance} %`;
  $("#afternoon-weather").textContent = `${Math.round(afternoon.feels)} °C pocitově · déšť ${afternoon.rainChance} %`;
  $("#morning-advice").textContent = clothingAdvice(morning.feels, morning.rainChance, morning.rain, morning.gust);
  $("#afternoon-advice").textContent = clothingAdvice(afternoon.feels, afternoon.rainChance, afternoon.rain, afternoon.gust);
}
function escapeHtml(value) {
  const node = document.createElement("span");
  node.textContent = value ?? "";
  return node.innerHTML;
}
async function loadAviation() {
  const response = await fetch("/api/aviation");
  const data = await response.json();
  if (!response.ok) throw Error(data.error);
  const map = L.map("aircraft-map", { scrollWheelZoom: false }).setView(
    [49.1951, 16.6068],
    9,
  );
  L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
    maxZoom: 18,
    attribution: "© OpenStreetMap",
  }).addTo(map);
  L.circleMarker([49.1513, 16.6944], {
    radius: 6,
    color: "#d44835",
    fillOpacity: 1,
  })
    .bindPopup("Letiště Brno-Tuřany (BRQ)")
    .addTo(map);
  data.aircraft.forEach((plane) => {
    const heading = plane.heading ?? 0;
    const icon = L.divIcon({
      className: "aircraft-icon",
      html: `<i style="transform:rotate(${heading}deg)">▲</i>`,
      iconSize: [28, 28],
      iconAnchor: [14, 14],
    });
    const altitude = plane.altitude == null ? "neznámá" : `${Math.round(plane.altitude)} m`;
    const speed = plane.speed == null ? "neznámá" : `${Math.round(plane.speed * 3.6)} km/h`;
    L.marker([plane.lat, plane.lon], { icon })
      .bindPopup(`<b>${escapeHtml(plane.callsign)}</b><br>${escapeHtml(plane.country)}<br>výška ${altitude}<br>rychlost ${speed}`)
      .addTo(map);
  });
  $("#aircraft-count").textContent = `${data.aircraft.length} letadel v oblasti`;
  const rows = data.flights
    .map((flight) => {
      const date = new Date(flight.scheduled).toLocaleString("cs-CZ", {
        weekday: "short",
        day: "numeric",
        month: "numeric",
        hour: "2-digit",
        minute: "2-digit",
      });
      const direction = flight.direction === "arrivals" ? "Přílet" : "Odlet";
      return `<tr><td>${date}</td><td class="flight-direction">${direction}</td><td><b>${escapeHtml(flight.flight)}</b></td><td>${escapeHtml(flight.place)}</td><td>${escapeHtml(flight.airline)}</td><td>${escapeHtml(flight.note)}</td></tr>`;
    })
    .join("");
  $("#flight-schedule").innerHTML = !data.schedule_available
    ? '<p>Letiště Brno nyní dočasně blokuje automatické načtení letového plánu. Živé polohy letadel fungují dál.</p>'
    : data.flights.length
    ? `<table><thead><tr><th>Čas</th><th>Směr</th><th>Let</th><th>Odkud / kam</th><th>Dopravce</th><th>Stav</th></tr></thead><tbody>${rows}</tbody></table>`
    : "<p>V následujících 48 hodinách nejsou zveřejněné žádné lety.</p>";
}
async function loadRadiation() {
  const response = await fetch("/api/radiation");
  const data = await response.json();
  if (!response.ok) throw Error(data.error);
  $("#radiation").innerHTML = data.stations
    .map((row) => {
      const measured = new Date(row.measured).toLocaleString("cs-CZ", {
        day: "numeric",
        month: "numeric",
        hour: "2-digit",
        minute: "2-digit",
      });
      return `<article><h3>${escapeHtml(row.station)}</h3><b>${Math.round(row.average)}</b><small>${data.unit}</small><p>hodinový průměr · maximum ${Math.round(row.maximum)} ${data.unit}<br>měřeno ${measured}</p></article>`;
    })
    .join("");
  $("#radiation-status").textContent = data.stations.every((row) =>
    row.status.includes("Normální"),
  )
    ? "normální situace"
    : "zkontrolujte stav měření";
}
async function loadTransitIncidents() {
  const response = await fetch("/api/transit-incidents");
  const data = await response.json();
  if (!response.ok) throw Error(data.error);
  const checked = new Date(data.checked * 1000).toLocaleTimeString("cs-CZ", {
    hour: "2-digit",
    minute: "2-digit",
  });
  $("#transit-status").textContent = data.incidents.length
    ? `${data.incidents.length} aktivní · kontrola ${checked}`
    : `bez omezení · kontrola ${checked}`;
  if (!data.incidents.length) {
    $("#transit-incidents").innerHTML = '<div class="transit-ok">Na sledovaných linkách nyní DPMB neeviduje žádnou mimořádnou událost.</div>';
    return;
  }
  $("#transit-incidents").innerHTML = data.incidents
    .map((incident) => {
      const lines = incident.lines.map((line) => `<b class="transit-line">${line}</b>`).join("");
      const period = incident.from
        ? `${new Date(incident.from).toLocaleString("cs-CZ")} – ${incident.to ? new Date(incident.to).toLocaleString("cs-CZ") : "do odvolání"}`
        : "platnost neuvedena";
      const details = [period, incident.direction ? `směr: ${incident.direction}` : "", incident.delay_minutes ? `zdržení: ${incident.delay_minutes} min` : ""].filter(Boolean).join(" · ");
      return `<article class="transit-incident"><div class="transit-lines">${lines}</div><h3><a href="${incident.url}" target="_blank" rel="noopener">${escapeHtml(incident.title)}</a></h3><p class="transit-meta">${escapeHtml(details)}</p></article>`;
    })
    .join("");
}
async function loadAurora() {
  const response = await fetch("/api/aurora");
  const data = await response.json();
  if (!response.ok) throw Error(data.error);
  $("#aurora-status").textContent = `šance ${data.level}`;
  $("#aurora-kp").textContent = data.current_kp?.toFixed(1) ?? "–";
  $("#aurora-peak").textContent = data.peak_kp_48h?.toFixed(1) ?? "–";
  $("#aurora-probability").textContent = data.brno_probability;
  $("#aurora-message").textContent = data.message;
  const forecast = new Date(data.forecast_time).toLocaleString("cs-CZ", {
    day: "numeric",
    month: "numeric",
    hour: "2-digit",
    minute: "2-digit",
  });
  const peak = data.peak_time
    ? new Date(data.peak_time).toLocaleString("cs-CZ", { weekday: "short", hour: "2-digit", minute: "2-digit" })
    : "neuvedeno";
  $("#aurora-time").textContent = `Model pro ${forecast} · očekávané maximum Kp: ${peak}`;
}
async function loadMedlankyEvents() {
  const response = await fetch("/api/medlanky-events");
  const data = await response.json();
  if (!response.ok) throw Error(data.error);
  $("#medlanky-events-status").textContent = `${data.total} nadcházejících akcí`;
  if (!data.events.length) {
    $("#medlanky-events").innerHTML = "<p>Kalendář nyní neobsahuje žádné nadcházející akce.</p>";
    return;
  }
  const today = new Intl.DateTimeFormat("sv-SE", { timeZone: "Europe/Prague" }).format(new Date());
  const formatDate = (value, options) => new Date(`${value}T12:00:00`).toLocaleDateString("cs-CZ", options);
  $("#medlanky-events").innerHTML = data.events
    .map((event) => {
      const ongoing = event.date_from < today && event.date_to >= today;
      const day = ongoing ? "běží" : formatDate(event.date_from, { day: "numeric" });
      const month = ongoing ? `do ${formatDate(event.date_to, { day: "numeric", month: "numeric" })}` : formatDate(event.date_from, { month: "short" });
      const dateRange = event.date_to && event.date_to !== event.date_from ? `${formatDate(event.date_from, { day: "numeric", month: "numeric" })} – ${formatDate(event.date_to, { day: "numeric", month: "numeric" })}` : formatDate(event.date_from, { weekday: "long", day: "numeric", month: "numeric" });
      const timeRange = event.time_from ? `${event.time_from}${event.time_to ? `–${event.time_to}` : ""}` : "čas neuveden";
      return `<article class="medlanky-event"><div class="event-date"><b>${day}</b><span>${month}</span></div><div><h3><a href="${event.url}" target="_blank" rel="noopener">${escapeHtml(event.title)}</a></h3><p>${escapeHtml(dateRange)} · ${escapeHtml(timeRange)}${event.place ? ` · ${escapeHtml(event.place)}` : ""}</p></div></article>`;
    })
    .join("");
}
async function loadDaylight() {
  const response = await fetch("/api/daylight");
  const rows = await response.json();
  const today = new Intl.DateTimeFormat("sv-SE", { timeZone: "Europe/Prague" }).format(new Date());
  const current = rows.find((row) => row.date === today);
  if (current) {
    const hours = Math.floor(current.hours);
    $("#daylight-now").textContent = `dnes ${hours} h ${Math.round((current.hours - hours) * 60)} min`;
  }
  new Chart($("#daylightChart"), {
    type: "line",
    data: {
      labels: rows.map((row) => row.date),
      datasets: [{ label: "Délka dne", data: rows.map((row) => row.hours), borderColor: "#d49b24", backgroundColor: "#d49b2433", fill: true, pointRadius: 0, tension: 0.25 }],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: { tooltip: { callbacks: { label: (item) => `${Math.floor(item.raw)} h ${Math.round((item.raw % 1) * 60)} min` } } },
      scales: { x: { ticks: { maxTicksLimit: 9, callback: (_, index) => new Date(rows[index].date).toLocaleDateString("cs-CZ", { day: "numeric", month: "short" }) } }, y: { title: { display: true, text: "hodin" } } },
    },
  });
}
async function load() {
  const response = await fetch("/api/data");
  const data = await response.json();
  if (!response.ok) throw Error(data.error);
  renderOccupancy(data.occupancy);
  renderWeatherAdvice(data.weather);
  const w = data.weather.current,
    a = data.air.current,
    d = data.weather.daily;
  $("#temp").textContent = Math.round(w.temperature_2m);
  $("#feels").textContent = Math.round(w.apparent_temperature);
  $("#cloud").textContent = w.cloud_cover;
  $("#wind").textContent = Math.round(w.wind_speed_10m);
  $("#gust").textContent = Math.round(w.wind_gusts_10m);
  $("#rain").textContent = w.precipitation.toFixed(1);
  $("#condition").textContent =
    weatherNames[w.weather_code] || "Proměnlivé počasí";
  $("#updated").textContent =
    "aktualizováno " +
    new Date(data.updated * 1000).toLocaleTimeString("cs-CZ");
  $("#aqi").textContent = Math.round(a.european_aqi);
  $("#aqi-title").textContent = aqiName(a.european_aqi);
  $("#pm25").textContent = a.pm2_5.toFixed(1) + " µg/m³";
  $("#pm10").textContent = a.pm10.toFixed(1) + " µg/m³";
  $("#no2").textContent = a.nitrogen_dioxide.toFixed(1) + " µg/m³";
  $("#o3").textContent = a.ozone.toFixed(1) + " µg/m³";
  $("#sunrise").textContent = time(d.sunrise[0]);
  $("#sunset").textContent = time(d.sunset[0]);
  const hours = Math.floor(d.daylight_duration[0] / 3600),
    minutes = Math.round((d.daylight_duration[0] % 3600) / 60);
  $("#daylight").textContent = `${hours} h ${minutes} min`;
  const start = data.weather.hourly.time.findIndex(
      (x) => x >= data.weather.current.time,
    ),
    end = start + 24,
    labels = data.weather.hourly.time.slice(start, end).map(time);
  new Chart($("#forecastChart"), {
    data: {
      labels,
      datasets: [
        {
          type: "line",
          label: "Teplota °C",
          data: data.weather.hourly.temperature_2m.slice(start, end),
          borderColor: "#d44835",
          yAxisID: "y",
          tension: 0.25,
        },
        {
          type: "bar",
          label: "Srážky mm",
          data: data.weather.hourly.precipitation.slice(start, end),
          backgroundColor: "#3f83a8aa",
          yAxisID: "rain",
        },
        {
          type: "line",
          label: "Vítr km/h",
          data: data.weather.hourly.wind_speed_10m.slice(start, end),
          borderColor: "#08736a",
          yAxisID: "wind",
          pointRadius: 0,
        },
      ],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      interaction: { mode: "index", intersect: false },
      scales: {
        y: { position: "left", title: { display: true, text: "°C" } },
        rain: {
          position: "right",
          min: 0,
          title: { display: true, text: "mm" },
          grid: { drawOnChartArea: false },
        },
        wind: { display: false },
      },
    },
  });
}
load().catch(() => {
  $("#updated").textContent = "data dočasně nedostupná";
});
loadAviation().catch(() => {
  $("#aircraft-count").textContent = "letecká data dočasně nedostupná";
  $("#flight-schedule").innerHTML = "<p>Letový plán se nepodařilo načíst.</p>";
});
loadRadiation().catch(() => {
  $("#radiation-status").textContent = "data dočasně nedostupná";
});
loadTransitIncidents().catch(() => {
  $("#transit-status").textContent = "data dočasně nedostupná";
});
loadAurora().catch(() => {
  $("#aurora-status").textContent = "data dočasně nedostupná";
});
loadMedlankyEvents().catch(() => {
  $("#medlanky-events-status").textContent = "data dočasně nedostupná";
});
loadDaylight().catch(() => {
  $("#daylight-now").textContent = "data dočasně nedostupná";
});
setTimeout(() => window.location.reload(), 300000);
