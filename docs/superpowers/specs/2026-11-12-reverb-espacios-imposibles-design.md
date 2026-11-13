# Reverb de Espacios Imposibles — Spec de diseño

Fecha: 2026-11-12 · Estado: pendiente de revisión

## 1. Objetivo

Plugin de reverb estéreo en formato Audio Unit (AUv2, tipo `aufx`) para Logic Pro, cuyo
carácter son los "espacios imposibles": salas que no pueden existir físicamente (tamaños
kilométricos, colas que no decaen, que cambian de tono o que ganan brillo con el tiempo).

Uso previsto: personal, en el Mac del autor. Sin firma ni notarización.

**Criterios de éxito**

1. El `.component` compila en macOS (universal arm64 + x86_64) y pasa `auval`.
2. Logic Pro lo carga como efecto estéreo y todos los parámetros son automatizables.
3. Contiene todo lo pedido: EQ de 6 bandas, LFO auto/manual, pantalla editable de
   pre-delay/sustain/release con visualización de la señal, inversor y puerta de ruido.
4. El motor DSP es estable (sin NaN, sin explosiones) en todo el rango de parámetros.
5. El README documenta concepto, arquitectura, DSP, parámetros, compilación e instalación.

## 2. Decisiones tomadas

| Decisión | Elección |
|---|---|
| Framework | JUCE 8 + CMake (JUCE vía `FetchContent`, versión fijada) |
| Compilación | GitHub Actions en runner macOS; también compilable en local |
| Concepto sonoro | Un único motor muy paramétrico, sin modos |
| Algoritmo | FDN (Feedback Delay Network) de 16 líneas |
| LFO AUTO | Sincronizado al tempo, profundidad dependiente del nivel de entrada |
| Formatos | AU + Standalone (este último solo para probar sin Logic) |

## 3. Cadena de señal

```
Entrada ─┬───────────────────────────────────────────────────────────── dry ─┐
         └→ Pre-delay → Difusores → FDN 16 → Inversor → EQ 6 → Puerta → wet ─┴→ Mix → Salida
              ▲                       ▲                            ▲
              │            TailEnvelope (sustain/release)          │
              │            LFO (size / pitch / damping / pan)      │
              └──────────── detector de nivel de entrada ──────────┘ (sidechain puerta, LFO auto, envolvente)
```

## 4. Bloques DSP

Todos son clases C++17 puras en `dsp/`, sin dependencia de JUCE, con la interfaz
`prepare(sampleRate)`, `reset()` y proceso muestra a muestra o por bloque. No reservan
memoria ni bloquean en el hilo de audio.

### 4.1 `Fdn` — motor de reverb
- 16 líneas de retardo con longitudes mutuamente primas, escaladas por **Size**.
- Matriz de mezcla Hadamard 16×16 (sin pérdidas, coste O(n log n)).
- Lectura con interpolación para permitir modulación y cambios de Size sin clics.
- Por línea, dentro del bucle:
  - ganancia de realimentación (dictada por `TailEnvelope`);
  - filtro de **Damping bipolar**: valores negativos = paso bajo (la cola se oscurece),
    positivos = realce de agudos compensado (la cola se abrillanta);
  - **desplazador de tono** granular (dos cabezales con crossfade), ±12 semitonos,
    aplicado a una fracción de la realimentación (**Pitch Mix**);
  - saturador suave (`tanh`) que acota la energía con realimentación ≥ 1.
- **Difusores** de entrada: 4 pasa-todo en serie por canal, cantidad controlada por **Diffusion**.
- **Freeze**: corta la entrada al bucle y fija la realimentación en 1.
- Salida estéreo por combinación decorrelada de las líneas; **Width** por matriz M/S.

### 4.2 `TailEnvelope` — pre-delay / sustain / release
- **Pre-delay**: línea de retardo 0–500 ms antes del motor.
- Detecta actividad en la entrada (seguidor de envolvente con umbral fijo bajo).
- Mientras hay señal y durante **Sustain** segundos tras su fin: realimentación ≈ 1
  (la cola se mantiene a nivel pleno).
- Después: realimentación calculada para que el RT60 sea **Release** (o 1 si es infinito).
- La transición entre ambos estados es suavizada (sin escalón).

### 4.3 `Eq6` — ecualizador de 6 bandas (sobre el wet)
Biquads (fórmulas RBJ) en serie, coeficientes suavizados:

| Banda | Tipo | Frecuencia por defecto |
|---|---|---|
| 1 | Low-cut 12 dB/oct | 40 Hz |
| 2 | Low-shelf | 150 Hz |
| 3 | Campana | 400 Hz |
| 4 | Campana | 1,5 kHz |
| 5 | Campana | 5 kHz |
| 6 | High-shelf / High-cut (conmutable) | 10 kHz |

Cada banda: on/off, frecuencia, ganancia (±18 dB; no aplica a los cortes) y Q.
Expone la respuesta en magnitud para dibujar la curva.

### 4.4 `Lfo`
- Formas: seno, triángulo, sierra, cuadrada, aleatoria (sample & hold suavizado).
- **MANUAL**: velocidad libre (0,01–20 Hz) o sincronizada a tempo (de 8 compases a 1/32),
  profundidad y fase inicial.
- **AUTO**: seno sincronizado al tempo del host (1 compás); la profundidad efectiva es
  `Depth × (1 − nivel de entrada normalizado)`, es decir, más movimiento en las colas y
  menos en los ataques.
- **Destino** (uno): Size, Pitch, Damping o Pan del wet.
- Si el host no informa de tempo, se usan 120 BPM.

### 4.5 `Reverser` — reverb inversa
- Graba el wet en ventanas de longitud **Window** (50 ms–2 s, o sincronizada a tempo) y
  reproduce cada ventana al revés, con doble búfer solapado al 50 % y ventana de Hann
  para evitar clics.
- Latencia = longitud de la ventana. El plugin la declara al host
  (`setLatencySamples`) y retrasa el dry en la misma cantidad para mantener la alineación.
- Desactivado: latencia 0 y bypass exacto.

### 4.6 `Gate` — puerta de ruido
- Parámetros: umbral, attack, hold, release y rango (atenuación máxima).
- Fuente de detección: **entrada seca** (gated reverb clásico: la cola se corta cuando
  calla la fuente) o **wet**.
- Histéresis de 3 dB para evitar castañeteo.

### 4.7 Mezcla
**Mix** dry/wet (ley de potencia constante) y **Output** (ganancia final).

## 5. Parámetros (automatizables)

| Grupo | Parámetro | Rango | Por defecto |
|---|---|---|---|
| Espacio | Size | 1–1000 m | 30 m |
| | Diffusion | 0–100 % | 70 % |
| | Damping | −100…+100 % | −30 % |
| | Pitch | −12…+12 st | 0 |
| | Pitch Mix | 0–100 % | 0 % |
| | Width | 0–200 % | 100 % |
| | Freeze | on/off | off |
| Envolvente | Pre-delay | 0–500 ms | 20 ms |
| | Sustain | 0–10 s | 0 s |
| | Release | 0,1–60 s + ∞ | 4 s |
| EQ | ×6: On, Freq, Gain, Q; Banda 6: modo | ver 4.3 | plano |
| LFO | Mode (Auto/Manual), Shape, Rate, Sync, Division, Depth, Phase, Target | ver 4.4 | Auto, Size, 20 % |
| Inversor | On, Window, Sync, Division | 50 ms–2 s | off, 500 ms |
| Puerta | On, Threshold, Attack, Hold, Release, Range, Source | −80…0 dB, 0,1–100 ms, 0–2 s, 5 ms–2 s, 0…−80 dB | off |
| Salida | Mix, Output | 0–100 %, −24…+12 dB | 30 %, 0 dB |

Los parámetros continuos se suavizan para evitar ruido de cremallera. El estado se
guarda/restaura con `AudioProcessorValueTreeState`.

## 6. Interfaz

Ventana fija ~1000×640, tema oscuro, dibujada con primitivas de JUCE (sin imágenes).

- **Pantalla central**: eje de tiempo logarítmico. Curva de envolvente con tres puntos
  arrastrables (pre-delay, fin de sustain, fin de release). Detrás, historial en
  desplazamiento de los últimos segundos: nivel de entrada y nivel de la reverb
  superpuestos en colores distintos, línea horizontal del umbral de puerta e indicador
  de puerta abierta/cerrada.
- **Panel EQ**: curva de respuesta con 6 nodos arrastrables (arrastre = frecuencia y
  ganancia, rueda = Q, doble clic = on/off).
- **Secciones** con diales: Espacio, LFO (con vista de la forma de onda y su fase actual),
  Inversor, Puerta, Salida.
- El hilo de audio envía los niveles a la interfaz mediante una cola FIFO sin bloqueo;
  la interfaz refresca a 30 Hz.

## 7. Estructura del repositorio

```
CMakeLists.txt
dsp/            DelayLine, Fdn, PitchShifter, TailEnvelope, Eq6, Lfo, Reverser, Gate, ReverbEngine
plugin/         PluginProcessor, PluginEditor, Parameters, componentes de UI
tests/          tests del DSP (ejecutable CTest, sin JUCE)
.github/workflows/build.yml
docs/           spec y plan
README.md
```

`ReverbEngine` orquesta la cadena completa y es lo único que usa `PluginProcessor`.

## 8. Verificación

**En Linux (tests automáticos del DSP):**
- Estabilidad: ruido e impulsos con parámetros en los extremos → salida finita y acotada.
- RT60 medido ≈ Release (tolerancia ±20 %) con Sustain = 0.
- Sustain: el nivel de la cola se mantiene durante el tiempo indicado antes de caer.
- Freeze: la energía no decae.
- EQ: magnitud medida en las frecuencias centrales ≈ ganancia configurada.
- Inversor: una rampa de entrada sale invertida en el tiempo dentro de la ventana; latencia correcta.
- Puerta: atenúa por debajo del umbral y respeta el hold.
- LFO: frecuencia y formas correctas; modo auto reduce profundidad con señal presente.

**En GitHub Actions (macOS):** compilación de AU + Standalone, tests del DSP, `auval -v aufx`
y subida del `.component` como artefacto.

**Fuera de alcance de la verificación automática:** la calidad sonora y el comportamiento
real en Logic Pro, que requieren escucha por parte del autor.

## 9. Fuera de alcance

VST3/AAX, AUv3/iOS, presets de fábrica, firma y notarización, instalador, redimensionado
de la ventana, entrada de sidechain externa, MIDI.

## 10. Riesgos

- **Sonido no validado por escucha**: se mitiga con tests objetivos y valores por defecto
  conservadores; el ajuste fino queda para iteraciones con el autor.
- **Plugin sin firmar**: macOS lo pone en cuarentena al descargarlo; el README documenta
  `xattr -dr com.apple.quarantine`.
- **Licencia de JUCE**: AGPLv3 o licencia gratuita Starter; válido para uso personal y
  código abierto. A revisar si se distribuye comercialmente.
- **CPU**: 16 desplazadores de tono en el bucle; si resulta costoso, se reduce a 4 líneas
  con pitch (decisión de implementación, sin cambio de interfaz).
