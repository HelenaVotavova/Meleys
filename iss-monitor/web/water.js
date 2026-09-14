require(['LightstreamerClient', 'Subscription'], (LightstreamerClient, Subscription) => {
  const values = {};
  const state = (map, value) => map[Number(value)] || value || '–';
  const upaStates = { 2: 'zastaven', 4: 'vypínání', 8: 'údržba', 16: 'běžný provoz', 32: 'pohotovost', 64: 'nečinný', 128: 'inicializován' };
  const wpaStates = { 1: 'zastaven', 2: 'vypínání', 3: 'pohotovost', 4: 'zpracovává', 5: 'horký servis', 6: 'proplach', 7: 'teplé vypnutí' };
  const wpaSteps = { 0: '', 1: 'odvětrání', 2: 'ohřev', 3: 'proplach', 4: 'průtok', 5: 'test', 6: 'test ventilu 1', 7: 'test ventilu 2', 8: 'servis' };
  const fields = {
    NODE3000005: ['urine-water', value => `${Number(value).toFixed(1)} %`],
    NODE3000008: ['waste-water', value => `${Number(value).toFixed(1)} %`],
    NODE3000009: ['clean-water', value => `${Number(value).toFixed(1)} %`]
  };
  const updateAtmosphere = () => {
    const pressureTorr = Number(values.USLAB000058);
    const oxygenTorr = Number(values.USLAB000053);
    const co2Torr = Number(values.USLAB000055);
    if (Number.isFinite(Number(values.USLAB000059))) document.querySelector('#cabin-temp').textContent = `${Number(values.USLAB000059).toFixed(1)} °C`;
    if (Number.isFinite(pressureTorr)) document.querySelector('#cabin-pressure').textContent = `${(pressureTorr / 7.50062).toFixed(1)} kPa`;
    if (Number.isFinite(pressureTorr) && pressureTorr > 0 && Number.isFinite(oxygenTorr)) document.querySelector('#cabin-oxygen').textContent = `${(oxygenTorr / pressureTorr * 100).toFixed(1)} %`;
    if (Number.isFinite(pressureTorr) && pressureTorr > 0 && Number.isFinite(co2Torr)) document.querySelector('#cabin-co2').textContent = `${Math.round(co2Torr / pressureTorr * 1000000)} ppm`;
  };
  const client = new LightstreamerClient('https://push.lightstreamer.com', 'ISSLIVE');
  const subscription = new Subscription('MERGE', ['NODE3000004', 'NODE3000005', 'NODE3000006', 'NODE3000007', 'NODE3000008', 'NODE3000009', 'USLAB000053', 'USLAB000055', 'USLAB000058', 'USLAB000059'], ['Value', 'TimeStamp']);
  subscription.addListener({
    onItemUpdate(update) {
      const item = update.getItemName();
      const value = update.getValue('Value');
      values[item] = value;
      updateAtmosphere();
      if (fields[item] && Number.isFinite(Number(value))) document.querySelector(`#${fields[item][0]}`).textContent = fields[item][1](value);
      document.querySelector('#upa-state').textContent = state(upaStates, values.NODE3000004);
      document.querySelector('#wpa-state').textContent = state(wpaStates, values.NODE3000006);
      document.querySelector('#wpa-step').textContent = state(wpaSteps, values.NODE3000007);
      document.querySelector('#water-time').textContent = update.getValue('TimeStamp') || new Date().toLocaleTimeString('cs-CZ');
      document.querySelector('#water-signal').textContent = 'živá telemetrie';
    }
  });
  client.addListener({ onStatusChange(status) { if (status.startsWith('DISCONNECTED')) document.querySelector('#water-signal').textContent = 'signál nedostupný'; } });
  client.subscribe(subscription);
  client.connect();
});
