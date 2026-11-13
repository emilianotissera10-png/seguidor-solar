# Seguidor Solar Bi-Axial

Firmware para un seguidor solar de dos ejes y lazo cerrado, con ESP32 y FreeRTOS. Calcula la posición del sol a partir de la hora y la ubicación, y mueve dos motores paso a paso hasta alcanzarla. Se configura por Serial, MQTT o desde una página web.



## Tabla de contenidos

- [Hardware](#hardware)
- [Compilar y cargar](#compilar-y-cargar)
- [Puesta en marcha](#puesta-en-marcha)
- [Funcionamiento](#funcionamiento)
- [Conexión WiFi e interfaz web](#conexión-wifi-e-interfaz-web)
- [Comandos](#comandos)
- [MQTT](#mqtt)
- [Pantalla OLED](#pantalla-oled)
- [Configuración por defecto](#configuración-por-defecto)
- [Si los motores no se mueven](#si-los-motores-no-se-mueven)
- [Limitaciones conocidas](#limitaciones-conocidas)

## Hardware

| Componente | Señal | GPIO |
|---|---|---|
| Motor azimut 28BYJ-48 con ULN2003 (sentido invertido) | IN1 / IN2 / IN3 / IN4 | 19 / 18 / 5 / 17 |
| Motor elevación 28BYJ-48 con ULN2003 (sentido directo) | IN1 / IN2 / IN3 / IN4 | 16 / 4 / 2 / 15 |
| Potenciómetro de azimut | ADC1 | 34 |
| Potenciómetro de elevación | ADC1 | 32 |
| Mini panel solar con resistencia de carga en paralelo | ADC1 | 35 |
| OLED SSD1306 128x64, dirección `0x3C` | SDA / SCL | 21 / 22 |

El panel se conecta sin divisor resistivo y entrega como máximo unos 2.5 V.

GPIO 2, 5 y 15 son pines de strapping. Si falla el arranque o la carga del firmware, revisa primero esas conexiones.

## Compilar y cargar

El proyecto usa PlatformIO, que descarga las librerías automáticamente.

```
pio run                 # compilar
pio run -t upload       # cargar en la placa
pio device monitor      # abrir el monitor serie (115200 baudios)
```

## Puesta en marcha

1. **Carga el firmware** con los motores listos para cortarles la alimentación, porque no hay protección contra atascos (ver [Limitaciones conocidas](#limitaciones-conocidas)).
2. **Configura la red** por Serial con `set wifi <ssid> <pass>`, o desde la [interfaz web](#conexión-wifi-e-interfaz-web).
3. **Configura el broker**, si lo usas, con `set mqtt <host> <puerto>` y, si hace falta, el prefijo de tópicos con `set topic <prefijo>`.
4. **Configura la ubicación** con `set location <lat> <lon>`, si no es Rosario.
5. **Calibra los potenciómetros** con `set cal <az_min> <az_max> <el_min> <el_max>`, si difieren de los valores por defecto.
6. **Comprueba el estado** con `get status` o en la sección **Estado** de la interfaz web.

Toda la configuración queda guardada en NVS y se conserva al reiniciar.

## Funcionamiento

**Convención de ángulos**

- **Azimut:** 0° apunta al norte, +70° al este y -70° al oeste.
- **Elevación:** 0° es horizontal y 90° es el cenit.

Si el sol queda fuera de esos rangos, el objetivo se limita al tope más cercano y el seguidor no se bloquea.

**Estados del seguimiento**

| Estado | Condición | Motores |
|---|---|---|
| Espera de hora | Todavía no hay hora válida | Quietos, con bobinas apagadas |
| Seguimiento | Hay hora válida y el sol está sobre el horizonte | Lazo cerrado |
| Noche | La elevación solar es menor a 0° | Quietos donde terminó el día, con bobinas apagadas |

**Movimiento de los motores**

- Un motor arranca cuando el error supera 2.0°.
- Se detiene cuando el error baja a 1.0° y entonces apaga sus bobinas.
- El objetivo se recalcula cada 60 s por defecto, y al cambiar la ubicación o el intervalo.

**Tareas de FreeRTOS**

| Tarea | Núcleo | Prioridad | Función |
|---|---|---|---|
| `Core0_NetworkTask` | 0 | 1 | WiFi, NTP, MQTT, web y comandos por Serial |
| `Core1_ControlTask` | 1 | 2 | Sensores, cálculo solar, lazo cerrado y OLED |
| `Core1_MotorTask` | 1 | 3 | Generación de pasos cada 4 ms |

## Conexión WiFi e interfaz web

Al arrancar, el equipo intenta conectarse a la red guardada. Tras 3 intentos fallidos de 10 s cada uno, levanta su propio punto de acceso, y lo mismo ocurre si pierde la red y no logra reconectarse. El seguimiento sigue funcionando en todos los casos, siempre que ya tenga hora válida.

**Modo estación (uso normal)**

Conectado al router, el equipo mantiene MQTT activo y sirve la interfaz web en su IP local, que aparece en la pantalla OLED (por ejemplo, `IP: 192.168.1.45`). Entra a `http://<ip>/` desde cualquier dispositivo de la misma red.

**Punto de acceso (contingencia)**

- **Red:** `ESP32_Solar_Config`
- **Contraseña:** `12345678`

Al conectarte, la página debería abrirse sola como portal cautivo. Si no ocurre, entra a `http://192.168.4.1/`.

Mientras nadie esté conectado al punto de acceso, el equipo reintenta la red guardada cada 60 s. Al conectarse, apaga el punto de acceso.

**Interfaz web**

El botón ☰ abre un menú con tres secciones:

| Sección | Contenido |
|---|---|
| Configuración | SSID, contraseña, broker MQTT (IP o dominio), puerto, prefijo de tópicos, latitud, longitud e intervalo de telemetría |
| Estado | WiFi con su IP, MQTT, azimut y elevación reales y objetivo, y tensión del panel. Se actualiza cada 2 s |
| Acerca de | Descripción del proyecto y autores |

- Si dejas la contraseña en blanco, se conserva la actual.
- Al guardar, cada campo inválido se informa y no se graba; el resto sí se guarda. Los cambios se aplican sin reiniciar.
- Rutas HTTP: `/` (página), `/config` (configuración en JSON, sin contraseña), `/status` (estado en JSON), `/save` (POST del formulario) y `/cmd` (comandos).

**Uso sin internet (por ejemplo, en una exposición)**

Conéctate al punto de acceso desde el celular, abre la página y pulsa **Guardar y sincronizar hora**. El equipo toma la hora del navegador y empieza a seguir el sol. La hora se pierde al apagarlo, así que hay que repetir este paso en cada encendido.

## Comandos

Los mismos comandos funcionan por los tres canales:

| Canal | Cómo enviarlos |
|---|---|
| Serial | Una línea de texto a 115200 baudios |
| MQTT | Publicar en `<prefijo>/comandos` |
| HTTP | `http://<ip>/cmd?c=<comando>` |

| Comando | Función |
|---|---|
| `get status` | Devuelve en JSON ángulos, objetivos, tensión del panel, IP y estado de WiFi y MQTT |
| `get wifi` | Devuelve el SSID, con la contraseña oculta |
| `set wifi <ssid> [pass]` | Guarda las credenciales y reconecta. Sin contraseña, la red se toma como abierta |
| `get mqtt` | Devuelve el host y el puerto del broker |
| `set mqtt <host> <puerto>` | Guarda el broker (IP o dominio, por ejemplo `broker.hivemq.com`) y reconecta |
| `get topic` | Devuelve el prefijo de los tópicos MQTT |
| `set topic <prefijo>` | Guarda el prefijo y reconecta. Sin espacios, `+`, `#` ni `/` al inicio o al final |
| `get location` | Devuelve la latitud y la longitud |
| `set location <lat> <lon>` | Guarda la ubicación y recalcula el objetivo |
| `get interval` | Devuelve el intervalo de seguimiento en ms |
| `set interval <ms>` | Cambia el intervalo, de 1000 a 86400000 ms |
| `get telemetry` | Devuelve el intervalo de telemetría MQTT en ms |
| `set telemetry <ms>` | Cambia el intervalo de telemetría, de 1000 a 3600000 ms |
| `get cal` | Devuelve la calibración del ADC |
| `set cal <az_min> <az_max> <el_min> <el_max>` | Guarda las cuentas crudas del ADC en -70°, +70°, 0° y 90° |

**Formatos**

- Los argumentos con espacios van entre comillas: `set wifi "Mi Red" clave1234`.
- También se acepta JSON: `{"cmd":"set location -32.95 -60.66"}`.
- La contraseña WiFi debe estar vacía o tener de 8 a 63 caracteres.
- Una contraseña o SSID con comillas dobles no se puede enviar por comando; usa el portal web.

**Respuestas**

Los `set` responden `OK` o un mensaje que empieza con `ERR:`.

## MQTT

- **Broker:** se indica por IP o por nombre de dominio, que se resuelve por DNS en cada intento de conexión. Sirve tanto un broker local como uno remoto en internet.
- **Autenticación:** el cliente se conecta sin usuario ni contraseña.
- **Client ID:** es `ESP32_Solar_` más los 48 bits del MAC del equipo, así que no se repite entre placas. Dos clientes con el mismo ID se desconectarían entre sí en el broker.
- **Tópicos:** se forman con un prefijo configurable (`get/set topic` o la interfaz web). Por defecto es `utn_frr/seguidor_tissera_molinari`.

| Tópico | Sentido | Contenido |
|---|---|---|
| `<prefijo>/telemetria` | Publica cada 5 s (configurable) | Telemetría en JSON |
| `<prefijo>/comandos` | Recibe | Comandos |
| `<prefijo>/respuestas` | Publica | Respuesta a cada comando |

Para monitorear desde una PC con Mosquitto, por ejemplo con el prefijo por defecto:

```
mosquitto_sub -h <broker> -t "utn_frr/seguidor_tissera_molinari/#" -v
mosquitto_pub -h <broker> -t "utn_frr/seguidor_tissera_molinari/comandos" -m "get status"
```

```json
{"azimut_real":12.5,"azimut_obj":13.1,"elevacion_real":45.2,"elevacion_obj":44.8,"radiacion_v":1.84}
```

Los objetivos valen `null` hasta que el equipo tiene hora válida.

## Pantalla OLED

```
WIFI:OK MQTT:OK 14:23
IP: 192.168.1.45
Lat:-32.95 Lon:-60.66
Az:  12.5 / 13.1
El:  45.2 / 44.8
Rad: 1.84 V
[█████████         ]
```

- **WiFi:** `OK` indica conectado, `AP` indica punto de acceso y `--` indica desconectado.
- **IP:** dirección de la interfaz web; `192.168.4.1` en modo punto de acceso y `--` sin conexión.
- **Ángulos:** se muestra el valor real y luego el objetivo.
- **Hora:** se muestra en UTC-3, o `--:--` si todavía no es válida.
- **Barra:** es proporcional a la tensión del panel, con el máximo en 3.3 V.

## Configuración por defecto

Los valores por defecto están en [include/Config.h](include/Config.h). Los marcados como persistentes se pueden cambiar por comando y quedan en NVS; el resto requiere recompilar.

| Parámetro | Valor | Persistente |
|---|---|---|
| WiFi | `MiRed` / `MiPassword` (ejemplo) | Sí |
| Broker MQTT | `192.168.1.100:1883` (ejemplo) | Sí |
| Prefijo de tópicos MQTT | `utn_frr/seguidor_tissera_molinari` | Sí |
| Ubicación | Rosario: -32.95, -60.66 | Sí |
| Intervalo de seguimiento | 60 s | Sí |
| Calibración de azimut | 2530 (-70°), 430 (+70°) | Sí |
| Calibración de elevación | 3000 (0°), 1500 (90°) | Sí |
| Telemetría | 5 s | Sí |
| Zona horaria de la pantalla | UTC-3 | No |
| Contraseña del punto de acceso | `12345678` | No |

## Si los motores no se mueven

Consulta `get status` (o la sección **Estado** de la web):

| Síntoma | Causa | Solución |
|---|---|---|
| Objetivos en `null` | No hay hora válida | Configura una WiFi con internet para NTP, o sincroniza la hora desde la web |
| Elevación objetivo en `0` | Es de noche | Normal. Para probar, `set location -32.95 119.34` simula el día; luego restaura la ubicación |
| Error menor a 2° | Ya está en posición | Normal: corrige cada varios minutos con movimientos breves |
| Azimut objetivo en `±70` | Sol fuera del rango mecánico | Normal a primera y última hora |
| Error grande sin movimiento | Hardware | Revisa los LEDs del ULN2003, la GND común, la fuente de 5 V y los potenciómetros |

## Limitaciones conocidas

- **Sin protección contra atascos:** si un potenciómetro se desconecta o está mal calibrado, o un sentido de giro está invertido, el motor sigue empujando contra el tope.
- **Sin lectura cruda del ADC:** ningún comando muestra las cuentas actuales de los potenciómetros, así que hoy hay que obtener por otro medio los valores para `set cal`.
- **Sin autenticación en HTTP ni MQTT:** quien acceda a la red o al broker puede cambiar la configuración.
- **Sin reloj de respaldo:** sin NTP ni portal, tras un encendido no hay hora y los motores no se mueven.
- **Flash al 93.2 %:** queda poco margen para agregar funciones. Se puede ganar espacio con `board_build.partitions = huge_app.csv`, que deja sin OTA.
