# Reverb de Espacios Imposibles — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Plugin Audio Unit de reverb estéreo para Logic Pro basado en una FDN de 16 líneas, con EQ de 6 bandas, LFO auto/manual, envolvente editable (pre-delay/sustain/release), inversor y puerta.

**Architecture:** El DSP vive en `dsp/` como C++17 puro sin JUCE, orquestado por `ReverbEngine`, y se prueba en Linux con un ejecutable CTest sin dependencias. `plugin/` envuelve el motor con JUCE 8 (parámetros, estado, interfaz). GitHub Actions en macOS compila el AU, pasa `auval` y publica el `.component`.

**Tech Stack:** C++17, CMake ≥ 3.22, JUCE 8.0.4 (FetchContent, solo si `EI_BUILD_PLUGIN=ON`), GitHub Actions `macos-latest`.

**Spec:** `docs/superpowers/specs/2026-11-12-reverb-espacios-imposibles-design.md`

## Global Constraints

- Todo el DSP en `namespace ei`, cabeceras en `dsp/`, sin incluir JUCE.
- Ninguna reserva de memoria, bloqueo ni E/S en el camino de audio (`process*`); toda la memoria se reserva en `prepare`.
- Frecuencias de muestreo soportadas: 44,1–192 kHz. Tamaños de bloque: de 1 a 8192 muestras.
- Identidad del plugin: nombre `Impossible Spaces`, fabricante `VitaPlug`, código de fabricante `Vita`, código de plugin `ImSp`, tipo AU `aufx`, bundle id `com.vitaplug.impossiblespaces`, target CMake `ImpossibleSpaces`.
- Formatos: `AU` y `Standalone`. Binario universal `arm64;x86_64`, macOS mínimo 11.0.
- IDs, rangos y valores por defecto de los parámetros: los de la sección 5 de la spec, literalmente.
- La salida del motor debe ser finita y con |x| ≤ 8.0 para cualquier combinación de parámetros con entrada |x| ≤ 1.
- Rótulos de interfaz en inglés; README en español. Diseño visual: maqueta aprobada (negro + verde, ver sección 6 de la spec). Mensajes de commit en inglés.
- Tests: `cmake -B build -DEI_BUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build --output-on-failure`.

## Review Focus

1. **Host sin tempo o con tempo 0** (Logic parado, Standalone): LFO e inversor sincronizados deben usar 120 BPM, nunca dividir por cero. → test en Task 6 y Task 9.
2. **Cambio de frecuencia de muestreo o `prepare` repetido** (Logic cambia a 96 kHz): los tiempos (RT60, pre-delay, ventana) se mantienen en segundos y no hay accesos fuera de búfer. → test en Task 9.
3. **Barridos bruscos de Size/Pitch/Window por automatización**: sin NaN, sin lecturas fuera de rango, salida acotada. → test en Task 9.
4. **Bloques de tamaño variable, incluido 1 muestra**: mismo resultado que con bloque fijo. → test en Task 9.
5. **Silencio prolongado tras la cola**: sin denormales que disparen la CPU; la salida llega a cero exacto o < 1e-15. → test en Task 3.

---

### Task 1: Esqueleto, arnés de tests, `DelayLine` y `EnvelopeFollower`

**Files:**
- Create: `CMakeLists.txt`, `.gitignore`, `tests/CMakeLists.txt`, `tests/TestHarness.h`, `tests/main.cpp`
- Create: `dsp/DelayLine.h`, `dsp/EnvelopeFollower.h`
- Test: `tests/DelayLineTests.cpp`

**Interfaces:**
- Produces:
  - `tests/TestHarness.h`: macros `TEST(name)`, `CHECK(cond)`, `CHECK_NEAR(a, b, tol)`; registro estático; `main` devuelve ≠ 0 si falla algo. Helper `float rmsDb(const float*, int)`.
  - `class ei::DelayLine { void prepare(int maxSamples); void reset(); void write(float x); float read(float delaySamples) const; float readInt(int delaySamples) const; };` — `read` usa interpolación Hermite de 4 puntos; el retardo se recorta a `[1, maxSamples-3]`.
  - `class ei::EnvelopeFollower { void prepare(double sr); void setTimes(float attackMs, float releaseMs); float process(float x); void reset(); };` — detector de pico, devuelve nivel lineal.
- CMake: opción `EI_BUILD_PLUGIN` (por defecto `ON` en Apple, `OFF` en el resto); target `INTERFACE` `ei_dsp`; ejecutable `ei_tests` registrado en CTest.

- [ ] **Step 1: Tests que fallan** — `DelayLine_integerDelay` (impulso sale exactamente a los 100 samples), `DelayLine_fractionalDelay` (seno de 1 kHz retrasado 10,5 samples: error < 1e-3 frente al analítico), `DelayLine_clampsOutOfRange` (retardos −5 y 1e9 no rompen y devuelven finito), `EnvelopeFollower_tracksLevel` (seno de amplitud 0,5 → 0,5 ± 0,05; tras 500 ms de silencio con release 50 ms → < 0,01).
- [ ] **Step 2: Ejecutar y ver que falla** (no compila: faltan cabeceras).
- [ ] **Step 3: Implementar** `DelayLine` y `EnvelopeFollower`.
- [ ] **Step 4: Ejecutar tests** → PASS.
- [ ] **Step 5: Commit** `chore: scaffold CMake, test harness, delay line and envelope follower`.

### Task 2: `PitchShifter`

**Files:** Create `dsp/PitchShifter.h` · Test `tests/PitchShifterTests.cpp`

**Interfaces:**
- Consumes: `ei::DelayLine`.
- Produces: `class ei::PitchShifter { void prepare(double sr); void reset(); void setSemitones(float st); float process(float x); };` — granular de dos cabezales desfasados 180°, grano de 60 ms, ventana Hann; `st` recortado a ±12; con `st == 0` es un retardo puro de medio grano.

- [ ] **Step 1: Tests** — `Pitch_octaveUp`: seno de 440 Hz con +12 st → frecuencia estimada por cruces por cero en 1 s = 880 Hz ± 3 %. `Pitch_fifthDown`: −7 st → 293,7 Hz ± 3 %. `Pitch_unityGain`: RMS de salida dentro de ±3 dB del de entrada. `Pitch_zeroIsClean`: con 0 st la salida es la entrada retrasada (error < 1e-4).
- [ ] **Step 2: Ver que falla.**
- [ ] **Step 3: Implementar.**
- [ ] **Step 4: Tests PASS.**
- [ ] **Step 5: Commit** `feat: granular pitch shifter`.

### Task 3: `Fdn`

**Files:** Create `dsp/Fdn.h`, `dsp/Fdn.cpp` · Test `tests/FdnTests.cpp`

**Interfaces:**
- Consumes: `ei::DelayLine`, `ei::PitchShifter`.
- Produces:
```cpp
class ei::Fdn {
public:
  static constexpr int kLines = 16;
  void prepare(double sampleRate);
  void reset();
  void setSize(float meters);            // 1..1000; suavizado internamente (≈ 200 ms)
  void setDiffusion(float amount01);
  void setDamping(float bipolar);        // -1..+1
  void setPitch(float semitones, float mix01);
  void setRelease(float rt60Seconds, bool infinite);
  void setHold(float hold01);            // 1 = realimentación unidad (sustain)
  void setFreeze(bool on);
  void setWidth(float width);            // 0..2
  void process(float inL, float inR, float& outL, float& outR);
};
```
- Decisiones:
  - Longitud base de línea *i* en segundos = `size / 343 × r[i]`, con `r` = 16 razones fijas en [0,35, 1,0] elegidas para que las longitudes en muestras sean primas entre sí a 48 kHz; se recorta a un máximo de 3 s por línea (un Size de 1000 m satura en ese máximo).
  - Ganancia por línea `g_i = lerp(10^(−3·T_i / rt60), 1, hold)`; con `infinite` o freeze, `g_i = 1`.
  - Damping: paso bajo de un polo si es negativo (corte de 20 kHz a 500 Hz); si es positivo, shelving de agudos de hasta +3 dB por vuelta. El saturador acota el resultado.
  - Pitch: solo las líneas 0, 4, 8 y 12 llevan `PitchShifter` (coste), mezclado con `mix01`.
  - Saturador `tanh` tras la matriz; la matriz Hadamard se normaliza por `1/4`.
  - Difusores: 4 pasa-todo Schroeder por canal (retardos 4,7 / 3,6 / 12,7 / 9,3 ms; coeficiente = `0,75 × diffusion`).
  - Antidenormales: se suma y resta una constante `1e-18` en el bucle.

- [ ] **Step 1: Tests** —
  - `Fdn_rt60MatchesRelease`: impulso, Size 30, Damping 0, release 2 s → el tiempo en que la envolvente RMS (ventanas de 50 ms) cae 60 dB está en 2 s ± 20 %. Repetir con 0,5 s y 8 s.
  - `Fdn_holdSustains`: con `setHold(1)` la energía entre el segundo 1 y el 5 varía < 1 dB.
  - `Fdn_freezeIgnoresInput`: en freeze, inyectar ruido no cambia la salida respecto a no inyectarlo.
  - `Fdn_boundedAtExtremes`: 10 s de ruido blanco a 0 dBFS con cada combinación de {Size 1, 1000} × {Damping −1, +1} × {Pitch −12, +12, mix 1} × {hold 1} → todas las muestras finitas y |x| ≤ 8.
  - `Fdn_stereoDecorrelated`: correlación L/R de la cola < 0,5 con width 1; con width 0, L == R.
  - `Fdn_silenceDecaysToZero` (Review Focus 5): tras un impulso con release 0,5 s y 20 s de silencio, |salida| < 1e-15.
- [ ] **Step 2: Ver que falla.**
- [ ] **Step 3: Implementar.**
- [ ] **Step 4: Tests PASS.**
- [ ] **Step 5: Commit** `feat: 16-line FDN reverb core`.

### Task 4: `TailEnvelope`

**Files:** Create `dsp/TailEnvelope.h` · Test `tests/TailEnvelopeTests.cpp`

**Interfaces:**
- Produces: `class ei::TailEnvelope { void prepare(double sr); void reset(); void setSustain(float seconds); float process(float inputLevelLin); };` — devuelve `hold` en [0, 1]. Umbral de actividad −50 dBFS. Con sustain 0 devuelve siempre 0. Mientras hay actividad y durante `sustain` segundos después devuelve 1; luego baja a 0 con una rampa de 50 ms.

- [ ] **Step 1: Tests** — `Tail_zeroSustainIsZero`; `Tail_holdsThenReleases`: nivel 0,5 durante 100 ms y luego 0, sustain 2 s → `hold == 1` en t = 1,9 s y `hold == 0` en t = 2,2 s; `Tail_retriggers`: un segundo golpe en t = 1 s extiende el hold hasta t ≈ 3,1 s.
- [ ] **Step 2: Ver que falla.** · **Step 3: Implementar.** · **Step 4: PASS.**
- [ ] **Step 5: Commit** `feat: tail envelope (sustain hold)`.

### Task 5: `Eq6`

**Files:** Create `dsp/Biquad.h`, `dsp/Eq6.h` · Test `tests/Eq6Tests.cpp`

**Interfaces:**
- Produces:
```cpp
struct ei::EqBand { bool on = true; float freq = 1000; float gainDb = 0; float q = 0.707f; };
class ei::Eq6 {
public:
  enum class Type { LowCut, LowShelf, Bell, HighShelf, HighCut };
  void prepare(double sampleRate);
  void reset();
  void setBand(int index, const EqBand& band);   // index 0..5
  void setBand6HighCut(bool highCut);
  void process(float& l, float& r);
  float magnitudeDb(float freqHz) const;         // respuesta total; segura desde el hilo de UI
  static Type typeOf(int index, bool band6HighCut);
};
```
- Decisiones: biquads RBJ, forma directa II transpuesta; frecuencia recortada a [20, 0,45·sr]; Q a [0,1, 10]; ganancia a ±18 dB. Los coeficientes se recalculan solo cuando cambia la banda. `magnitudeDb` lee una copia de coeficientes en `std::atomic<float>`.

- [ ] **Step 1: Tests** — `Eq_flatByDefault` (todas las bandas con ganancia 0 y cortes off → `magnitudeDb` 0 ± 0,1 dB en 50 Hz…15 kHz); `Eq_bellGainAtCenter` (banda 4: 1,5 kHz, +12 dB → +12 ± 0,5 dB medido con un seno procesado, y coincide con `magnitudeDb` ± 0,2 dB); `Eq_lowCutSlope` (corte a 200 Hz: −12 ± 2 dB a 100 Hz); `Eq_highShelf` (+6 dB a 10 kHz → +6 ± 0,7 dB a 18 kHz); `Eq_highCutMode` (banda 6 en high-cut a 5 kHz → < −10 dB a 10 kHz); `Eq_bandOffIsBypass`.
- [ ] **Step 2: Ver que falla.** · **Step 3: Implementar.** · **Step 4: PASS.**
- [ ] **Step 5: Commit** `feat: six-band EQ`.

### Task 6: `Lfo`

**Files:** Create `dsp/Lfo.h` · Test `tests/LfoTests.cpp`

**Interfaces:**
- Produces:
```cpp
class ei::Lfo {
public:
  enum class Mode { Auto, Manual };
  enum class Shape { Sine, Triangle, Saw, Square, Random };
  void prepare(double sampleRate);
  void reset();
  void setMode(Mode);
  void setShape(Shape);
  void setRateHz(float hz);              // 0.01..20
  void setSync(bool on, float beats);    // beats: 32 (8 compases) … 0.125 (1/32)
  void setDepth(float depth01);
  void setPhase(float degrees);
  void setTempo(double bpm);             // <= 0 → 120
  float process(float inputLevel01);     // devuelve [-depth, +depth]
  float phase01() const;                 // para la UI
};
static float ei::divisionToBeats(int index);  // tabla de 9 divisiones: 8 comp., 4 comp., 2 comp., 1 comp., 1/2, 1/4, 1/8, 1/16, 1/32
```
- Auto: seno, periodo de 4 negras, profundidad efectiva `depth × (1 − clamp(inputLevel01, 0, 1))` suavizada con 100 ms.

- [ ] **Step 1: Tests** — `Lfo_sineFrequency` (2 Hz manual → 2 ciclos por segundo ± 1 %); `Lfo_shapesRange` (cada forma queda en [−depth, depth] y alcanza ≥ 0,95·depth); `Lfo_syncFollowsTempo` (120 BPM, 4 negras → periodo 2 s); `Lfo_autoDucksWithInput` (nivel 1 → pico < 0,05·depth; nivel 0 → ≥ 0,95·depth); `Lfo_zeroTempoFallsBack` (Review Focus 1: `setTempo(0)` y `setTempo(-1)` → periodo 2 s en sync de 4 negras, salida finita).
- [ ] **Step 2: Ver que falla.** · **Step 3: Implementar.** · **Step 4: PASS.**
- [ ] **Step 5: Commit** `feat: LFO with auto and manual modes`.

### Task 7: `Reverser`

**Files:** Create `dsp/Reverser.h` · Test `tests/ReverserTests.cpp`

**Interfaces:**
- Produces: `class ei::Reverser { void prepare(double sr, float maxWindowSeconds = 2.0f); void reset(); void setWindowSamples(int n); int latencySamples() const; void process(float& l, float& r); };`
- Decisiones: dos lectores inversos desfasados media ventana, cada uno con ventana Hann (suma constante = 1). `latencySamples()` devuelve la longitud de ventana. Un cambio de ventana se aplica al iniciar el siguiente grano (sin clic). Ventana recortada a [64, maxWindow].

- [ ] **Step 1: Tests** — `Rev_reversesRamp`: ventana de 1000 muestras, entrada = rampa ascendente periódica alineada a la ventana → dentro de cada grano la salida es descendente (correlación con la rampa invertida > 0,95); `Rev_unityLevel`: ruido blanco → RMS de salida dentro de ±1,5 dB; `Rev_latencyReported`; `Rev_windowChangeNoGlitch`: cambiar la ventana de 500 a 5000 a mitad de un seno → ningún salto entre muestras > 0,5; `Rev_clampsWindow` (0 y 1e9 no rompen).
- [ ] **Step 2: Ver que falla.** · **Step 3: Implementar.** · **Step 4: PASS.**
- [ ] **Step 5: Commit** `feat: windowed reverser`.

### Task 8: `Gate`

**Files:** Create `dsp/Gate.h` · Test `tests/GateTests.cpp`

**Interfaces:**
- Produces: `class ei::Gate { void prepare(double sr); void reset(); void setParams(float thresholdDb, float attackMs, float holdMs, float releaseMs, float rangeDb); float process(float detectorLevelLin); bool isOpen() const; };` — devuelve la ganancia lineal a aplicar al wet. Cierra por debajo de `threshold − 3 dB` (histéresis). `rangeDb` ≤ 0 es la ganancia con la puerta cerrada.

- [ ] **Step 1: Tests** — `Gate_opensAboveThreshold` (umbral −30 dB, nivel −20 dB → ganancia ≥ 0,99 tras 5× attack); `Gate_closesToRange` (rango −60 dB → ganancia ≤ 0,0011 tras hold + 5× release); `Gate_holdKeepsOpen` (hold 200 ms: a los 150 ms de silencio la ganancia sigue ≥ 0,99); `Gate_hysteresis` (nivel a −32 dB con umbral −30: si estaba abierta sigue abierta; si estaba cerrada sigue cerrada).
- [ ] **Step 2: Ver que falla.** · **Step 3: Implementar.** · **Step 4: PASS.**
- [ ] **Step 5: Commit** `feat: noise gate`.

### Task 9: `ReverbEngine` (integración)

**Files:** Create `dsp/ReverbEngine.h`, `dsp/ReverbEngine.cpp` · Test `tests/ReverbEngineTests.cpp`

**Interfaces:**
- Consumes: todos los bloques anteriores.
- Produces:
```cpp
struct ei::EngineParams {
  float size = 30, diffusion = 0.7f, damping = -0.3f, pitch = 0, pitchMix = 0, width = 1; bool freeze = false;
  float preDelayMs = 20, sustainSec = 0, releaseSec = 4; bool releaseInfinite = false;
  EqBand eq[6]; bool eqBand6HighCut = false;
  Lfo::Mode lfoMode = Lfo::Mode::Auto; Lfo::Shape lfoShape = Lfo::Shape::Sine;
  float lfoRateHz = 0.5f; bool lfoSync = false; int lfoDivision = 3; float lfoDepth = 0.2f, lfoPhaseDeg = 0;
  enum class LfoTarget { Size, Pitch, Damping, Pan } lfoTarget = LfoTarget::Size;
  bool reverseOn = false; float reverseWindowMs = 500; bool reverseSync = false; int reverseDivision = 5;
  bool gateOn = false; float gateThresholdDb = -30, gateAttackMs = 1, gateHoldMs = 100, gateReleaseMs = 100, gateRangeDb = -80;
  enum class GateSource { Dry, Wet } gateSource = GateSource::Dry;
  float mix = 0.3f, outputDb = 0;
};
struct ei::Meters { float inputLevel = 0, wetLevel = 0, gateGain = 1, lfoValue = 0, hold = 0; };
class ei::ReverbEngine {
public:
  void prepare(double sampleRate, int maxBlockSize);
  void reset();
  void setParams(const EngineParams&);        // una vez por bloque
  void setTempo(double bpm);
  void process(float* left, float* right, int numSamples);  // in situ
  int latencySamples() const;                 // 0 si reverseOn == false
  Meters meters() const;                      // último bloque
  const Eq6& eq() const;
};
```
- Decisiones:
  - Orden de la cadena: el de la sección 3 de la spec.
  - Profundidades del LFO a tope: Size ±50 %, Pitch ±2 st, Damping ±0,5, Pan ±100 %.
  - El dry se retrasa `latencySamples()` cuando el inversor está activo. Al conmutar el inversor, crossfade de 20 ms.
  - Mix de potencia constante: `dry·cos(mix·π/2) + wet·sin(mix·π/2)`. Mix y Output suavizados en 20 ms.
  - Pre-delay con cambio suavizado (≈ 50 ms) para evitar clics.

- [ ] **Step 1: Tests** —
  - `Engine_dryPassthroughAtMixZero` (mix 0 → salida == entrada, error < 1e-6).
  - `Engine_preDelayOffsetsOnset` (mix 1, pre-delay 100 ms: energía < −80 dB antes de 100 ms y presente después).
  - `Engine_sustainThenRelease` (sustain 2 s, release 1 s: el nivel en t = 1,5 s está a menos de 3 dB del de t = 0,5 s; en t = 4 s ha caído > 40 dB).
  - `Engine_gatedReverbCuts` (puerta on, fuente Dry, golpe de 100 ms, hold 100 ms, release 50 ms: wet < −60 dB respecto al pico a los 500 ms; con la puerta off, > −20 dB).
  - `Engine_reverseReportsLatency` (window 500 ms a 48 kHz → 24000; off → 0).
  - `Engine_lfoTargetsAffectOutput` (para cada destino, la salida con depth 1 difiere de la de depth 0).
  - `Engine_noTempoFallback` (Review Focus 1: `setTempo(0)` con LFO e inversor en sync → finito, ventana > 0).
  - `Engine_sampleRates` (Review Focus 2: `prepare` a 44100, 96000, 192000 y vuelta a 44100; RT60 con release 2 s dentro de ±25 % en cada una).
  - `Engine_automationSweepsAreSafe` (Review Focus 3: 20 s de ruido cambiando en cada bloque Size, Pitch, Damping, ventana del inversor y destino del LFO a valores aleatorios en todo su rango → finito, |x| ≤ 8).
  - `Engine_blockSizeInvariant` (Review Focus 4: la misma señal procesada en bloques de 1, 64, 480 y tamaños aleatorios con parámetros fijos → diferencia máxima < 1e-4).
- [ ] **Step 2: Ver que falla.** · **Step 3: Implementar.** · **Step 4: PASS.**
- [ ] **Step 5: Commit** `feat: reverb engine wiring all DSP blocks`.

### Task 10: Plugin JUCE — procesador y parámetros

**Files:**
- Create: `plugin/Parameters.h`, `plugin/Parameters.cpp`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `plugin/CMakeLists.txt`
- Modify: `CMakeLists.txt` (FetchContent de JUCE 8.0.4 cuando `EI_BUILD_PLUGIN=ON`)

**Interfaces:**
- Consumes: `ei::ReverbEngine`, `ei::EngineParams`, `ei::Meters`.
- Produces:
  - `juce::AudioProcessorValueTreeState::ParameterLayout ei::createParameterLayout();`
  - `ei::EngineParams ei::readParams(const juce::AudioProcessorValueTreeState&);`
  - IDs de parámetros (cadenas estables, no cambiarlas nunca): `size, diffusion, damping, pitch, pitchMix, width, freeze, preDelay, sustain, release, releaseInf, eq{1..6}On, eq{1..6}Freq, eq{1..6}Gain, eq{1..6}Q, eq6HighCut, lfoMode, lfoShape, lfoRate, lfoSync, lfoDivision, lfoDepth, lfoPhase, lfoTarget, revOn, revWindow, revSync, revDivision, gateOn, gateThreshold, gateAttack, gateHold, gateRelease, gateRange, gateSource, mix, output`.
  - `class ImpossibleSpacesProcessor : public juce::AudioProcessor` con `apvts` público y `struct MeterFrame { float in, wet, gateGain, lfo, hold; }` expuesto mediante `bool popMeterFrame(MeterFrame&)` (cola `juce::AbstractFifo` de 512 entradas, una por bloque).
- Decisiones:
  - Buses: entrada y salida estéreo; se acepta también mono→estéreo. `isBusesLayoutSupported` rechaza el resto.
  - Tempo desde `getPlayHead()->getPosition()->getBpm()`; si no hay, 0 (el motor recurre a 120).
  - `setLatencySamples(engine.latencySamples())` cuando cambie.
  - `juce::ScopedNoDenormals` en `processBlock`. Si el bloque supera `maxBlockSize`, se trocea.
  - Rangos con skew: frecuencias y tiempos logarítmicos (`NormalisableRange` con `setSkewForCentre`). `release` 0,1–60 s; `releaseInf` booleano aparte.
  - `getTailLengthSeconds()` devuelve `release + sustain` (o 60 si infinito).
  - Estado: `apvts.copyState()` ↔ XML en `get/setStateInformation`.
  - Hasta la Task 11, `createEditor()` devuelve `juce::GenericAudioProcessorEditor`.

- [ ] **Step 1: Escribir** `Parameters.*` y `PluginProcessor.*` y el CMake (`juce_add_plugin` con los valores de Global Constraints, `COPY_PLUGIN_AFTER_BUILD TRUE`).
- [ ] **Step 2: Comprobación de compilación en Linux (mejor esfuerzo):** `cmake -B build-plugin -DEI_BUILD_PLUGIN=ON -DEI_LINUX_COMPILE_CHECK=ON && cmake --build build-plugin --target ImpossibleSpaces_Standalone`. Con `EI_LINUX_COMPILE_CHECK` los formatos son solo `Standalone`. Si las dependencias de JUCE en Linux no pueden instalarse, se anota y la verificación de compilación pasa a la Task 12.
- [ ] **Step 3: Tests del DSP siguen en verde.**
- [ ] **Step 4: Commit** `feat: JUCE processor and parameter layout`.

### Task 11: Interfaz

**Files:**
- Create: `plugin/ui/Theme.h`, `plugin/ui/Knob.h`, `plugin/ui/EnvelopeDisplay.{h,cpp}`, `plugin/ui/EqDisplay.{h,cpp}`, `plugin/ui/LfoView.{h,cpp}`, `plugin/PluginEditor.{h,cpp}`
- Modify: `plugin/PluginProcessor.cpp` (`createEditor`), `plugin/CMakeLists.txt`

**Interfaces:**
- Consumes: `apvts`, `popMeterFrame`, `engine.eq().magnitudeDb`, IDs de la Task 10.
- Produces: `class ImpossibleSpacesEditor : public juce::AudioProcessorEditor, private juce::Timer` — 1000×640, no redimensionable, temporizador a 30 Hz.
- Decisiones:
  - `Theme.h`: fondo `#0E1016`, panel `#171A23`, texto `#E6E8EE`, acento (envolvente) `#7C5CFF`, entrada `#3DD6C6`, reverb `#FF7A59`, puerta `#F2C94C`.
  - Distribución: fila superior = `EnvelopeDisplay` (620×300) + `EqDisplay` (360×300). Fila inferior = cinco secciones: Espacio, LFO, Inversor, Puerta, Salida.
  - `EnvelopeDisplay`: eje X logarítmico de 1 ms a 80 s. Tres asas: pre-delay (X), fin de sustain (X = pre-delay + sustain), fin de release (X = anterior + release). Arrastrar llama a `beginChangeGesture / setValueNotifyingHost / endChangeGesture`. Doble clic en el asa de release conmuta `releaseInf`. Detrás, historial de 6 s (180 cuadros) con el nivel de entrada y de wet en dB (−72…0), línea del umbral de puerta cuando `gateOn`, y un indicador de puerta abierta/cerrada.
  - `EqDisplay`: curva de 20 Hz a 20 kHz, ±18 dB, 6 nodos; arrastre = freq/gain, rueda = Q, doble clic = on/off. Los cortes solo se mueven en X.
  - `LfoView`: dibuja un ciclo de la forma actual y un punto en la fase actual.
  - `Knob`: `juce::Slider` rotatorio + etiqueta + `SliderAttachment`. Los controles que no aplican se atenúan (p. ej. `lfoRate` cuando `lfoSync` o modo Auto; `revWindow` cuando `revSync`).

- [ ] **Step 1: Implementar** tema, `Knob` y el editor con las cinco secciones.
- [ ] **Step 2: Implementar** `EnvelopeDisplay`, `EqDisplay`, `LfoView`.
- [ ] **Step 3: Compilación en Linux (mejor esfuerzo)** como en la Task 10; si es posible, lanzar el Standalone bajo `xvfb` y capturar pantalla para revisar la distribución.
- [ ] **Step 4: Commit** `feat: plugin editor with envelope, EQ and LFO displays`.

### Task 12: CI en macOS, `auval` y artefacto

**Files:** Create `.github/workflows/build.yml`

**Decisiones:**
- Disparadores: `push` a cualquier rama y `workflow_dispatch`.
- Job `dsp-tests` (`ubuntu-latest`): comando de tests de Global Constraints.
- Job `macos-au` (`macos-latest`): `cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`; `cmake --build build --config Release`; tests del DSP; copiar el `.component` a `~/Library/Audio/Plug-Ins/Components/`; `killall -9 AudioComponentRegistrar || true`; `auval -v aufx ImSp Vita`; `lipo -archs` sobre el binario para confirmar `x86_64 arm64`; firma ad-hoc `codesign --force --deep -s -`; empaquetar con `ditto -c -k --keepParent` y subir `ImpossibleSpaces-AU.zip` y `ImpossibleSpaces-Standalone.zip` con `actions/upload-artifact@v4`.

- [ ] **Step 1: Escribir el workflow, commit y push.**
- [ ] **Step 2: Vigilar la ejecución** (herramientas de GitHub Actions) y corregir hasta que ambos jobs estén en verde. Criterio: `auval` termina con `AU VALIDATION SUCCEEDED`.
- [ ] **Step 3: Commit** de las correcciones necesarias.

### Task 13: README

**Files:** Modify `README.md`

**Contenido (en español), en este orden:** qué es y concepto de "espacios imposibles" · captura o esquema de la interfaz · descarga e instalación en Logic (ruta `~/Library/Audio/Plug-Ins/Components/`, `xattr -dr com.apple.quarantine`, reescaneo en el Gestor de plug-ins) · guía de uso por sección con todos los parámetros y rangos · recetas (cola infinita, shimmer ascendente, reverb inversa, gated de los 80, sala que respira) · arquitectura y cadena de señal · cada bloque DSP explicado (FDN, envolvente, pitch, EQ, LFO, inversor, puerta) con sus decisiones · proceso de desarrollo (decisiones tomadas, alternativas descartadas y por qué, cómo se verificó) · compilar en local (macOS) y ejecutar los tests · CI · limitaciones conocidas y licencia de JUCE · checklist de validación en Logic (la de la Task 14).

- [ ] **Step 1: Escribir el README** comprobando cada rango y cada ID contra `plugin/Parameters.cpp`.
- [ ] **Step 2: Commit** `docs: detailed README`.

### Task 14: Validación e instalación en Logic Pro

Requiere el Mac del autor. Se intenta conectar la sesión a su Mac (Remote Control); si no es posible, el autor ejecuta los pasos y comunica los resultados.

- [ ] **Step 1: Instalar** el `.component` del artefacto de CI en `~/Library/Audio/Plug-Ins/Components/` y quitar la cuarentena.
- [ ] **Step 2: Validar** con `auval -v aufx ImSp Vita` en el Mac → `AU VALIDATION SUCCEEDED`.
- [ ] **Step 3: Logic Pro**: reescanear en *Ajustes → Gestor de plug-ins*; comprobar que aparece en *Audio Units → VitaPlug → Impossible Spaces* y que carga en una pista estéreo.
- [ ] **Step 4: Checklist funcional**: suena con los valores por defecto · la pantalla muestra entrada y reverb · arrastrar pre-delay/sustain/release cambia el sonido · EQ audible y curva coherente · LFO auto y manual · inversor (y compensación de latencia: el dry sigue a tempo) · puerta con fuente Dry sobre una caja · automatización de un parámetro · guardar y reabrir el proyecto conserva el estado · bounce offline sin fallos · uso de CPU razonable.
- [ ] **Step 5: Recoger impresiones de sonido** y abrir una ronda de ajustes si hace falta (nueva tarea, fuera de este plan).
