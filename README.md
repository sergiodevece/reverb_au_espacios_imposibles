# IMPOSSIBLE SPACES

Reverb en formato **Audio Unit** para **Logic Pro**, basada en espacios que no pueden existir: salas de un kilómetro, colas que no decaen, que suben de tono a cada rebote o que ganan brillo con el tiempo.

*Marca: VitaPlug · Diseño: SerxDev*

![Interfaz de Impossible Spaces](docs/img/interfaz.png)

> Captura real del plugin, tomada de la versión autónoma compilada en Linux y sin audio entrando (por eso la pantalla de señal está vacía).

---

## Índice

1. [Qué incluye](#qué-incluye)
2. [Estado del proyecto](#estado-del-proyecto)
3. [Instalación en Logic Pro](#instalación-en-logic-pro)
4. [Guía de uso](#guía-de-uso)
5. [Recetas](#recetas)
6. [Cómo funciona por dentro](#cómo-funciona-por-dentro)
7. [Proceso de desarrollo](#proceso-de-desarrollo)
8. [Compilar y probar](#compilar-y-probar)
9. [Validación en Logic: checklist](#validación-en-logic-checklist)
10. [Limitaciones conocidas](#limitaciones-conocidas)
11. [Licencias](#licencias)

---

## Qué incluye

| Pedido | Cómo está resuelto |
|---|---|
| Reverb de espacios imposibles | Un único motor muy paramétrico: red de 16 retardos realimentados (FDN) con tamaño de 1 m a 1 km, cola infinita, cambio de tono dentro del bucle y damping que puede oscurecer **o abrillantar** |
| Ecualizador de 6 bandas | Low-cut, low-shelf, 3 campanas y high-shelf conmutable a high-cut, con curva editable en pantalla |
| LFO auto o editable | **AUTO**: sincronizado al tempo y con profundidad que se retira cuando suena la fuente. **MANUAL**: forma, velocidad (Hz o tempo), profundidad y fase. Destino: Size, Pitch, Damping o Pan |
| Pantalla para editar pre-delay, sustain y release | Pre-delay y ataque, más una curva de cinco puntos libres (tiempo y nivel) que dibuja el sustain y la caída como se quiera |
| Ver el impacto de la señal sobre la reverb | Historial en tiempo real del nivel de entrada (barras) y de la reverb (área) detrás de la envolvente |
| Inversor | Reverb inversa en tiempo real, con ventana libre o sincronizada al tempo y latencia compensada |
| Puerta de ruido | Para reverbs gateadas: detecta desde la señal seca (el gated clásico) o desde la propia cola |

## Estado del proyecto

Qué está comprobado y qué no, sin adornos:

| Aspecto | Estado |
|---|---|
| Motor DSP | Verificado con 86 tests automáticos (estabilidad, tiempos de caída, EQ, inversión, puerta, automatización brusca, varias frecuencias de muestreo) |
| Compilación del plugin | Verificada en Linux (versión autónoma) y en GitHub Actions sobre macOS (AU universal) — ver la pestaña *Actions* |
| Validación `auval` | La ejecuta GitHub Actions en cada push |
| Carga y uso en Logic Pro | Validado por el autor en su Mac: pasa `auval`, carga en Logic y guarda y recupera los ajustes |
| Calidad sonora | Primera escucha del autor: cola natural y densa, cambio de tono limpio, nivel correcto. Detectó clics esporádicos al arrancar la reproducción y en modo ciclo; ver [Problemas encontrados](#4-problemas-encontrados-y-cómo-se-resolvieron) |

## Instalación en Logic Pro

### 1. Descargar

En GitHub: pestaña **Actions** → último run en verde del workflow **build** → sección *Artifacts* → **ImpossibleSpaces-AU**. Descomprime hasta obtener `Impossible Spaces.component`.

(O compílalo tú: ver [Compilar y probar](#compilar-y-probar).)

### 2. Instalar

```bash
mkdir -p ~/Library/Audio/Plug-Ins/Components
cp -R "Impossible Spaces.component" ~/Library/Audio/Plug-Ins/Components/
```

### 3. Quitar la cuarentena

El plugin no está firmado con un certificado de Apple, así que macOS lo bloquea al venir de una descarga:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Impossible Spaces.component"
```

### 4. Validar

```bash
killall -9 AudioComponentRegistrar 2>/dev/null
auval -v aufx Imsp Vita
```

Debe terminar con `AU VALIDATION SUCCEEDED`.

### 5. Cargar en Logic

Abre Logic. Si no aparece: **Logic Pro → Ajustes → Gestor de plug-ins**, busca *Impossible Spaces* y pulsa **Restablecer y volver a explorar la selección**.

Lo encontrarás en una pista de audio o auxiliar, en **Audio FX → Audio Units → VitaPlug → Impossible Spaces**.

## Guía de uso

### Pantalla TAIL ENVELOPE

El eje horizontal es tiempo en escala logarítmica (de 1 ms a 80 s); el vertical, nivel, de +12 dB arriba a −60 dB (silencio) abajo. La línea brillante es la forma que tendrá la reverb.

**Cómo entra la reverb**

| Punto | Parámetro | Rango | Qué hace |
|---|---|---|---|
| **PRE** (rombo, abajo) | Pre-Delay | 0–500 ms | Silencio entre la señal y el arranque de la reverb |
| **ATT** (círculo, en la subida) | Attack | 0–2 s | La reverb entra en fundido cada vez que la fuente arranca tras un silencio. Suaviza los ataques; con valores largos, un golpe corto apenas llega a entrar |

**La cola: cinco puntos libres**

Los rombos numerados del **1 al 5** dibujan la cola. Cada uno se arrastra libremente en las dos direcciones: en horizontal eliges **cuándo** (de 0 a 60 s desde que calla la fuente) y en vertical **a qué nivel** (de +12 dB a −60 dB). La reverb sigue esa forma tramo a tramo:

- Tramo **plano**: la cola se queda sostenida.
- Tramo **descendente**: decae, más deprisa cuanto más inclinado.
- Tramo **ascendente**: la cola **crece**, algo que ninguna sala real puede hacer.

Detalles de manejo:

- Al acercar el ratón a un punto aparecen su tiempo y su nivel.
- **Doble clic** devuelve el punto a su posición de fábrica.
- Los puntos no pueden cruzarse en el tiempo: cada uno se mueve entre su vecino anterior y el siguiente. Si empujas uno contra otro, se detiene ahí.
- Si el **punto 5** no está abajo del todo, la cola **no muere nunca**: se queda a ese nivel (la pantalla lo indica con una línea discontinua y el símbolo ∞). Para una cola normal, deja el 5 en −60 dB.
- Por defecto los cinco puntos forman una caída exponencial de 4 segundos.

Cada punto son dos parámetros (`Tail N Time` y `Tail N Level`), así que el movimiento se puede **automatizar**: pon la pista en modo *Touch* o *Latch* y arrastra el punto durante la reproducción.

A tener en cuenta:

- **La curva la dispara la señal.** El ataque empieza cuando la fuente arranca; la curva de cinco puntos, cuando calla. Mientras la fuente suena, la reverb decae a la pendiente media de la curva. Con música continua, sin silencios, la forma dibujada no llega a recorrerse: pruébala con golpes o frases sueltas.
- **Tramos ascendentes largos o muy empinados** hacen crecer la cola hasta el limitador interno, donde satura. Puede ser un efecto buscado, pero conviene vigilar el nivel.
- **Un salto vertical** (dos puntos en el mismo instante) no puede ser instantáneo: el nivel cambia como máximo a 600 dB por segundo al bajar y 60 al subir.

Detrás de la curva se ve la señal en tiempo real, desplazándose de derecha a izquierda (unos 5 segundos de historia) y en la misma escala de nivel:

- **Barras claras**: nivel de entrada.
- **Área verde**: nivel de la reverb.
- **Línea discontinua lima**: umbral de la puerta (solo con la puerta activada). El testigo **GATE** se enciende cuando está abierta.

Ojo: la envolvente usa el eje de tiempo logarítmico y la señal en vivo uno lineal; comparten pantalla y escala de nivel, pero no escala de tiempo.

### Pantalla EQ

Ecualiza solo la reverb, no la señal seca.

| Banda | Tipo | Por defecto |
|---|---|---|
| 1 | Low-cut 12 dB/oct | 40 Hz, apagada |
| 2 | Low-shelf | 150 Hz |
| 3, 4, 5 | Campana | 400 Hz, 1,5 kHz, 5 kHz |
| 6 | High-shelf o high-cut (selector **SHELF / CUT**) | 10 kHz |

- **Arrastrar** un nodo: frecuencia (20 Hz–20 kHz) y ganancia (±18 dB). Los filtros de corte solo se mueven en frecuencia.
- **Rueda** sobre un nodo: Q (0,1–10).
- **Doble clic**: enciende o apaga la banda.

### SPACE

| Control | Rango | Por defecto | Qué hace |
|---|---|---|---|
| **SIZE** | 1–1000 m | 30 m | Tamaño del espacio. Moverlo mientras suena produce un efecto Doppler |
| **DIFFUSION** | 0–100 % | 70 % | Emborrona el arranque de la reverb: bajo = se distinguen los primeros ecos, alto = nube desde el principio |
| **DENSITY** | 0–100 % | 50 % | Textura de toda la cola. 0 %: rebotes sueltos que se repiten sin mezclarse. 50 %: la red normal. 100 %: más densa, con difusores dentro del propio bucle. Para ecos realmente separados, baja también DIFFUSION |
| **DAMPING** | −100…+100 % | −30 % | Negativo: los agudos mueren antes y la cola se oscurece. Positivo: mueren antes los graves y la cola se va quedando en puro brillo (imposible en una sala real) |
| **WIDTH** | 0–200 % | 100 % | Anchura estéreo; 0 = mono |
| **PITCH** | ±12 semitonos | 0 | Cambio de tono dentro de la realimentación: la cola sube o baja un poco más en cada vuelta |
| **PITCH MIX** | 0–100 % | 0 % | Cuánta realimentación pasa por el cambio de tono. A 0, PITCH no hace nada |
| **FREEZE** | on/off | off | Congela lo que haya dentro: deja de entrar señal y nada decae |

### LFO

| Control | Qué hace |
|---|---|
| **ON/OFF** | Apaga el LFO por completo (con un fundido de unas décimas de segundo, para que no se note el salto) |
| **AUTO / MANUAL** | AUTO: seno de un compás, sincronizado al tempo; se retira cuando suena la fuente (por encima de −12 dBFS desaparece) y vuelve en las colas. MANUAL: tú decides todo |
| **SHAPE** | Seno, triángulo, sierra, cuadrada, aleatoria (solo MANUAL) |
| **TARGET** | Destino: SIZE (±50 %), PITCH (±2 semitonos), DAMP (±50 %) o PAN (de lado a lado) — valores a profundidad máxima |
| **RATE** | 0,01–20 Hz. Con **SYNC**, de 8 compases a 1/32 |
| **DEPTH** | 0–100 % (20 % por defecto) |
| **PHASE** | 0–360°, punto de arranque de la onda (solo MANUAL) |

Para que TARGET = PITCH se oiga, PITCH MIX debe ser mayor que 0.

### REVERSE

Reproduce la reverb al revés por tramos: en vez de apagarse, cada tramo crece.

| Control | Qué hace |
|---|---|
| **ON/OFF** | Activa el inversor |
| **WINDOW** | Longitud del tramo, 50 ms–2 s. Con **SYNC**, de 1/32 a lo que quepa en 2 s según el tempo |

Activarlo introduce una latencia igual a la ventana. El plugin se la declara a Logic, que la compensa, y retrasa la señal seca lo mismo para que siga alineada.

### GATE

| Control | Rango | Por defecto | Qué hace |
|---|---|---|---|
| **THRESHOLD** | −80…0 dB | −30 dB | Nivel por encima del cual la puerta se abre |
| **RANGE** | −80…0 dB | −80 dB | Cuánto atenúa cerrada. A 0 dB no hace nada |
| **SOURCE** | DRY / WET | DRY | DRY: manda la señal original (la reverb se corta cuando calla la fuente). WET: manda la propia reverb |
| **ATTACK** | 0,1–100 ms | 1 ms | Velocidad de apertura |
| **HOLD** | 0–2000 ms | 100 ms | Tiempo que sigue abierta tras caer bajo el umbral |
| **RELEASE** | 5–2000 ms | 100 ms | Velocidad de cierre |

### OUTPUT

| Control | Rango | Por defecto |
|---|---|---|
| **MIX** | 0–100 % | 30 %. En un envío auxiliar, ponlo al 100 % |
| **LEVEL** | −24…+12 dB | 0 dB |

Todos los controles vuelven a su valor por defecto con doble clic, y todos son automatizables.

## Recetas

Puntos de partida, no verdades absolutas (recuerda: están sin escuchar).

| Efecto | Ajustes |
|---|---|
| **Catedral infinita** | SIZE 150 m · DAMPING −20 % · los cinco puntos arriba, en 0 dB (el 5 no baja: cola infinita). Cada nota se suma a las anteriores |
| **Shimmer ascendente** | PITCH +12 · PITCH MIX 30–50 % · DAMPING −40 % · puntos en caída suave hasta los 8 s |
| **Abismo** | PITCH −5 · PITCH MIX 60 % · SIZE 300 m · DAMPING −60 % |
| **Reverb inversa** | REVERSE ON · WINDOW SYNC 1/4 · caída corta (punto 5 en 1,5 s) · MIX 100 % en un auxiliar |
| **Gated de los 80** | GATE ON · SOURCE DRY · THRESHOLD ajustado a la caja · HOLD 150 ms · RELEASE 30 ms · SIZE 40 m |
| **Sala que respira** | LFO MANUAL · TARGET SIZE · RATE 0,1 Hz · DEPTH 60 % |
| **Muro de sustain** | Puntos 1 a 4 en 0 dB repartidos hasta los 4 s y el 5 justo después en −60 dB: la cola se queda clavada y desaparece de golpe |
| **Crescendo imposible** | Punto 1 en 0 dB, puntos 2 a 4 subiendo hasta +9 dB a los 3 s, punto 5 en −60 dB a los 4 s: tras cada frase la reverb crece y luego se desploma |
| **Resurrección** | Puntos 1 y 2 bajando a −40 dB en 1 s, punto 3 de vuelta a −6 dB a los 3 s, 4 y 5 cayendo: la cola casi desaparece y vuelve |
| **Reverb sin ataque** | ATT 400 ms: los golpes pierden el transitorio y solo queda el cuerpo |
| **Eco de piedra** | DENSITY 0 % · DIFFUSION 0 % · SIZE 60 m: dieciséis ecos nítidos en vez de una nube |
| **Seda** | DENSITY 100 % · DIFFUSION 90 % · SIZE 40 m |
| **Cristal** | DAMPING +80 % · SIZE 8 m · DIFFUSION 30 % |

## Cómo funciona por dentro

### Cadena de señal

```
Entrada ─┬─────────────────────────────────────────────────── seca ─┐
         └→ Pre-delay → Difusores → FDN 16 → Inversor → EQ → Puerta ─┴→ Mix → Salida
                                      ▲
                 envolvente (sustain/release) · LFO · detector de nivel
```

### Estructura del código

```
dsp/                 Motor en C++17 puro, sin JUCE. Se compila y prueba en cualquier sistema.
  DelayLine.h          Retardo circular con lectura entera e interpolada (Hermite)
  EnvelopeFollower.h   Detector de nivel
  PitchShifter.h       Cambio de tono granular
  Fdn.h/.cpp           Núcleo de la reverb
  TailEnvelope.h       Ataque y curva de la cola
  Biquad.h, Eq6.h      Ecualizador
  Lfo.h                LFO
  Reverser.h           Inversor
  Gate.h               Puerta
  ReverbEngine.h/.cpp  Encadena todo; es lo único que usa el plugin
plugin/              Envoltorio JUCE
  Parameters.*         Los 81 parámetros y su paso al motor
  PluginProcessor.*    Audio, estado, latencia, tempo
  PluginEditor.*       Ventana y distribución
  ui/                  Tema, diales, pantallas de envolvente, EQ y LFO
  assets/fonts/        Tipografías incrustadas
tests/               86 tests del DSP, sin dependencias externas
.github/workflows/   Compilación y validación automáticas
docs/                Spec de diseño y plan de implementación
```

### El núcleo: FDN de 16 líneas

Una *Feedback Delay Network* son varios retardos cuyas salidas se mezclan entre sí y vuelven a entrar. Con retardos de longitudes irregulares y una mezcla sin pérdidas, los ecos se multiplican hasta formar una cola densa.

- **16 líneas de retardo** con longitudes repartidas de forma irregular entre el 35 % y el 100 % de `Size / 343 m/s`. A 30 m, la más larga mide 87 ms; a 1000 m, 2,9 s.
- **Matriz de Hadamard** 16×16 para la mezcla: reparte cada línea por igual entre todas las demás sin perder ni ganar energía, y se calcula con 64 sumas.
- **Ganancia por línea** calculada para perder en cada vuelta los dB que marca la curva de la cola en ese instante (ver *La envolvente de la cola*).
- **Difusores**: 4 filtros pasa-todo en serie por canal antes de la red, para que un golpe seco no suene a ecos sueltos.
- **Salida estéreo**: las líneas pares van a la izquierda y las impares a la derecha, con signos alternos, lo que da dos canales decorrelados.

Lo "imposible" está en lo que pasa dentro del bucle:

- **Damping bipolar.** En negativo es un paso bajo (de 20 kHz a 500 Hz): cada rebote pierde agudos, como en una sala real. En positivo ocurre lo contrario: los graves pierden hasta 3 dB más que los agudos en cada vuelta, así que la cola se aclara con el tiempo, algo que ningún material hace. Está normalizado para que ninguna frecuencia supere la ganancia de la realimentación: el brillo nunca hace que la cola dure más de lo dibujado.
- **Cambio de tono en la realimentación.** Cuatro de las 16 líneas llevan un desplazador de tono. Como la señal da vueltas, cada pasada la desplaza otra vez: la cola sube o baja sin fin.
- **Saturador.** Con realimentación igual o mayor que 1 la energía crecería sin límite. Un limitador suave en cada línea lo impide; por debajo de cierto nivel es totalmente lineal, para que una cola sostenida no pierda nada.

### La envolvente de la cola

`TailEnvelope` vigila la entrada con un detector rápido y traduce la curva dibujada en una **velocidad de caída** (dB por segundo) que la red obedece en cada instante. La ganancia de realimentación de cada línea se calcula para perder exactamente esos dB por segundo: `g = 10^(−velocidad · T_línea / 20)`.

- **Mientras la fuente suena**, la velocidad es la pendiente media de la curva (del principio al punto 5).
- **Cuando calla**, empieza a recorrerse la curva. Cada tramo entre dos puntos es una recta en dB, es decir, una velocidad constante: `−(nivel₂ − nivel₁) / (tiempo₂ − tiempo₁)`.
  - Tramo plano: velocidad 0, realimentación exactamente 1. Además se desactiva el damping, para que una cola sostenida no se oscurezca.
  - Tramo ascendente: velocidad negativa, realimentación mayor que 1. El saturador de la red la mantiene acotada.
- **Al terminar la curva**, si el punto 5 está en silencio la cola sigue muriendo; si no, la velocidad queda en 0 y la cola se sostiene indefinidamente.
- Cada golpe nuevo reinicia la curva.

Recorrer la curva solo cuando la fuente calla es deliberado: con realimentación 1 o mayor mientras sigue entrando señal, el nivel se acumularía sin freno.

**Attack** es independiente: multiplica lo que entra a la reverb por una rampa de 0 a 1 que arranca en cada inicio de la fuente. Actúa sobre la entrada y no sobre la salida para no cortar la cola de la nota anterior.

Los tiempos fuera de orden o fuera de rango se corrigen dentro del motor, de modo que ninguna automatización puede producir una curva inválida.

### Densidad

El control tiene dos mitades con mecanismos distintos:

- **De 0 a 50 %** cambia cuánto se mezclan las 16 líneas entre sí. La matriz de mezcla se desliza desde la identidad (cada línea realimenta solo a sí misma: dieciséis ecos independientes) hasta la matriz de Hadamard (mezcla total). En todo el recorrido la matriz sigue siendo ortogonal, así que no se gana ni se pierde energía por el camino.
- **De 50 a 100 %** activa un difusor (filtro pasa-todo) dentro de cada línea. Su retardo se recorta de la propia línea, de modo que el tiempo de vuelta no cambia; lo que cambia es que cada vuelta multiplica los rebotes en vez de solo repetirlos.

Al 50 % el resultado es idéntico, muestra a muestra, al de la red sin este control.

### Cambio de tono

Granular, con dos cabezales de lectura sobre un retardo de 60 ms. Cada uno se desplaza a velocidad distinta de la de escritura (eso cambia el tono) y, cuando llega al final, salta al principio. Para tapar el salto, cada cabezal lleva una ventana que lo silencia justo en ese momento, y el otro, desfasado media vuelta, cubre el hueco.

### Ecualizador

Seis filtros bicuadráticos en serie con las fórmulas clásicas de Robert Bristow-Johnson. La curva que ves en pantalla se calcula con esas mismas fórmulas, así que dibujo y sonido coinciden.

### LFO

Un oscilador lento con cinco formas. En modo AUTO su profundidad se multiplica por `1 − nivel de entrada`: con la fuente sonando fuerte no modula, y al callar la fuente aparece el movimiento en la cola.

### Inversor

No se puede invertir el tiempo en directo, pero sí por tramos. El inversor guarda la reverb y la reproduce hacia atrás en "granos" de la longitud de la ventana. Dos granos solapados a la mitad, cada uno con una ventana suave, evitan clics en las uniones. El precio es la latencia: hay que esperar a tener el tramo para poder darle la vuelta.

### Puerta

Una máquina de estados sencilla: abre al superar el umbral, espera el tiempo de *hold* al caer por debajo y luego cierra. Tiene 3 dB de histéresis para no castañetear con señales que rondan el umbral. Con fuente DRY escucha la señal original, no la reverb: por eso la cola se corta en seco al callar la caja aunque a la reverb le quedasen segundos.

### Seguridad del hilo de audio

Durante el proceso no se reserva memoria, no se bloquea y no se hace entrada/salida: todos los búferes se crean al preparar el plugin. La interfaz recibe los niveles por una cola sin bloqueo. La salida está acotada para cualquier combinación de parámetros.

## Proceso de desarrollo

### 1. Definición

Partiendo de la petición original se tomaron estas decisiones:

| Decisión | Elegido | Descartado y por qué |
|---|---|---|
| Framework | JUCE 8 + CMake | AUv3 nativo en Swift: imposible de probar fuera de un Mac y con un proyecto Xcode frágil de mantener a mano |
| Compilación | GitHub Actions en macOS | Compilar solo en local: no habría verificación automática de que el AU compila y valida |
| Concepto | Un motor muy paramétrico | Varios "espacios" seleccionables: más rápido de usar, pero menos libre |
| Algoritmo | FDN de 16 líneas | *Plate* tipo Dattorro (suena a reverb convencional) y convolución (ni cola infinita ni tono en el bucle) |
| LFO AUTO | Sincronizado y dependiente del nivel | Modulación fija de fábrica; dibujar la forma de onda a mano |

El diseño completo está en [`docs/superpowers/specs/`](docs/superpowers/specs/2026-11-12-reverb-espacios-imposibles-design.md) y el plan de tareas en [`docs/superpowers/plans/`](docs/superpowers/plans/2026-11-12-reverb-espacios-imposibles.md).

### 2. Condicionante principal

El desarrollo se hizo en un contenedor Linux: sin macOS, sin Logic y sin altavoces. De ahí la arquitectura en dos capas. El DSP no depende de JUCE ni de Apple, así que se pudo probar entero en Linux; el envoltorio del plugin es fino y lo verifica la integración continua en un Mac real.

### 3. Construcción con tests primero y revisión independiente

Al terminar, un revisor independiente (sin el contexto de quien escribió el código) repasó la rama entera. Encontró un fallo crítico y cuatro importantes que los tests no habían cazado; se corrigieron todos, cada uno con un test nuevo que primero fallaba. Están señalados como *(Revisión)* más abajo.

Cada bloque se escribió así: primero el test, comprobar que falla, luego el código, comprobar que pasa. Orden: retardos → cambio de tono → FDN → envolvente → EQ → LFO → inversor → puerta → motor completo → plugin → interfaz → CI.

Qué miden los tests más relevantes:

- **Tiempo de caída**: se mide el RT60 real de la respuesta al impulso y se compara con el pedido (0,5, 2 y 8 s; tolerancia ±20 %).
- **La cola sigue la curva dibujada**: con una curva de cinco tramos (sostener, bajar 30 dB, sostener, subir 12 dB, caer) el nivel medido cumple cada tramo. Con señal continua, sostener no sube el nivel; con golpes repetidos, sube menos de 12 dB.
- **Densidad**: se mide la densidad de ecos de la respuesta al impulso (0 = ecos aislados, 1 = cola totalmente densa). A 0 % queda en 0,01; a 100 % la cola se llena antes que a 50 %. A cualquier valor la energía se conserva y el tiempo de caída se mantiene.
- **La cola siempre termina**: con cualquier valor de Damping, de −100 a +100 %, cae más de 60 dB.
- **Estabilidad**: 10 s de ruido a 0 dBFS con cada combinación extrema de tamaño, damping y tono; y 20 s cambiando *todos* los parámetros al azar en cada bloque. Nunca un valor no finito ni fuera de rango.
- **Attack**: la entrada de la reverb queda más de 12 dB por debajo durante el ataque y recupera el nivel después.
- **Reinicio del host**: con la cola sonando, un vaciado no produce clic y 50 ms después no queda nada del sonido anterior.
- **Silencio**: tras la cola, la salida llega a cero exacto (sin denormales que disparen la CPU).
- **Independencia del tamaño de bloque**: el mismo audio procesado en bloques de 1, 64, 480 o tamaños aleatorios da el mismo resultado.
- **Frecuencias de muestreo**: a 44,1, 96 y 192 kHz los tiempos se mantienen en segundos.
- **Host sin tempo**: con tempo 0 o negativo se usan 120 BPM.
- **EQ**: la ganancia medida con senos coincide con la configurada y con la curva dibujada.
- **Inversión**: un golpe que decae sale como un golpe que crece.
- **Gated**: con la puerta, la reverb queda 60 dB por debajo a los 500 ms; sin ella, sigue sonando.

### 4. Problemas encontrados y cómo se resolvieron

- **El primer test del inversor estaba mal planteado.** Pedía que una rampa saliese invertida, pero con dos granos solapados eso no se cumple ni en un inversor correcto. Se sustituyó por uno que mide dónde queda la energía respecto al pico.
- **El inversor sonaba 1,6 dB bajo.** Dos granos que leen momentos distintos no suman en amplitud sino en potencia. Se compensó con una ganancia fija.
- **El freeze dejaba pasar algo de entrada** durante su fundido. Se acortó el fundido.
- **La puerta cerraba tarde** porque compartía detector con la envolvente (lento). Ahora tiene uno propio, más rápido.
- **Una cola sostenida seguía oscureciéndose** por el damping. Ahora el sustain y el freeze lo desactivan.
- **La interpolación de los retardos restaba agudos** en cada vuelta, lo que impedía una cola realmente infinita. Ahora los retardos son de longitud entera cuando el tamaño está quieto y solo se interpolan mientras se mueve.
- **(Revisión) El damping positivo hacía autooscilar la red**: sumaba agudos por encima de la ganancia de la realimentación y la cola no terminaba nunca. Se normalizó para que ninguna frecuencia supere la ganancia de la realimentación.
- **(Revisión) El sustain acumulaba demasiado nivel** con señal continua. Ahora solo sostiene cuando la fuente calla.
- **(Revisión) Reactivar REVERSE soltaba audio antiguo** que se había quedado en su búfer. Ahora el inversor se alimenta siempre.
- **(Revisión) La latencia se comunicaba al host desde el hilo de audio.** Ahora se hace desde el hilo de mensajes y solo cuando el valor se estabiliza.
- **(Revisión) El EQ cambiaba de golpe.** Ahora cada cambio se desliza en 20 ms.
- **(Primera escucha) Clics al arrancar la reproducción y en modo ciclo.** En reproducción continua el motor no produce discontinuidades (medido con señales suaves y con ruido). El problema estaba en el reinicio que pide el host al arrancar o al saltar el ciclo: el plugin cortaba la cola en seco y además borraba unos 17 MB de retardos en el hilo de audio, lo bastante lento como para provocar un corte con búferes pequeños. Ahora el reinicio es instantáneo (los retardos no se borran: lo antiguo simplemente deja de poder leerse) y la cola se funde en 5 ms en vez de cortarse. *Pendiente de confirmar por escucha que los clics han desaparecido.*
- **Una pista falsa.** Al investigar los clics pareció que cambiar SIZE de golpe producía un estallido de nivel. Era un artefacto de medir con un tono puro (las resonancias de la sala se desplazan); con ruido no hay ni estallido ni clic. Se descartó el arreglo que se había empezado a escribir.
- **El primer run de CI se canceló** al instante: la etiqueta `macos-14` ya no tenía runner. Se cambió a `macos-latest`.

### 5. Iteración tras la primera escucha

Con el plugin ya instalado y validado en Logic, el autor pidió dos cambios, además de señalar los clics:

- **Poder apagar el LFO**: se añadió el interruptor ON/OFF.
- **Una envolvente más moldeable**: en una primera versión se añadieron tres puntos (ATTACK, BENDER y CURVE) sobre el esquema clásico de sustain y release.

En la segunda escucha el autor confirmó que los clics habían desaparecido y pidió dos cosas más:

- **Puntos realmente libres.** Los de la primera versión dependían unos de otros (mover uno desplazaba a los siguientes) y cada uno solo se movía en una dirección. Se sustituyó todo el esquema de sustain y release por una curva de cinco puntos libres en tiempo y nivel. La objeción inicial a una curva libre era la automatización; se resolvió fijando el número de puntos en cinco: así cada punto son dos parámetros normales, que Logic graba en modo Touch. Contrapartida asumida: los proyectos guardados con la versión anterior pierden sus ajustes de sustain y release.
- **Control de densidad**, tanto para hacer la cola más densa como para dejarla en ecos sueltos.

### 6. Interfaz

Se presentó una maqueta antes de programar nada, con las condiciones dadas: negro y verde, estética futurista, el nombre arriba a la izquierda y los créditos discretos arriba a la derecha. Aprobada la maqueta, se implementó con primitivas de dibujo de JUCE, sin imágenes. El fondo de la pantalla principal es un corredor en perspectiva cuyo punto de fuga se desplaza con Size y el LFO; el símbolo es un triángulo de Penrose.

## Compilar y probar

### Tests del DSP (Linux o macOS, sin JUCE)

```bash
cmake -B build -DEI_BUILD_PLUGIN=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

### Plugin en macOS

Requisitos: Xcode y CMake 3.22 o superior (`brew install cmake`). JUCE se descarga solo.

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release
```

Al terminar, el `.component` se copia automáticamente a `~/Library/Audio/Plug-Ins/Components/`. También se genera una aplicación autónoma (`Impossible Spaces.app`) para probar sin Logic.

### Integración continua

El workflow [`build.yml`](.github/workflows/build.yml) se ejecuta en cada push:

1. **dsp-tests** (Ubuntu): compila y pasa los 86 tests.
2. **macos-au** (macOS): compila el AU universal (Apple Silicon + Intel), repite los tests, comprueba las arquitecturas, firma ad-hoc, instala, ejecuta `auval -v aufx Imsp Vita` y publica `ImpossibleSpaces-AU.zip` y `ImpossibleSpaces-Standalone.zip`.

### Identidad del plugin

| | |
|---|---|
| Nombre | Impossible Spaces |
| Fabricante | VitaPlug (`Vita`) |
| Código | `Imsp` |
| Tipo | `aufx` (efecto) |
| Bundle ID | `com.vitaplug.impossiblespaces` |
| Entrada / salida | Estéreo → estéreo, o mono → estéreo |

## Validación en Logic: checklist

- [ ] `auval -v aufx Imsp Vita` termina con `AU VALIDATION SUCCEEDED`
- [ ] Aparece en *Audio Units → VitaPlug → Impossible Spaces* y carga en una pista estéreo
- [ ] Carga también en una pista mono (como mono → estéreo)
- [ ] Suena con los valores por defecto
- [ ] La pantalla muestra la entrada y la reverb moviéndose
- [ ] Arrastrar PRE y ATT cambia la entrada de la reverb
- [ ] Los cinco puntos se mueven libremente y el sonido sigue la curva (sostener, caer, crecer)
- [ ] Con el punto 5 por encima de −60 dB la cola no muere
- [ ] Automatizar un punto en modo Touch graba y reproduce su movimiento
- [ ] DENSITY: a 0 % se oyen ecos sueltos, a 100 % la cola es más densa que a 50 %
- [ ] El EQ se oye y la curva corresponde
- [ ] LFO en AUTO y en MANUAL, con los cuatro destinos, y apagado con ON/OFF
- [ ] REVERSE: suena invertido y la señal seca sigue a tempo
- [ ] GATE con fuente DRY sobre una caja: la cola se corta
- [ ] FREEZE mantiene el sonido
- [ ] Automatizar un parámetro funciona en reproducción
- [ ] Guardar, cerrar y reabrir el proyecto conserva los ajustes
- [ ] Un *bounce* offline sale igual que en tiempo real
- [ ] El consumo de CPU es razonable con varias instancias

## Limitaciones conocidas

- **Al arrancar la reproducción o saltar el ciclo, Logic vacía la cola de la reverb** (se funde en 5 ms). Es el comportamiento estándar de un efecto, pero significa que la cola no continúa de una vuelta del ciclo a la siguiente.
- **El cambio de tono es granular**: con material muy tonal y sostenido puede oírse un ligero trémolo a 60 ms.
- **DENSITY alto con SIZE muy pequeño** puede dar un timbre metálico: es propio de los difusores dentro del bucle. Pendiente de valorar por escucha.
- **Por encima del 50 % de DENSITY, el efecto es sutil si DIFFUSION ya está alto**, porque la cola ya nace densa; se nota más en los primeros 100 ms.
- **Solo 4 de las 16 líneas** llevan cambio de tono, por coste de CPU.
- **Cambiar WINDOW con el inversor en marcha**: la nueva longitud se aplica al terminar el tramo en curso, y en ese momento puede oírse un clic en la señal seca.
- **Activar o desactivar REVERSE cambia la latencia** del plugin; Logic puede tardar un instante en recompensar.
- **Con sustain y golpes muy seguidos** (más juntos que el tiempo de Sustain) la reverb apenas decae entre golpe y golpe y el nivel sube unos dB respecto a un golpe aislado.
- **El aviso de latencia a Logic** llega con hasta 0,2 s de retraso al activar REVERSE o cambiar la ventana.
- **En bypass con REVERSE activo** la señal no se retrasa lo que el plugin declara de latencia.
- **Memoria**: unos 17 MB por instancia a 48 kHz y unos 67 MB a 192 kHz, por los retardos de hasta 3 s.
- **CPU**: en las pruebas, alrededor del 3 % de un núcleo con todo activado (medido en Linux, no en un Mac).
- **Sin presets de fábrica**, sin VST3/AAX, sin entrada de sidechain externa y con ventana de tamaño fijo (1000×640).
- **Sin firmar ni notarizar**: hace falta el paso de la cuarentena.

## Licencias

- **JUCE 8** se usa bajo su licencia AGPLv3 (o la licencia Starter gratuita de JUCE). Para uso personal no hay problema. Si algún día distribuyes el plugin, tendrás que publicar el código bajo AGPLv3 o adquirir la licencia de JUCE que corresponda. **Este repositorio aún no tiene un archivo de licencia propio**: es una decisión pendiente del autor.
- **Tipografías** incrustadas, todas bajo SIL Open Font License 1.1 (textos en `plugin/assets/fonts/`): Michroma, Chakra Petch y JetBrains Mono.
