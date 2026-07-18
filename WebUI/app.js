// ─────────────────────────────────────────────────────────────────────────────
// Pitch Wrench — app.js
// Bridge JS ↔ C++ (PluginEditor) + UI interaction logic
// ─────────────────────────────────────────────────────────────────────────────

// ── Costanti ──────────────────────────────────────────────────────────────────
const MIN_SEMITONES = -24;
const MAX_SEMITONES =  24;
const MIN_ANGLE     = -140;   // gradi (7 o'clock)
const MAX_ANGLE     =  140;   // gradi (5 o'clock)

// ── State ─────────────────────────────────────────────────────────────────────
const state = {
  semitones: 0,
  glide:     5.0,
  enabled:   true,
  uiScale:   1,
};

// ── DOM refs ──────────────────────────────────────────────────────────────────
const knobRig    = document.getElementById('knob-rig');
const knobDial   = document.getElementById('knob-dial');
const valNumber  = document.getElementById('val-number');

const glideSlider = document.getElementById('glide-slider');
const glideFill   = document.getElementById('glide-fill');
const glideVal    = document.getElementById('glide-val');

const powerBtn   = document.getElementById('power-btn');
const powerLed   = document.getElementById('power-led');
const powerText  = document.getElementById('power-text');
const tickCanvas = document.getElementById('tick-canvas');
const sizeSelect = document.getElementById('size-select');
const shell      = document.getElementById('plugin-shell');

// ── Tick canvas ───────────────────────────────────────────────────────────────
function drawTicks() {
  const w = tickCanvas.offsetWidth;
  const h = tickCanvas.offsetHeight;
  tickCanvas.width  = w;
  tickCanvas.height = h;
  const cx = w / 2;
  const cy = h / 2;
  const ctx = tickCanvas.getContext('2d');
  ctx.clearRect(0, 0, w, h);

  const totalSteps = MAX_SEMITONES - MIN_SEMITONES;  // 48

  for (let i = 0; i <= totalSteps; ++i) {
    const val = MIN_SEMITONES + i;
    const t   = i / totalSteps;
    const ang = (MIN_ANGLE + t * (MAX_ANGLE - MIN_ANGLE)) * (Math.PI / 180);

    const isMajor = val % 6 === 0;
    const isZero  = val === 0;

    const outerR = cx - 4;
    const innerR = outerR - (isZero ? 18 : isMajor ? 14 : 8);

    const cos = Math.cos(ang - Math.PI / 2);
    const sin = Math.sin(ang - Math.PI / 2);

    ctx.beginPath();
    ctx.moveTo(cx + cos * innerR, cy + sin * innerR);
    ctx.lineTo(cx + cos * outerR, cy + sin * outerR);

    if (isZero) {
      ctx.strokeStyle = '#ff8c00';
      ctx.lineWidth   = 3;
    } else if (isMajor) {
      ctx.strokeStyle = '#ff8c0088';
      ctx.lineWidth   = 2;
    } else {
      ctx.strokeStyle = '#4a4e52';
      ctx.lineWidth   = 1;
    }
    ctx.stroke();

    // Label per i major ticks
    if (isMajor) {
      const labelR = innerR - 16;
      ctx.font         = '500 12px Teko, sans-serif';
      ctx.fillStyle    = isZero ? '#ff8c00' : '#737880';
      ctx.textAlign    = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(
        val > 0 ? '+' + val : String(val),
        cx + cos * labelR,
        cy + sin * labelR
      );
    }
  }
}

// ── Knob rotation ─────────────────────────────────────────────────────────────
function semitonesToAngle(st) {
  const t = (st - MIN_SEMITONES) / (MAX_SEMITONES - MIN_SEMITONES);
  return MIN_ANGLE + t * (MAX_ANGLE - MIN_ANGLE);
}

function setKnobAngle(angle) {
  knobDial.style.transform = `rotate(${angle}deg)`;
}

function setSemitones(st, sendToHost = true) {
  st = Math.round(Math.max(MIN_SEMITONES, Math.min(MAX_SEMITONES, st)));
  state.semitones = st;
  setKnobAngle(semitonesToAngle(st));
  valNumber.textContent = st > 0 ? '+' + st : String(st);
  if (sendToHost) hostSetParameter('semitones', st);
}

// ── Knob drag ─────────────────────────────────────────────────────────────────
let activeDrag = null;

function genericDragStart(e, currentVal) {
  e.preventDefault();
  activeDrag = { startY: e.clientY, startVal: currentVal };
  document.body.style.cursor = 'grabbing';
}

knobRig.addEventListener('mousedown', (e) => {
  genericDragStart(e, state.semitones);
  activeDrag.target = 'semitones';
});

document.addEventListener('mousemove', (e) => {
  if (!activeDrag) return;
  const delta = activeDrag.startY - e.clientY;
  
  if (activeDrag.target === 'semitones') {
    const range = MAX_SEMITONES - MIN_SEMITONES;
    const pxRange = 180;
    setSemitones(activeDrag.startVal + (delta / pxRange) * range);
  }
});

document.addEventListener('mouseup', () => {
  if (!activeDrag) return;
  activeDrag = null;
  document.body.style.cursor = '';
});

// Scroll wheel sul knob principale (1 semitono per click)
knobRig.addEventListener('wheel', (e) => {
  e.preventDefault();
  const dir = e.deltaY < 0 ? 1 : -1;
  setSemitones(state.semitones + dir);
}, { passive: false });

knobRig.addEventListener('dblclick', () => setSemitones(0));

// ── Glide Slider ──────────────────────────────────────────────────────────────
glideSlider.addEventListener('input', (e) => {
  // slider values are 0..1000
  // we want exponential curve from 5ms to 2000ms
  const v = e.target.value / 1000.0;
  // Use a simple cubic curve
  const minG = 5.0;
  const maxG = 2000.0;
  const ms = minG + Math.pow(v, 3) * (maxG - minG);
  setGlide(ms);
});

function setGlide(ms, sendToHost = true) {
  ms = Math.max(5.0, Math.min(2000.0, ms));
  state.glide = ms;
  
  // Inverse curve to set slider value
  const minG = 5.0;
  const maxG = 2000.0;
  const v = Math.pow((ms - minG) / (maxG - minG), 1.0/3.0);
  
  glideSlider.value = Math.round(v * 1000);
  glideFill.style.width = (v * 100) + '%';
  glideVal.textContent = Math.round(ms) + ' ms';
  
  if (sendToHost) hostSetParameter('glide', ms);
}

// ── Power button ──────────────────────────────────────────────────────────────
powerBtn.addEventListener('click', () => {
  setEnabled(!state.enabled);
});

function setEnabled(val, sendToHost = true) {
  state.enabled = val;
  powerBtn.classList.toggle('active', val);
  powerLed.classList.toggle('active', val);
  powerText.textContent = val ? 'ON' : 'OFF';
  if (sendToHost) hostSetParameter('enabled', val ? 1 : 0);
}

// ── Size selector ─────────────────────────────────────────────────────────────
sizeSelect.addEventListener('change', () => {
  const scale = parseInt(sizeSelect.value, 10);
  applyScale(scale);
  hostSetParameter('uiScale', scale);
});

function applyScale(scale) {
  state.uiScale = scale;
  sizeSelect.value = String(scale);
  // Le dimensioni del contenitore vengono gestite dal C++ (resize dell'editor)
  // La WebUI si adatta con CSS grazie a width/height: 100%
}

// ── Bridge verso il host C++ ──────────────────────────────────────────────────
function hostSetParameter(param, value) {
  // Metodo per comunicare con il PluginEditor JUCE 8
  if (window.__JUCE__ && window.__JUCE__.backend) {
    window.__JUCE__.backend.emitEvent("hostSetParameter", { param, value });
  }
}

// ── Bridge dal host C++ → JS ──────────────────────────────────────────────────
// Chiamato da PluginEditor.cpp via evaluateJavascript()
window.pitchWrench = {
  updateParam(param, value) {
    switch (param) {
      case 'semitones': setSemitones(value, false);       break;
      case 'glide':     setGlide(value, false);           break;
      case 'enabled':   setEnabled(value >= 0.5, false);  break;
      case 'uiScale':   applyScale(Math.round(value));    break;
    }
  }
};

// ── Init ──────────────────────────────────────────────────────────────────────
function init() {
  drawTicks();
  setSemitones(0, false);
  setGlide(5.0, false);
  setEnabled(true, false);

  // Ridisegna i tick se la finestra cambia dimensione
  window.addEventListener('resize', drawTicks);
  
  if (window.__JUCE__ && window.__JUCE__.backend) {
    window.__JUCE__.backend.emitEvent("ready", {});
  }
}

document.addEventListener('DOMContentLoaded', init);
