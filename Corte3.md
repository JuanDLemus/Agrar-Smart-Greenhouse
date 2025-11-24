# **Diseño e implementación de un sistema inteligente de invernadero para temperatura y humedad basado en ESP32**

Juan Diego Lemus Rey
Universidad de La Sabana, Colombia

---

## Resumen

La producción de flores en invernadero es extremadamente sensible al microclima; una gestión deficiente de temperatura y humedad relativa favorece la aparición de enfermedades fúngicas y bacterianas, reduce la calidad del producto e incrementa el costo energético por ventilación ineficiente. Este trabajo presenta el diseño e implementación de un nodo de monitoreo y control de bajo costo pero orientación industrial, basado en un ESP32 integrado en una tarjeta KC868-A6, con cuatro canales de sensado: temperatura y humedad (SHT45), corriente de bus DC (ACS758LCB-050), tensión de línea AC (ZMPT101B con red resistiva de adaptación) y potencia eléctrica derivada. El nodo acciona hasta seis relevadores utilizados para gobernar ventiladores y otras cargas en un invernadero pequeño.

La contribución es deliberadamente instrumental: se detalla el frente analógico, la adaptación de señal al dominio de 0–3,3 V del ADC, las estrategias de reducción de ruido y un marco de co-simulación en MATLAB/Simulink que replica el cableado físico y el firmware del ESP32, incluyendo lazos PID y filtrado digital. La arquitectura resultante es segura, replicable y lo suficientemente económica para su uso en instalaciones agrícolas pequeñas y medianas, a la vez que es extensible hacia un nodo AIoT industrial con tableros remotos seguros y analítica predictiva.

**Palabras clave** — ESP32, control industrial, invernadero, control PID, IoT, IIoT, AIoT, ACS758, ZMPT101B, SHT45, medición de potencia, instrumentación, MATLAB/Simulink.

---

## I. Introducción

### A. Motivación

Los invernaderos permiten cierto control sobre radiación solar, viento y precipitación, pero también crean un ambiente cerrado y húmedo donde los patógenos proliferan si la renovación de aire no se gestiona correctamente. En la práctica, muchos productores pequeños operan con unos pocos ventiladores conmutados manualmente y sin información objetiva sobre temperatura, humedad y consumo eléctrico. Esto conduce a fallos típicos: humedad persistentemente alta, aire estancado, condensación sobre superficies foliares, estrés térmico en horas de máxima irradiancia y fallas no detectadas de ventiladores o circuitos de potencia.

Un controlador compacto capaz de medir temperatura, humedad, tensión de línea, corriente de ventiladores y energía consumida, y de aplicar estrategias de ventilación basadas en PID, puede reducir significativamente estos riesgos y proporcionar datos cuantitativos para justificar un mayor nivel de automatización. Existen controladores comerciales, pero suelen ser costosos, cerrados y rara vez exponen datos crudos o métricas de potencia de alta resolución al operador.

### B. Planteamiento del problema

El caso de uso objetivo es un invernadero de flores alimentado desde una red de distribución de 220 V, con varios ventiladores accionados desde un bus DC de 12 V y cargas adicionales potenciales a 110/220 V AC (calefactores, bombas). Los requisitos principales son:

* Monitoreo continuo de temperatura y humedad con resolución sub-grado y sub-porcentaje.
* Medición de corriente DC de ventiladores hasta 50 A y estimación de tensión de línea AC para calcular tensión RMS, corriente RMS y potencia consumida.
* Aislamiento galvánico seguro entre red y lógica de control.
* Estrategia de control basada en reguladores PI/PID que pueda sintonizarse y validarse en simulación antes del despliegue.
* Implementación hardware compacta, reproducible y programable desde el entorno Arduino.
* Ruta clara de migración hacia un nodo IoT/IIoT/AIoT con tableros locales y remotos y elementos básicos de ciberseguridad.

### C. Contribuciones

Las contribuciones principales de este trabajo son:

1. Una cadena de instrumentación completa alrededor de un controlador KC868-A6 con ESP32, integrando SHT45, ACS758LCB-050 y ZMPT101B, con atención explícita a seguridad eléctrica, escalamiento de señal y compatibilidad con el ADC interno.
2. Una red resistiva tipo medio puente (3×330 Ω y 2×220 Ω) para adaptar un módulo ZMPT101B alimentado a 12 V al rango 0–5 V de las entradas analógicas de la KC868-A6, manteniendo la impedancia efectiva y el ruido dentro de límites aceptables.
3. Una estrategia de adquisición de datos que combina sobre-muestreo (≈1000 muestras por ventana), cálculo de RMS, promedios móviles y filtrado digital simple para obtener métricas de potencia estables a ≈0,5 Hz de actualización.
4. Un modelo en MATLAB/Simulink que reproduce el suministro eléctrico, los frentes de sensado, la dinámica ambiental y el firmware del ESP32 (incluyendo controladores PID, cuantización de ADC y compuertas lógicas), permitiendo una validación tipo hardware-in-the-loop antes de la puesta en servicio.
5. Un plan arquitectónico para evolucionar el prototipo hacia un dispositivo AIoT industrial con tableros seguros, soporte de protocolos (HTTP/MQTT/Modbus) y posibilidad de incorporar detección de anomalías basada en aprendizaje automático.

### D. Organización del documento

La Sección II describe la arquitectura global y los requisitos. La Sección III detalla el diseño de hardware e instrumentación, con énfasis en sensores, módulos, niveles de tensión y conexiones. La Sección IV discute el acondicionamiento de señal, el muestreo y la mitigación de ruido. La Sección V presenta el control embebido y la arquitectura de software. La Sección VI se centra en el modelado en MATLAB/Simulink. La Sección VII resume resultados experimentales preliminares. La Sección VIII discute las limitaciones. La Sección IX plantea la evolución hacia un nodo AIoT y la Sección X presenta las conclusiones. Una tabla-resumen en la Sección XI sintetiza componentes y roles. Finalmente, se propone una guía de README con material gráfico sugerido.

---

## II. Arquitectura del sistema y requisitos

### A. Caso de uso y condiciones de operación

El controlador está diseñado para una instalación rural típica alimentada a 220 V, con un bus DC de 12 V para varios ventiladores axiales. Las condiciones nominales son:

* **Temperatura:** rango de operación 15–35 °C; objetivo de control entre 18–28 °C.
* **Humedad relativa:** 40–95 %RH, evitando exposiciones prolongadas por encima de 85 %RH.
* **Corriente de ventiladores:** hasta 50 A continuos en el bus de 12 V.
* **Tensión de red:** 110/220 V AC con caídas y sobretensiones ocasionales.

Estas condiciones se usan tanto para especificar sensores como para dimensionar conductores, fusibles y disipación térmica de los módulos.

### B. Visión general del sistema

El sistema físico se resume en dos lazos acoplados:

1. **Lazo ambiental**
   Ambiente → SHT45 → ESP32 (ADC/I²C) → PID(T,RH) → relevadores KC868-A6 → ventiladores → ambiente.

2. **Lazo de monitoreo de potencia**
   Red y bus DC de 12 V → ZMPT101B + ACS758LCB-050 + acondicionamiento analógico → entradas A1–A4 de KC868-A6 → ADC del ESP32 → cálculo de Vrms, Irms, potencia activa y energía → tablero local y (futuro) pasarela IIoT.

La KC868-A6 integra un módulo ESP32, entrada de 12 V, seis relevadores, cuatro entradas analógicas nominalmente tolerantes a 5 V, varias entradas digitales optoacopladas, un módulo LoRa y un conector I²C. Esto permite un cableado muy compacto: el SHT45 se conecta directamente al bus I²C de 3,3 V, la salida del ACS758 y la señal adaptada del ZMPT101B alimentan las entradas A1 y A2, y los relevadores accionan los ventiladores.

Desde el punto de vista electrónico, la placa proporciona: terminales de tornillo para entradas/salidas de potencia, regulación interna de 12 V a 5 V y 3,3 V, drivers de relevador con transistores y diodos de rueda libre, y una etapa analógica simple que escala 0–5 V a 0–3,3 V para el ADC del ESP32. Estos detalles reducen significativamente el esfuerzo de diseño de PCB y permiten concentrarse en la instrumentación de sensores externos y en la lógica de control.

### C. Requisitos de diseño

Los requisitos de instrumentación son:

* **Margen del ADC:** todas las señales analógicas deben permanecer estrictamente entre 0–5 V en la interfaz de la KC868 y entre 0–3,3 V en el ADC interno del ESP32 tras el escalado.
* **Resolución:** resolución efectiva mejor que 1 V en tensión de red, mejor que 0,1 A en corriente DC, y ≈0,1 °C y 0,5 %RH en variables ambientales.
* **Muestreo:** frecuencias de muestreo suficientemente altas para resolver la red de 50/60 Hz y armónicos de orden bajo (≥2 kHz en canales de tensión y corriente).
* **Aislamiento y seguridad:** aislamiento galvánico entre red y lógica mediante sensores Hall y transformadores; fusibles, varistores y correcta disposición de pistas en el lado primario.
* **Reproducibilidad:** uso de módulos comerciales estándar (SHT45, ZMPT101B, ACS758) y componentes discretos fáciles de conseguir; constantes de escalamiento derivadas analíticamente o calibradas con instrumentos externos.

---

## III. Diseño de hardware e instrumentación

### A. Plataforma de control: tarjeta KC868-A6 con ESP32

La KC868-A6 integra un ESP32, seis relevadores de 10 A (contactos NO/COM/NC), cuatro entradas analógicas, entradas digitales optoacopladas, radio LoRa, salidas DAC e interfaz I²C en un módulo montable en riel DIN. Su entrada de 12 V alimenta los relevadores y convertidores DC/DC internos para el ESP32 y la lógica auxiliar.

En este proyecto:

* **Relevadores:** hasta tres relevadores se asignan al control escalonado de ventiladores, uno a cargas auxiliares potenciales (por ejemplo calefactor o bomba) y dos quedan de reserva. Cada relevador dispone de LED indicador en la tarjeta, lo que facilita las pruebas sin conectar cargas reales.
* **Entradas analógicas:** A1 recibe la salida del ACS758LCB-050; A2 recibe la señal del ZMPT101B tras la red divisora; A3 y A4 se reservan para futuras mediciones (tensión directa de 12 V, sensores de presión, etc.).
* **I²C:** la hilera de pines originalmente pensada para una pantalla integrada se reutiliza como bus I²C para la SHT45. Se emplean resistencias de pull-up típicas de 4,7 kΩ hacia 3,3 V.

Desde el punto de vista electrónico, es relevante destacar:

* La existencia de filtros RC básicos en las entradas analógicas de la KC868 que actúan como antialiasing inicial.
* El uso de optoacopladores en las entradas digitales, que permite conectar contactos secos o señales de campo sin comprometer el dominio del ESP32.
* La disposición de planos de masa y separación física entre pistas de potencia y señales de baja amplitud, esencial para medir tensiones en el rango de milivoltios provenientes de los sensores de potencia.

Esta plataforma reduce el esfuerzo de diseño de electrónica de potencia y deja al diseñador centrarse en la elección, conexión y calibración de módulos de sensado externos.

### B. Sensado de temperatura y humedad: SHT45

El SHT45 es un sensor digital de humedad y temperatura con calibración integrada y una interfaz I²C estándar. Opera entre 1,08 y 3,6 V y entrega mediciones de 16 bits:

* Temperatura en el rango −40 a 125 °C con exactitud típica de ±0,1 °C en el rango de interés del invernadero.
* Humedad relativa 0–100 % con exactitud de ±1 %RH en la banda central.

Se alimenta desde el riel de 3,3 V de la KC868 y se conecta a las líneas SDA/SCL del ESP32 mediante trazas cortas y resistencias de pull-up. Un condensador de desacoplo (≈100 nF) se coloca lo más cerca posible del encapsulado del sensor para filtrar ruido de alta frecuencia.

En cuanto a montaje físico, el SHT45 debe ubicarse en una región representativa del volumen de aire del invernadero, evitando zonas donde el flujo directo del ventilador enfríe artificialmente el sensor. Es recomendable que el módulo esté protegido por una carcasa permeable al aire (por ejemplo, una capucha con membrana PTFE) que lo aísle de gotas, polvo y salpicaduras.

Aunque el sensor viene calibrado de fábrica, se contempla una calibración de campo básica comparando sus lecturas con un termo-higrómetro de referencia en varias condiciones estacionarias; cualquier desviación sistemática se corrige mediante offsets en firmware.

### C. Medición de corriente: ACS758LCB-050

El ACS758LCB-050 es un sensor de corriente por efecto Hall con conductor primario integrado de baja resistencia (≈100 µΩ) y aislamiento galvánico entre primario y secundario. Opera entre 3 y 5,5 V y produce una salida analógica ratiométrica centrada en VCC/2:

* Con VCC = 3,3 V, la salida en ausencia de corriente se sitúa alrededor de 1,65 V.
* La sensibilidad es del orden de decenas de mV/A (dependiente del sufijo de modelo), lo que implica deltas de tensión razonables incluso para corrientes moderadas.
* El ancho de banda es de hasta ≈120 kHz, muy superior a la dinámica de interés, por lo que se requiere filtrado analógico.

En la implementación propuesta, el ACS758 se alimenta a 3,3 V para mantener todo el dominio de medida dentro del rango del ADC sin necesidad de traslación de nivel. Su salida se conduce a la entrada analógica A1 mediante una pista corta, con un pequeño resistor serie y un condensador CF entre salida y masa según recomendaciones del fabricante, formando un filtro pasa-bajo de primer orden que recorta ruido de alta frecuencia.

El bus DC de 12 V que alimenta a los ventiladores se cablea físicamente a través de los terminales IP+ e IP− del ACS758, de forma que se mide la corriente total de ventilación. Los terminales de tornillo del módulo se seleccionan con capacidad para la corriente máxima prevista y sección suficiente de conductor, manteniendo distancias de fuga adecuadas respecto a la electrónica de baja señal.

### D. Medición de tensión AC: ZMPT101B con divisor personalizado

El ZMPT101B es un transformador de tensión compacto diseñado para medición de red monofásica. Los módulos comerciales integran el transformador, una resistencia de carga y un amplificador operacional (habitualmente LM358) configurado para amplificar y centrar la señal secundaria alrededor de VCC/2.

Para aprovechar el margen dinámico del amplificador, el módulo se alimenta con los 12 V disponibles en la KC868. En estas condiciones, la salida puede variar entre 0 y casi 12 V, mientras que las entradas analógicas de la tarjeta solo admiten 0–5 V. Para resolver esta incompatibilidad se implementa una red resistiva con tres resistores de 330 Ω y dos de 220 Ω en configuración de medio puente tipo Wheatstone:

* El nodo de salida del módulo (“OUT”) alimenta la parte superior de la red.
* La combinación resistiva produce un nodo intermedio “Vmeas” cuyo valor máximo no excede 5 V incluso si OUT alcanza 12 V.
* La impedancia equivalente vista por el módulo permanece en el rango de kilo-ohmios bajos, asegurando que el LM358 no se sobrecargue, mientras que la fuente para el ADC presenta una impedancia suficientemente baja para evitar errores de muestreo.

El factor de división teórico se calcula en función de los valores de resistor y se ajusta experimentalmente comparando VRMS con un multímetro de referencia para varias tensiones de línea conocidas. Esta misma red se replica en el modelo de Simulink para que la transferencia tensión-ADC sea coherente entre simulación y hardware.

Desde el punto de vista de hardware, se cuidan las distancias de aislamiento entre el lado de red (primario del ZMPT, bornes de entrada) y la parte de baja tensión, siguiendo buenas prácticas de creepage y clearance. El potenciómetro integrado en el módulo se ajusta durante la etapa de calibración para centrar la señal y ajustar la ganancia global al rango deseado.

### E. Seguridad y aislamiento

Aunque el prototipo está orientado a pruebas de laboratorio, se contemplan criterios básicos de diseño industrial:

* Fusibles y varistores (MOV) protegen las conexiones primarias de AC frente a cortocircuitos y sobretensiones.
* El ACS758 y el ZMPT101B proporcionan aislamiento galvánico entre la red y el dominio del ESP32.
* Los contactos de relevador proporcionan separación física entre lógica y circuitos conmutados.
* Se prevé una caja plástica para montaje en riel DIN, con compartimentos separados para potencia y señal, prensaestopas adecuados y rotulación clara de terminales.

Cualquier instalación en sistemas reales de 110/220 V debe cumplir la normativa eléctrica local y ser ejecutada por personal calificado; el enfoque de este trabajo se centra en la parte de baja tensión e instrumentación.

### F. Resumen de módulos y parámetros eléctricos clave

A nivel de hardware, el sistema se apoya en:

* **Microcontrolador:** ESP32 (doble núcleo, Wi-Fi, Bluetooth, ADC de 12 bits).
* **Módulo de control:** KC868-A6, con alimentación de 12 V DC, 6 relevadores de 10 A, 4 entradas analógicas 0–5 V, opto-entradas digitales, radio LoRa y conector I²C.
* **Sensor ambiental:** SHT45, rango −40–125 °C, 0–100 %RH, I²C, alimentación 1,08–3,6 V.
* **Sensor de corriente:** ACS758LCB-050, rango ±50 A, salida ratiométrica centrada en VCC/2, aislamiento galvánico.
* **Sensor de tensión:** ZMPT101B + LM358, VCC 5–30 V (12 V en este diseño), salida analógica centrada en VCC/2.
* **Front-end de adaptación:** red 3×330 Ω + 2×220 Ω, divisores adicionales y filtros RC.

Este inventario resume la base electrónica sobre la que se construyen el acondicionamiento de señal y el modelado posterior.

---

## IV. Acondicionamiento de señal, muestreo y mitigación de ruido

### A. Mapeo de ADC y canales

La KC868-A6 enruta cada entrada analógica hacia un canal del ADC del ESP32 a través de una etapa de acondicionamiento sencilla 0–5 V. En este diseño:

* A1 → salida del ACS758 (centrada en ≈1,65 V).
* A2 → salida del ZMPT101B tras la red divisora.
* A3/A4 → libres para futuras expansiones.

El ADC del ESP32 opera nominalmente entre 0 y 3,3 V con resolución de 12 bits. Curvas de calibración en firmware compensan parte de su no linealidad inherente.

### B. Estimación de RMS de tensión y corriente

El firmware toma ≈1000 muestras crudas por canal en un bucle rápido, registra valores mínimo y máximo y calcula una amplitud pico a pico aproximada:

[
V_{\text{pp}} = V_{\max} - V_{\min}
]

Asumiendo una onda casi senoidal centrada en la mitad de escala, el valor RMS se aproxima como:

[
V_{\text{RMS}} \approx \frac{V_{\text{pp}}}{2\sqrt{2}} \cdot K_V
]

donde (K_V) es una constante de calibración empírica. Un cálculo similar se realiza para corriente con constante (K_I). Se emplea sobre-muestreo: 20 ventanas consecutivas de RMS se promedian, entregando una medición consolidada cada ≈2 s, lo que atenúa significativamente ruido de alta frecuencia y parpadeo visual en las lecturas.

En una rama posterior del firmware se implementa un RMS discreto pleno:

[
V_{\text{RMS}} = \sqrt{\frac{1}{N}\sum_{k=1}^{N}(V_k - V_{\text{offset}})^2}
]

menos sensible a distorsiones de forma de onda. Ambas formulaciones se reproducen en Simulink para validación cruzada.

### C. Filtrado digital y fuentes de ruido

Las fuentes de ruido incluyen conmutación de relevadores, conmutación de inductancias de ventiladores, rizado de convertidores DC/DC, ruido Hall intrínseco del ACS758 y ruido del amplificador operacional del módulo ZMPT101B. Las estrategias de mitigación son:

* Filtros RC hardware en las salidas del ACS758 y del ZMPT para limitar el ancho de banda a unos pocos cientos de Hz.
* Separación física entre planos de masa de potencia y de señal, minimizando lazo de área en trazas de alta corriente.
* Filtros de media móvil y filtros IIR exponenciales en firmware para temperatura, humedad, tensión y corriente.
* Ventanas de cálculo RMS alineadas, en el modelo de Simulink, con múltiplos enteros del periodo de red para estudiar el impacto de muestreo no síncrono.

### D. Cadena de sensado ambiental

La lectura del SHT45, aunque digital, está afectada por jitter y latencia de conversión. El ESP32 aplica:

* Un filtro IIR del tipo (y[n]=\alpha x[n]+(1-\alpha)y[n-1]) para temperatura y humedad.
* Rechazo de valores atípicos cuando se detectan saltos mayores a cambios físicamente plausibles por segundo.
* Limitación de la frecuencia de actualización del control (0,5–1 Hz) para evitar conmutaciones excesivas de relevadores.

---

## V. Control y arquitectura de software embebido

### A. Partición de tareas

El firmware del ESP32 se organiza en tareas lógicas (como tareas FreeRTOS o lazos cooperativos):

1. **Tarea de sensado:** interroga ADCs y SHT45, realiza el cálculo de RMS y filtrado y actualiza una estructura global de métricas.
2. **Tarea de control:** ejecuta los algoritmos PID para temperatura y humedad, aplica lógica de seguridad y decide el estado de los relevadores (escalones de ventilación).
3. **Tarea de comunicación:** proporciona una interfaz serie para depuración y, en revisiones futuras, puntos finales HTTP/MQTT para tableros.
4. **Tarea de diagnóstico:** supervisa rangos plausibles de sensores, detecta lecturas ausentes o saturadas y genera alarmas.

### B. Control PID de temperatura y humedad

Se implementan dos controladores PI/PID, uno para temperatura y otro para humedad. La variable manipulada es el nivel de ventilación efectivo, realizado como un número discreto de ventiladores encendidos. El resultado continuo del PID se mapea mediante una función de etapificación:

* Salidas en [0,1] → 0 o 1 ventilador.
* [1,2] → 2 ventiladores, etc.

Se implementa anti-windup limitando el término integral cuando todos los ventiladores están completamente apagados o encendidos. Los puntos de consigna se configuran manualmente y, en versiones posteriores, desde el tablero web.

### C. Lógica consciente de potencia

Gracias a la medición continua de tensión y corriente, el firmware puede aplicar estrategias conscientes de potencia:

* Detección de ventiladores bloqueados o fallidos: presencia de comando de relevador sin cambio de corriente apreciable.
* Detección de picos de corriente anómalos indicativos de cortocircuitos o fallas mecánicas.
* Acumulación de energía (Wh) por nivel de ventilación, permitiendo análisis posteriores de costo energético versus desempeño microclimático.

### D. Tableros locales y remotos

El firmware incluye el concepto de servidor HTTP embebido que sirve:

* Un tablero HTML/JavaScript estático (almacenado en flash) con gráficas en tiempo real de T, RH, tensión, corriente, potencia y estado de relevadores.
* Un punto final JSON (por ejemplo `/api/sensors`) que entrega un “snapshot” de las variables medidas (`Voltage`, `Current`, `Power`, `Energy`, `Temperature`, `Humidity`, etc.).

El mismo esquema de datos se utilizará para publicar información vía MQTT a un broker IIoT o a una plataforma de supervisión industrial.

---

## VI. Modelado y co-simulación en MATLAB/Simulink

MATLAB/Simulink se utiliza como banco de pruebas de alta fidelidad para co-diseñar la instrumentación y la lógica de control antes y durante la implementación física. Los modelos imitan el cableado real con componentes básicos y modelos explícitos de sensores, buscando replicar el comportamiento de hardware con el mínimo de abstracciones.

### A. Modelo de suministro eléctrico y etapa de potencia

En el extremo izquierdo del diagrama de alto nivel se encuentra una **fuente trifásica de 220 V** que representa la alimentación principal. Un bloque lógico “On/Off” modela el interruptor general. A partir de esta fuente se derivan:

* La línea monofásica que alimenta el lazo de medida de tensión con ZMPT101B.
* Un riel DC de 12 V que representa el suministro de ventiladores y la alimentación de 12 V del ZMPT.
* Una referencia de 3,3 V correspondiente a la lógica del ESP32/KC868.

Los ventiladores se representan como cargas de Simscape controladas por interruptores lógicos cuyos comandos son las salidas de relevador simuladas. Lámparas de Simulink indican el estado ON/OFF de cada ventilador, cerrando el ciclo visual entre lógica digital y flujo de potencia.

### B. Subsistemas de emulación de sensores

Cada sensor físico tiene un subsistema correspondiente:

1. **Subsistema de corriente**
   La corriente del bus DC se mide con un sensor ideal cuya salida entra a un bloque “Sensor Voltage + Current”. Dentro se modela el comportamiento del ACS758: ganancia (mV/A), offset DC en VCC/2, fuentes de ruido y el filtro RC. La salida es la señal de tensión equivalente al pin conectado a A1. A continuación, un bloque “C Instrumental Normalizer” aplica el escalado y normalización, incluyendo cuantización del ADC.

2. **Subsistema de tensión**
   La red AC pasa por un modelo de transformador de tensión que representa el ZMPT101B. El secundario alimenta un bloque de amplificador operacional y luego la red de resistores 3×330/2×220, replicando el medio puente físico. El nodo resultante alimenta el bloque “V Instrumental Normalizer”, que normaliza y cuantiza la señal.

3. **Subsistema SHT45**
   Perfiles de temperatura y humedad ambiente se generan con fuentes que simulan ciclos día-noche y perturbaciones. Estos perfiles ingresan a un bloque SHT45 (System object o subsistema enmascarado) que replica la interfaz I²C: entrega un marco digital o un bus estructurado. Un adaptador “i2c_to_digital” separa temperatura y humedad y, cuando se requiere, las convierte en tensiones equivalentes al dominio de 3,3 V.

### C. Emulación de lógica ESP32

En el lado derecho del diagrama principal, un bloque grande representa el firmware del ESP32 mediante un bloque MATLAB Function o grupos de subsistemas discretos. Sus entradas son las cuatro señales normalizadas (corriente, tensión, temperatura, humedad); sus salidas son:

* Mediciones cuantizadas y filtradas (`curr_in`, `volt_in`, `temp_in`, `hum_in`) para graficación.
* Señales internas PID (`C_PID`, `V_PID`, `H_PID`, `T_PID`).
* Pines lógicos (`pinTemp`, `pinHum`, `pinVolt`, `pinCurr`) que emulan los comandos a relevadores.

Dentro de cada subsistema PID se incluye:

* Un bloque de ganancia para escalar el error.
* Un bloque PID(s) continuo o discreto con anti-windup.
* Un modelo de actuador de primer orden que aproxima la dinámica térmica o de humedad frente al comando de ventilación.
* Un cuantizador y función escalonada que convierten la salida continua del PID en número entero de ventiladores actuados.

Compuertas lógicas combinan condiciones (por ejemplo, “temperatura alta OR humedad alta”) para generar el comando final hacia los ventiladores.

### D. Dinámica ambiental y modelado de ruido

El ambiente se representa como un volumen cerrado con:

* Capacitancia térmica y resistencias térmicas que modelan el intercambio de calor con el exterior y ganancias solares.
* Un balance de masa de vapor que considera fuentes de humedad (transpiración, evaporación de suelo) y sumideros (ventilación).

Se modelan ciclos día-noche mediante fuentes periódicas, y se inyecta ruido en varias etapas:

* Ruido blanco aditivo en señales de tensión y corriente para emular interferencia eléctrica.
* Ruido tipo 1/f en lecturas SHT45 para simular deriva de sensor.
* Perturbaciones tipo escalón que representan apertura de puertas o frentes fríos.

Estos elementos permiten “estresar” las estrategias de filtrado y los PID antes de implementarlos en hardware.

### E. Estrategia de validación

El marco de modelado admite varios modos de validación:

1. **Simulación pura:** ejecución del modelo con sensores ideales para sintonizar el PID y verificar estabilidad en lazo cerrado.
2. **Verificación de instrumentación:** activación de modelos detallados de sensores y ruido para observar cómo se comportan los algoritmos RMS y los filtros; comparación de RMS de Simulink con valores analíticos.
3. **Lógica equivalente a firmware:** el bloque MATLAB Function replica la aritmética entera, intervalos de muestreo y condicionales del código Arduino.
4. **Comparación de trazas:** registro de series de tiempo de T, RH, Vrms, Irms y potencia tanto de Simulink como del prototipo físico bajo escenarios similares, y comparación para cuantificar error de modelado.

---

## VII. Prototipo experimental y resultados preliminares

### A. Montaje de laboratorio

El prototipo de banco utiliza una KC868-A6 montada en riel DIN, un módulo SHT45, un ACS758LCB-050 en la línea de 12 V, un módulo ZMPT101B con su red divisora personalizada y un conjunto de ventiladores DC o cargas resistivas que emulan la ventilación. La alimentación de 12 V se realiza con una fuente de laboratorio; la red AC se simula inicialmente mediante un autotransformador con protección.

Instrumentos externos —un vatímetro true-RMS y un termo-higrómetro comercial— se utilizan como referencias para la calibración. El cableado se organiza en dos zonas: potencia (220/12 V, ventiladores, ACS/ZMPT primarios) y señal (módulos de sensado, KC868, SHT45), manteniendo separación física clara.

### B. Procedimiento de calibración

La calibración sigue una metodología de tres puntos para tensión y corriente:

* Aplicar tres tensiones AC conocidas (por ejemplo 180, 220 y 240 V) y medir Vrms con el vatímetro y con el prototipo; ajustar (K_V) para minimizar el error cuadrático medio.
* Conducir corrientes DC de 5, 15 y 30 A a través del ACS758 utilizando cargas controladas y un amperímetro de referencia; ajustar (K_I).
* Registrar offsets de sensor sin carga (cero corriente y, en el caso de tensión, salida del ZMPT con entrada desconectada).

Los factores obtenidos se programan en el firmware como constantes de calibración iniciales, con posibilidad de reajuste fino en campo.

### C. Resultados representativos

Ensayos preliminares muestran:

* Lecturas de tensión dentro de ±2 % del vatímetro en el rango 200–230 V.
* Lecturas de corriente con errores inferiores a ±5 % para corrientes superiores a ≈0,3 A; las corrientes muy bajas quedan dominadas por ruido y por la resolución del ADC.
* Seguimiento de temperatura y humedad muy próximo al termo-higrómetro de referencia, con errores compatibles con las especificaciones del SHT45.

El entorno de Simulink reproduce estas tendencias cuando se emplean las mismas constantes de calibración y niveles de ruido, lo que confirma la consistencia interna entre modelo y hardware.

---

## VIII. Discusión

El prototipo demuestra que un controlador basado en ESP32 y módulos de sensado comerciales puede ofrecer mediciones razonablemente precisas de microclima y potencia, mientras acciona ventilación de forma automática en un contexto de invernadero. El aspecto más delicado es el frente analógico de tensión: alimentar el ZMPT101B a 12 V y atenuar con un divisor introduce ruido adicional y dependencia de tolerancias de resistores, pero simplifica el cableado al reutilizar el riel de 12 V. Una versión más industrial posiblemente migraría a VCC = 5 V con un front-end de instrumentación dedicado.

El ACS758 alimentado a 3,3 V ofrece aislamiento y robustez, pero su sensibilidad a corrientes bajas es limitada; para ventiladores de alta eficiencia o variadores de frecuencia podría requerirse un rango de corriente más bajo o sensores Hall alternativos.

En software, la combinación de sobre-muestreo, promedios móviles y estimación RMS resulta suficiente para estabilizar las lecturas, pero el método pico-a-pico es vulnerable a formas de onda distorsionadas. La implementación de RMS discreto pleno o de análisis FFT mejoraría la exactitud bajo cargas no lineales.

Finalmente, aunque el nodo es compacto y funcional, su carácter “industrial” depende de envolventes certificadas, verificación formal de creepage/clearance, fuentes de alimentación con certificación y cumplimiento de normas EMC y de seguridad.

---

## IX. Trabajo futuro: hacia un nodo AIoT

La evolución natural de este proyecto es un dispositivo AIoT listo para producción, con varias mejoras:

* Migración a un stack de comunicación estándar (MQTT sobre TLS, Modbus/TCP, OPC UA opcional) con autenticación basada en roles y cifrado.
* Integración con una base de datos de series de tiempo y una plataforma de dashboards (por ejemplo, Grafana) para supervisar múltiples invernaderos.
* Uso de los datos de energía y clima para entrenar modelos de detección de anomalías que identifiquen fallas tempranas de ventiladores, obstrucciones o fugas de agua.
* Implementación de estrategias de control avanzadas: control predictivo basado en modelo (MPC) utilizando los modelos térmicos de Simulink, optimización multiobjetivo que equilibre confort vegetal y costo energético, y sintonía adaptativa de PID.
* Refinamiento del frente analógico (amplificadores de instrumentación, referencias de precisión, mejor protección ESD/EMC) para aproximarse a la exactitud de medidores comerciales manteniendo bajo costo.

---

## X. Conclusiones

Se ha presentado un diseño completo, centrado en la instrumentación, para un controlador inteligente de invernadero basado en ESP32. El sistema integra medición de microclima, medición de potencia y accionamiento de ventiladores, con un nivel de detalle suficiente en hardware, acondicionamiento de señal, firmware y modelado en MATLAB/Simulink. Al fundamentar el diseño en modelos explícitos de sensores y en ruido realista, el sistema se puede sintonizar y validar antes de su instalación, reduciendo riesgo y tiempo de puesta en marcha.

La arquitectura es lo bastante flexible como para servir como plataforma docente en cursos de electrónica avanzada e instrumentación, y como base para un nodo AIoT industrial capaz de gestión de invernaderos basada en datos y compatible con ecosistemas IIoT existentes.

---

## XI. Tabla-resumen de componentes, roles y contrapartes de simulación

| Dominio              | Componente físico / bloque           | Rol en el sistema                                 | Contraparte en Simulink                                 |
| -------------------- | ------------------------------------ | ------------------------------------------------- | ------------------------------------------------------- |
| Controlador          | KC868-A6 (ESP32 + relevadores + ADC) | MCU central, adquisición analógica, accionamiento | Bloque de lógica ESP32 (MATLAB Function + subsistemas)  |
| Sensado ambiental    | SHT45                                | Medida de temperatura y humedad (I²C digital)     | Subsistema SHT45 + adaptador i2c_to_digital             |
| Corriente            | ACS758LCB-050                        | Corriente del bus de 12 V, aislamiento galvánico  | Sensor de corriente + “C Instrumental Normalizer”       |
| Tensión AC           | ZMPT101B + red 3×330/2×220 Ω         | Medida de tensión de red, aislamiento galvánico   | Transformador + op-amp + divisor + “V Normalizer”       |
| Potencia/actuación   | Relevadores KC868 + ventiladores     | Ventilación por etapas, carga de potencia         | Interruptores lógicos + cargas de ventilador + lámparas |
| Acond. analógico     | Filtros RC, CF, divisores resistivos | Limitación de banda, escalado, protección de ADC  | Redes RC equivalentes en subsistemas de sensores        |
| Adquisición de datos | ADC ESP32 (12 bits) + sobre-muestreo | Muestreo, cálculo RMS, filtrado digital           | Bloques de cuantización y RMS                           |
| Control              | Algoritmos PID en firmware           | Regulación de T/RH vía ventilación                | Bloques PID + modelos de primer orden                   |
| Dashboards           | Servidor HTTP + (futuro) MQTT        | Monitoreo y configuración local/remota            | Bloques de salida de datos / sinks lógicos              |
| Simulación           | MATLAB/Simulink                      | Diseño, validación, análisis de escenarios        | Diagrama completo (fuente trifásica, sensores, control) |

---

## XII. Guía de README y material gráfico para el proyecto

Para transformar este trabajo en un README.md profesional de repositorio (por ejemplo en GitHub), se recomienda mantener la estructura general del paper, pero condensada en secciones y acompañada de figuras, diagramas y enlaces a archivos de proyecto.

### A. Estructura sugerida de `README.md`

1. **Título y resumen corto**

   * Reutilizar el título bilingüe y un párrafo de resumen de 4–6 líneas.

2. **Fotografía del hardware principal**

   * *Figura 1*: foto del montaje de la KC868-A6 con el SHT45, ACS758 y ZMPT101B conectados.

3. **Diagrama de bloques del sistema**

   * *Figura 2*: captura de pantalla del diagrama general en Simulink (la primera imagen que enviaste, con el flujo desde la fuente trifásica hasta la lógica ESP32).

4. **Características principales**

   * Lista breve de características: número de relevadores, sensores, rangos de medida, interfaz web.

5. **Arquitectura de hardware**

   * Resumen de la Sección III.
   * Incluir:

     * *Figura 3*: fragmento de Simulink con el subsistema de “Sensor Voltage + Current” (segunda imagen).
     * *Figura 4*: foto de detalle del cableado del ACS758 en la línea de 12 V.
     * *Figura 5*: foto de detalle del módulo ZMPT101B y la red 3×330/2×220 Ω montada en protoboard o PCB.

6. **Arquitectura de software**

   * Explicar brevemente las tareas de sensado, control y comunicación.
   * Incluir pseudocódigo o diagrama de flujo simple del firmware.
   * Enlazar al archivo de firmware:

     * `[Firmware ESP32 (Arduino)](./sketch_nov24a.ino)`

7. **Modelado en MATLAB/Simulink**

   * Describir qué se modela y para qué.
   * Incluir:

     * *Figura 6*: captura del subsistema del SHT45 (tercera imagen).
     * *Figura 7*: captura del bloque de PIDs (cuarta imagen).
   * Enlazar al archivo de modelo (cuando lo tengas en el repo):

     * `[Modelo Simulink](./simulink/greenhouse_model.slx)`

8. **Resultados y gráficas**

   * *Figura 8*: gráfico de temperatura y humedad vs. tiempo donde se observe la acción de los ventiladores.
   * *Figura 9*: gráfico de tensión, corriente y potencia vs. tiempo.
   * *Figura 10*: captura de pantalla del dashboard web local mostrando métricas en vivo.

9. **Calibración y pruebas**

   * Breve resumen de cómo se calibran VRMS e IRMS.
   * Referenciar un documento más largo si lo tienes, por ejemplo:

     * `[Informe de corte 2](./Corte2.md)`

10. **Cómo reproducir el proyecto**

    * Sección con pasos claros:

      * Clonar el repositorio.
      * Cargar `sketch_nov24a.ino` en el ESP32 (seleccionando la placa adecuada).
      * Conectar sensores según diagramas (añadir un esquema de pines).
      * Abrir el modelo de Simulink para experimentar con la simulación.

11. **Trabajo futuro / Roadmap**

    * Lista de tareas planificadas (integrar MQTT, AIoT, mejorar front-end analógico, etc.).

12. **Licencia y agradecimientos**

    * Mencionar a Universidad de La Sabana y una nota de agradecimiento a Agrar Ingeniería SAS por apoyo en componentes.

### B. Capturas de pantalla y fotografías recomendadas

Concretando, las imágenes mínimas sugeridas son:

1. **Figura 1 – Vista general del montaje físico**

   * Foto superior de la KC868-A6, módulos de sensores y ventiladores conectados.
   * Ubicación recomendada: sección “Visión general” del README.

2. **Figura 2 – Diagrama de bloques en Simulink**

   * Pantallazo de todo el modelo (la primera imagen que compartiste).
   * Ubicación: sección “Modelado en MATLAB/Simulink”.

3. **Figura 3 – Subsistema de medición de potencia**

   * Pantallazo ampliado del subsistema donde están ACS758, ZMPT y normalizadores (segunda imagen).
   * Resalta la relación entre sensores físicos y bloques de simulación.

4. **Figura 4 – Subsistema SHT45 + adaptador I²C**

   * Pantallazo de la tercera imagen con el bloque SHT45.

5. **Figura 5 – Bloques PID**

   * Pantallazo de la cuarta imagen donde se ven los cuatro PIDs para C, V, H y T.

6. **Figura 6 – Detalle de cableado del ACS758**

   * Foto cercana mostrando cómo el bus de 12 V pasa por IP+/IP- y la conexión a A1.

7. **Figura 7 – Detalle de cableado del ZMPT101B y red resistiva**

   * Foto donde se aprecie el módulo ZMPT y los cinco resistores (3×330, 2×220) conectados, con anotaciones opcionales.

8. **Figura 8 – Dashboard web local**

   * Captura de la página que muestra temperatura, humedad, potencia y estado de relevadores.

9. **Figura 9 – Gráficas de resultados**

   * Una o dos gráficas exportadas desde MATLAB o Python con T/RH y potencia a lo largo del tiempo, marcando cuándo se encienden ventiladores.

Todas las figuras deben tener pie de figura conciso pero técnico, por ejemplo:
*“Figura 3. Subsistema de medición de tensión y corriente en Simulink, emulando ACS758 y ZMPT101B con sus redes de acondicionamiento.”*

### C. Enlaces a archivos del proyecto

En el README conviene agrupar los archivos clave con enlaces directos:

* Código Arduino / ESP32:

  * `[sketch_nov24a.ino](./sketch_nov24a.ino)`

* Documentación extendida:

  * `[README detallado (versión larga)](./README.md)`
  * `[Informe de Corte 2](./Corte2.md)`

* Modelos de simulación (cuando estén disponibles en el repo):

  * `./simulink/greenhouse_model.slx`
  * `./simulink/power_frontend.slx`

* Esquemáticos y PCB (si generas KiCad/Altium):

  * `./hardware/esquematico_kc868_extension.pdf`
  * `./hardware/pcb_frontend_acs_zmpt.kicad_pcb`

Con esta combinación de texto técnico, figuras bien ubicadas y enlaces a código y modelos, el proyecto se presenta simultáneamente como un artículo de nivel IEEE y como un README profesional que guía a cualquier lector —ingeniero, docente o estudiante— desde la idea hasta la replicación práctica del sistema.
