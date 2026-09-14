import * as THREE from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
import {CSS2DRenderer,CSS2DObject} from 'three/addons/renderers/CSS2DRenderer.js';

const host=document.querySelector('#iss3d'), scene=new THREE.Scene();
const camera=new THREE.PerspectiveCamera(42,1,.1,200);camera.position.set(19,13,23);
const renderer=new THREE.WebGLRenderer({antialias:true});renderer.setPixelRatio(Math.min(devicePixelRatio,2));renderer.setClearColor(0x101820);host.appendChild(renderer.domElement);
const labels=new CSS2DRenderer();labels.domElement.style.cssText='position:absolute;inset:0;pointer-events:none';host.appendChild(labels.domElement);
scene.add(new THREE.HemisphereLight(0xddefff,0x26323b,2.6));const sun=new THREE.DirectionalLight(0xffffff,3);sun.position.set(8,12,10);scene.add(sun);
const station=new THREE.Group();station.rotation.set(.12,-.32,0);scene.add(station);
const metal=new THREE.MeshStandardMaterial({color:0xc4cbd0,metalness:.65,roughness:.35}), russian=new THREE.MeshStandardMaterial({color:0x9ea9a3,metalness:.45,roughness:.5}), gold=new THREE.MeshStandardMaterial({color:0xc79b35,metalness:.35,roughness:.45}), blue=new THREE.MeshStandardMaterial({color:0x164d7d,metalness:.2,roughness:.5}), shipMat=new THREE.MeshStandardMaterial({color:0xe34b38,metalness:.35,roughness:.4});
function label(object,name,sub='',ship=false,y=1.15){if(!ship)return;const el=document.createElement('div');el.className='model-label ship';el.innerHTML=name+(sub?`<small>${sub}</small>`:'');const tag=new CSS2DObject(el);tag.position.set(0,y,0);object.add(tag)}
function module(name,x,y,z,length=2.8,radius=.65,mat=metal,sub=''){const mesh=new THREE.Mesh(new THREE.CylinderGeometry(radius,radius,length,20),mat);mesh.rotation.z=Math.PI/2;mesh.position.set(x,y,z);station.add(mesh);label(mesh,name,sub);return mesh}
module('Zvezda',-7,0,0,3.2,.7,russian,'servisní modul');module('Zarja',-4.2,0,0,2.4,.7,russian,'první modul ISS');module('Unity',-1.8,0,0,1.3,.85,metal,'spojovací uzel');module('Destiny',.4,0,0,3,.72,metal,'laboratoř USA');module('Harmony',3,0,0,1.5,.88,metal,'spojovací uzel');module('Columbus',1.5,0,1.8,2.5,.62,metal,'laboratoř ESA').rotation.y=Math.PI/2;module('Kibo',1.5,0,-2,3.2,.7,metal,'laboratoř JAXA').rotation.y=Math.PI/2;module('Nauka',-5.2,-1.8,0,3,.68,russian,'ruská laboratoř').rotation.z=0;const prichal=module('Pričal',-5.2,-3.5,0,1.2,.78,russian,'dokovací uzel');prichal.rotation.z=0;module('Tranquility',-1.8,-1.7,0,2.1,.65,metal,'životní podpora').rotation.z=0;
const cupola=new THREE.Mesh(new THREE.CylinderGeometry(.48,.72,.5,7),gold);cupola.position.set(-1.8,-2.9,0);station.add(cupola);label(cupola,'Cupola','pozorovací okna','',.8);
const truss=new THREE.Mesh(new THREE.BoxGeometry(23,.28,.28),metal);truss.position.set(0,2.1,0);station.add(truss);label(truss,'Hlavní příhradový nosník','',false,.65);
for(const side of [-1,1])for(const x of [-8.2,-4.9,4.9,8.2]){const panel=new THREE.Mesh(new THREE.BoxGeometry(2.7,.06,4.2),blue);panel.position.set(x,2.1,side*2.45);station.add(panel);const boom=new THREE.Mesh(new THREE.BoxGeometry(.12,.12,2.2),metal);boom.position.set(x,2.1,side*1.1);station.add(boom)}
const dragon=module('Dragon Crew-12',3,1.75,0,2.2,.7,shipMat,'Harmony · horní port');dragon.rotation.z=0;label(dragon,'Dragon Crew-12','Harmony · horní port',true,1.4);const cone=new THREE.Mesh(new THREE.ConeGeometry(.7,1.2,20),shipMat);cone.position.set(3,3.45,0);station.add(cone);
const soyuz=module('Sojuz MS-29',-5.2,-5.1,0,1.9,.55,shipMat,'Pričal');soyuz.rotation.z=0;label(soyuz,'Sojuz MS-29','Pričal',true,1.2);const soyuzCone=new THREE.Mesh(new THREE.ConeGeometry(.55,1,20),shipMat);soyuzCone.rotation.z=Math.PI;soyuzCone.position.set(-5.2,-6.45,0);station.add(soyuzCone);
const controls=new OrbitControls(camera,renderer.domElement);controls.enableDamping=true;controls.minDistance=14;controls.maxDistance=50;controls.target.set(0,0,0);
function size(){const w=host.clientWidth,h=host.clientHeight;camera.aspect=w/h;camera.updateProjectionMatrix();renderer.setSize(w,h);labels.setSize(w,h)}size();addEventListener('resize',size);
function animate(){requestAnimationFrame(animate);controls.update();renderer.render(scene,camera);labels.render(scene,camera)}animate();
