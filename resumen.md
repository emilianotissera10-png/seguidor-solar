# Resumen de sesiones

Memoria del proyecto para retomar el trabajo. La guía de uso está en [README.md](README.md) y los requerimientos en [req1.md](req1.md), [req2.md](req2.md) y [req3.md](req3.md).

## Tabla de contenidos

- [Estado actual](#estado-actual)
- [Estructura del código](#estructura-del-código)
- [Decisiones del usuario](#decisiones-del-usuario)
- [Decisiones de implementación](#decisiones-de-implementación)
- [Entorno de compilación](#entorno-de-compilación)
- [Próximos pasos](#próximos-pasos)

## Estado actual

- Implementados [req1.md](req1.md) (sesión 1), y [req2.md](req2.md) y [req3.md](req3.md) (sesión 2).
- La última compilación dio `[SUCCESS]`, con RAM al 15.5 % y flash al 93.2 %.
- **Nada se probó en hardware:** motores, sentidos de giro, sensores, OLED, WiFi, interfaz web, NTP y MQTT siguen sin verificar.
- El usuario reportó que el seguidor no se movía. Se le pasó un diagnóstico basado en `get status`, que ahora figura en el README, y está pendiente su respuesta. Por lectura del código no se encontró ningún error.
- `README.md` y `resumen.md` quedaron actualizados al cierre de la sesión 2.

**Cambios de la sesión 2 (req2)**

- **Pantalla OLED:** agrega la línea `IP: x.x.x.x` debajo del estado; para que entre todo, las líneas pasan a 9 px.
- **Interfaz web:** página única estática (`kPage`) con menú hamburguesa y tres secciones: Configuración, Estado y Acerca de. Usa las rutas `/config` y `/status` y guarda en `/save` con `fetch`.
- **Telemetría:** intervalo persistido en NVS (clave `telemetry`, de 1000 a 3600000 ms), con comandos `get/set telemetry` y aplicación en caliente.
- **Formulario web:** suma el puerto MQTT.

**Cambios de la sesión 2 (req3)**

- **Host del broker:** acepta IP o nombre de dominio (`mqttHost[64]`, validado por `isValidHost`); `WiFiClient` lo resuelve por DNS en cada intento. `get mqtt` responde `host=`.
- **Prefijo de tópicos:** persistido en NVS (clave `mqtt_topic`), por defecto `DEFAULT_MQTT_TOPIC_PREFIX`. Tiene comandos `get/set topic` y campo en la web.
  - Los tópicos se arman en `buildTopics()` como prefijo más los sufijos `MQTT_SUFFIX_*`.
  - Al cambiar el prefijo, el host o el puerto, el equipo se reconecta y se vuelve a suscribir.
- **Client ID:** `ESP32_Solar_` más los 48 bits del MAC. Antes usaba solo los 32 bits bajos, que incluyen el OUI común a todos los ESP32 y podían repetirse entre placas.
- **README:** req3 pedía actualizar la tabla de comandos y la sección MQTT al implementarlo, así que se editó antes del cierre.

## Estructura del código

| Módulo | Función |
|---|---|
| `include/Config.h` | Macros de pines, valores por defecto, límites, tiempos y tareas |
| `include/SystemState.h` | Datos de ejecución compartidos entre núcleos, con mutex. Solo tiene `.h` |
| `StorageManager` | NVS con copia en RAM, validación en los setters y contador `revision()` |
| `SensorManager` | Lectura ADC1 con 16 muestras e interpolación a grados |
| `MotorController` | FSM por motor: `update()`/`stop()` en control y `tick()` en la tarea de motores |
| `SolarCalculatorModule` | Cálculo solar y conversión a ángulos mecánicos limitados |
| `DisplayManager` | Pantalla OLED, con la IP de acceso. Se desactiva si no responde por I2C |
| `CommandProcessor` | Comandos get/set definidos en la tabla `kCommands` |
| `ConnectivityManager` | FSM de WiFi, punto de acceso, DNS, NTP, MQTT y servidor web con la página `kPage` |
| `src/main.cpp` | Inicialización, mutex, tareas, FSM de seguimiento y lectura de Serial |

## Decisiones del usuario

- **Banda muerta:** 2.0°; el valor de 1.5° de la sección 2 del req queda descartado.
- **Azimut:** 0° = norte, +70° = este, -70° = oeste, con el objetivo limitado a ±70°.
- **Noche:** quieto donde terminó el día y con bobinas apagadas; sin posición de reposo.
- **Sin hora válida:** los motores no se mueven. El portal toma la hora del navegador al guardar, para exposiciones sin internet.
- **Panel:** sin divisor, con macros `V_REF` 3.3 y `ADC_RES` 4095.0.
- **Pasos:** 4 ms por paso, en una tarea separada de mayor prioridad.
- **MQTT:** `192.168.1.100:1883` de ejemplo, sin autenticación, telemetría cada 5 s y cálculo solar cada 60 s.
- **Seguridad:** AP con WPA2 y contraseña `12345678` por macro, portal cautivo real con `DNSServer` y contraseña oculta en `get wifi`.

## Decisiones de implementación

Estas decisiones las tomó Claude durante la implementación. Salvo las aclaradas, se informaron al usuario, que no las objetó.

- **`NetworkManager` renombrado a `ConnectivityManager`:** el core 3.3.12 ya trae un archivo y una clase con ese nombre.
- **Tópico `solar_tracker/respuestas`:** agregado porque el req no define dónde responder los comandos MQTT.
- **`STOP_BAND_DEG` de 1.0°:** el motor arranca con más de 2.0° de error y para en 1.0°, para que el ruido no lo reactive.
- **Escritura en NVS desde el núcleo 0:** el req la ubica en el núcleo 1. Los `set` escriben desde la tarea de red con mutex, y el control detecta el cambio con `revision()`.
- **Sin colas:** todos los comandos se atienden en la tarea de red, así que no hay llamadas concurrentes al procesador.
- **Medio paso:** los motores usan la secuencia de 8 fases. Esta decisión no se mencionó expresamente al usuario.
- **Reintentos en modo AP:** cada 60 s y solo sin clientes conectados, porque el intento de conexión puede cambiar de canal y cortar el portal. Tampoco se mencionó expresamente.
- **Texto del OLED:** `Lat:-32.95 Lon:-60.66`, sin espacios, para que entre en 21 caracteres.
- **Punto 1 de req2 sin cambios en el código:** el servidor web ya arrancaba en `begin()` y atendía en modo estación y en punto de acceso, con `handleClient()` en el núcleo 0. Se informó al usuario.
- **Textos de la web en castellano:** antes estaban en inglés. Se informó al usuario.
- **Página estática con `/config` en JSON:** reemplaza el HTML armado en el servidor, así que se eliminaron `escapeHtml` y `appendField`. La contraseña nunca se envía al navegador.
- **`/status` reutiliza `get status`** del procesador de comandos.
- **Constantes renombradas:** `TELEMETRY_INTERVAL_MS` pasó a `DEFAULT_TELEMETRY_INTERVAL_MS`, y se agregaron `TELEMETRY_INTERVAL_MIN_MS` y `TELEMETRY_INTERVAL_MAX_MS`.

## Entorno de compilación

- **Plataforma:** `espressif32` 8.0.0, sin versión fijada en `platformio.ini`.
- **Core:** Arduino 3.3.12. Una actualización de la plataforma podría cambiarlo.
- **Librerías instaladas:** SolarCalculator 2.0.2, PubSubClient 2.8.0, ArduinoJson 7.4.3, Adafruit SSD1306 2.5.17 y Adafruit GFX 1.13.0.
- **PlatformIO:** en esta máquina, `pio` está en `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`.

## Próximos pasos

1. Probar en hardware, con los motores listos para cortarles la alimentación:
   - comprobar que el ángulo aumenta al moverse hacia el objetivo en ambos ejes, y corregir `AZ_MOTOR_INVERTED` o `EL_MOTOR_INVERTED` si hace falta;
   - verificar las lecturas de los potenciómetros y del panel;
   - probar el OLED con la IP, la interfaz web en modo estación y como portal cautivo en un celular, la sincronización de hora, NTP y MQTT, incluyendo un broker remoto por dominio y el prefijo de tópicos;
   - resolver el reporte de motores sin movimiento con la salida de `get status` y el estado de los LEDs del ULN2003.
2. Definir con el usuario estas mejoras, que no están implementadas:
   - protección contra atascos, por ejemplo cortar el motor si el ángulo no cambia;
   - comando para leer las cuentas crudas del ADC, útil para `set cal`;
   - comando de prueba para mover cada motor a mano sin depender de la hora (ofrecido, sin respuesta).
3. Considerar `board_build.partitions = huge_app.csv` si la flash resulta insuficiente.
