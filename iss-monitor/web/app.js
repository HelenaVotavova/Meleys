const people=[
 ['Jessica Meir','USA','49 let','2026-02-12'],['Jack Hathaway','USA','přibl. 43 let','2026-02-12'],
 ['Sophie Adenot','Francie','44 let','2026-02-12'],['Andrey Fedyaev','Rusko','45 let','2026-02-12'],
 ['Anil Menon','USA','49 let','2026-07-14'],['Pyotr Dubrov','Rusko','48 let','2026-07-14'],['Anna Kikina','Rusko','42 let','2026-07-14']];
const crew=document.querySelector('#crew');
for(const [name,country,age,arrival] of people){const days=Math.floor((Date.now()-new Date(arrival+'T00:00:00Z'))/864e5);crew.insertAdjacentHTML('beforeend',`<article class="person"><b>${name}</b><span>${country} · ${age}</span><span>na ISS ${days} dní</span></article>`)}
const map=L.map('map',{worldCopyJump:true,zoomControl:true}).setView([20,0],2);
L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{attribution:'© OpenStreetMap',maxZoom:8}).addTo(map);
const icon=L.divIcon({className:'',html:'<div style="background:#e24632;color:#fff;border:3px solid white;border-radius:50%;width:48px;height:48px;display:grid;place-items:center;font-weight:900;box-shadow:0 2px 8px #0007">ISS</div>',iconSize:[48,48],iconAnchor:[24,24]});
const marker=L.marker([0,0],{icon}).addTo(map), track=L.polyline([],{color:'#08736a',weight:3}).addTo(map);let points=[];
async function update(){try{const r=await fetch('/api/position');const d=await r.json();if(!r.ok)throw Error(d.error);const p=[d.latitude,d.longitude];marker.setLatLng(p);points.push(p);if(points.length>180)points.shift();track.setLatLngs(points);document.querySelector('#speed').textContent=Math.round(d.velocity).toLocaleString('cs-CZ');document.querySelector('#altitude').textContent=Math.round(d.altitude);document.querySelector('#coords').textContent=`${d.latitude.toFixed(2)}°, ${d.longitude.toFixed(2)}°`;document.querySelector('#light').textContent=d.visibility==='daylight'?'den':'noc';document.querySelector('#state').textContent='Živá data · '+new Date(d.timestamp*1000).toLocaleTimeString('cs-CZ');if(d.heading!=null){document.querySelector('#station').style.transform=`rotate(${d.heading-90}deg)`;document.querySelector('#heading').textContent=`kurz ${Math.round(d.heading)}°`}}catch(e){document.querySelector('#state').textContent='Živá data dočasně nedostupná'}}
update();setInterval(update,5000);
