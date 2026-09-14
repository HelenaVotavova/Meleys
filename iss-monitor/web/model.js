const modelFrame = document.querySelector('#iss-model');
const modelClient = new Sketchfab('1.12.1', modelFrame);
const dockButtons = document.querySelectorAll('[data-dock]');
let dockApi;
let dockAnnotations = [];

dockButtons.forEach(button => button.addEventListener('click', () => {
  const annotation = dockAnnotations[Number(button.dataset.dock)];
  if (dockApi && annotation !== undefined) {
    dockApi.gotoAnnotation(annotation, { preventCameraAnimation: false, preventCameraMove: false });
  }
}));

modelClient.init('b7d40d89fcbd4c998462380545f391b6', {
  autostart: 1,
  transparent: 1,
  ui_theme: 'dark',
  ui_infos: 0,
  ui_hint: 0,
  ui_watermark_link: 0,
  success(api) {
    dockApi = api;
    api.start();
    api.addEventListener('viewerready', () => {
      api.getCameraLookAt((cameraError, camera) => {
        if (cameraError) return;
        const docks = [
          { position: [0, 0, 5.3], title: 'Progress 95', text: 'Zadní port modulu Zvezda' },
          { position: [0, -2.1, -1.3], title: 'Sojuz MS-29', text: 'Dokovací modul Pričal' },
          { position: [0, 2.2, -1.5], title: 'Dragon Crew-12', text: 'Horní port modulu Harmony' },
          { position: [0, -2.2, 1.3], title: 'Cygnus XL', text: 'Spodní port modulu Unity' }
        ];
        docks.forEach((dock, dockIndex) => api.createAnnotationFromScenePosition(
          dock.position,
          camera.position,
          camera.target,
          dock.title,
          dock.text,
          (error, annotationIndex) => {
            if (!error) {
              dockAnnotations[dockIndex] = annotationIndex;
              api.showAnnotation(annotationIndex);
            }
          }
        ));
      });
    });
  },
  error() {
    modelFrame.title = '3D model se nepodařilo načíst';
  }
});
