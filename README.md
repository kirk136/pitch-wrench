# Pitch Wrench

**Pulverine Audio** — Modulo piattaforma v1.0

Pitch shifter ultra-preciso per chitarra e basso. Da -24 a +24 semitoni.  
Latenza algoritmica: **0 campioni**. Latenza totale determinata solo dal block size del runtime.

---

## Algoritmo DSP

**Dual-Head Crossfading Delay Line** — dominio del tempo, zero FFT.

- Due testine di lettura su buffer circolare
- Crossfade Hann quando una testina si avvicina alla zona di collisione
- Interpolazione Hermite cubica (migliore di lineare, più leggera di sinc)
- Smoothing pitch ratio 8ms (anti-zipper, mantiene bending naturali)
- Bypass con crossfade 3ms (zero click al toggle)

## Architettura

```
Source/
├── PlatformModule.h      Platform::IModule interface (zero JUCE)
├── Parameters.h          ID stabili, range, normalizzazione (zero JUCE)  
├── PitchWrenchDSP.h      Motore DSP (zero JUCE, zero allocazioni in process)
├── PitchWrenchModule.h   IModule implementation (atomic params, serializable state)
├── PitchWrenchModule.cpp TU esplicita
├── PluginProcessor.h/cpp JUCE wrapper (APVTS, processBlock, preset)
└── PluginEditor.h/cpp    WebView editor (HTML/CSS/JS bundled)
WebUI/
├── index.html            Struttura UI
├── style.css             Design industriale flat (Gunmetal + Caution Orange)
└── app.js                Knob drag, power, size, bridge C++↔JS
```

## Build

```bash
chmod +x build_mac.sh
./build_mac.sh
```

Richiede: CMake 3.22+, Xcode Command Line Tools, accesso internet (JUCE via FetchContent).

## Parametri

| ID | Nome | Range | Default |
|----|------|-------|---------|
| `semitones_v1` | Semitones | -24 / +24 | 0 |
| `enabled_v1` | Enabled | on/off | on |
| `ui_scale_v1` | UI Scale | S/M/L | Medium |

## Costituzione

Questo modulo rispetta la [CONSTITUTION.md](CONSTITUTION.md) della piattaforma Pulverine Audio.

- DSP e UI separati ✓
- Zero allocazioni nel thread audio ✓  
- Zero JUCE nel core DSP ✓
- Stato serializzabile e versionato ✓
- Latenza algoritmica = 0 ✓
