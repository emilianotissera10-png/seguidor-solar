# Requerimiento 1 (req1.md): Seguidor Solar Bi-Axial con ESP32 y FreeRTOS

## Rol
Actúa como un Ingeniero Experto en Sistemas Embebidos, C++, FreeRTOS y PlatformIO para la cátedra de Electrónica II (UTN FRRO). Tu objetivo es diseñar e implementar el firmware completo, modular, escalable y no bloqueante de un Seguidor Solar Bi-Axial de lazo cerrado.

## 1. Configuración de PlatformIO (`platformio.ini`)
Configura el archivo `platformio.ini` en la raíz del proyecto con el siguiente entorno y librerías:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    jpb10/SolarCalculator @ ^2.0.1
    knolleary/PubSubClient @ ^2.8
    bblanchon/ArduinoJson @ ^7.0.4
    adafruit/Adafruit SSD1306 @ ^2.5.9
    adafruit/Adafruit GFX Library @ ^1.11.9
```

## 2. Arquitectura Modular Orientada a Objetos (Clases en `.h` y `.cpp`)
Cada módulo debe construirse como una clase C++ separando su declaración en `include/` (`.h`) y su implementación en `src/` (`.cpp`). Todos los comentarios dentro del código fuente deben estar en inglés:

- `include/Config.h`: Macros (`#define`) con pines de hardware, valores por defecto de credenciales WiFi, IP/puerto del Broker MQTT (Mosquitto), coordenadas geográficas iniciales (Rosario: Latitud `-32.95`, Longitud `-60.66`, UTC `-3`), límites mecánicos y cotas de calibración ADC.
- `include/StorageManager.h` y `src/StorageManager.cpp`: Clase que encapsula la memoria flash no volátil **NVS** (`Preferences.h`). Carga valores por defecto desde `Config.h` si no existen en NVS y permite leer/guardar configuraciones en tiempo real.
- `include/SensorManager.h` y `src/SensorManager.cpp`: Clase para lectura analógica en pines del bloque **ADC1** con sobremuestreo (promedio de muestras) e interpolación lineal a grados para los 2 potenciómetros de posición y lectura de tensión del mini panel solar.
- `include/MotorController.h` y `src/MotorController.cpp`: Clase con **Máquina de Estados Finitos (FSM) no bloqueante** para accionar 2 motores paso a paso unipolares `28BYJ-48` mediante drivers `ULN2003` (eje Azimut y eje Elevación). Incluye banda muerta (`deadband` de 1.5°) y desenergización automática de bobinas en reposo.
- `include/SolarCalculatorModule.h` y `src/SolarCalculatorModule.cpp`: Clase que envuelve la librería `SolarCalculator` para calcular el Azimut y Elevación solar objetivo según la fecha/hora UTC (vía NTP) y la latitud/longitud almacenadas en NVS.
- `include/DisplayManager.h` y `src/DisplayManager.cpp`: Clase que controla la pantalla OLED I2C `SSD1306` (128x64, dirección `0x3C`) mostrando en pantalla:
  1. **Estado de conexión WiFi** claro en el encabezado (`WIFI:OK` cuando está conectado a la red, `WIFI:AP` en modo portal cautivo o `WIFI:--` si está desconectado), estado MQTT y hora actual.
  2. **Latitud y Longitud** actuales leídas desde NVS (`Lat: -32.95 Lon: -60.66`).
  3. **Ángulos de Azimut y Elevación** (valor real vs. objetivo).
  4. **Radiación solar** medida en el **GPIO 35** en Voltios (`Rad: X.XX V`) y barra gráfica proporcional.
- `include/CommandProcessor.h` y `src/CommandProcessor.cpp`: Clase procesadora de comandos `get` y `set` desacoplada de la capa física. Recibe un string de comando desde cualquier interfaz (Serial, MQTT o HTTP Web), ejecuta la acción sobre NVS o el sistema y devuelve la respuesta formateada.
- `include/NetworkManager.h` y `src/NetworkManager.cpp`: Clase no bloqueante que gestiona WiFi (Modo Station + Fallback a Modo Access Point si no conecta), sincronización horaria NTP (`pool.ntp.org`), cliente MQTT (publicación periódica de telemetría JSON y suscripción a comandos en broker Mosquitto) y el **WebServer HTTP local**.
- `src/main.cpp`: Orquestador que inicializa los objetos, crea las colas/mutex de sincronización y lanza las tareas de FreeRTOS en cada núcleo.

## 3. Asignación de Pines de Hardware (Bloque ADC1 obligatorio)
- **Motor Azimut (ULN2003):** IN1 = GPIO 19, IN2 = GPIO 18, IN3 = GPIO 5, IN4 = GPIO 17. Sentido: invertido (`true`).
- **Motor Elevación (ULN2003):** IN1 = GPIO 16, IN2 = GPIO 4, IN3 = GPIO 2, IN4 = GPIO 15. Sentido: directo (`false`).
- **Potenciómetro Azimut (ADC1):** GPIO 34.
  - Calibración NVS por defecto: `ADC_AZ_MIN = 2530` (-70.0°), `ADC_AZ_MAX = 430` (+70.0°).
- **Potenciómetro Elevación (ADC1):** GPIO 32.
  - Calibración NVS por defecto: `ADC_EL_0 = 3000` (0.0° horizontal), `ADC_EL_90 = 1500` (90.0° cenit vertical).
- **Sensor Radiación Mini Panel Solar (ADC1):** GPIO 35.
- **Display OLED SSD1306 (I2C):** SDA = GPIO 21, SCL = GPIO 22.
- **Banda Muerta de Control (Deadband):** 2.0° de tolerancia en ambos ejes con corte de bobinas en reposo.

## 4. Distribución Dual-Core con FreeRTOS y Máquina de Estados
Implementar multitarea real en FreeRTOS utilizando `xTaskCreatePinnedToCore` y protegiendo datos compartidos mediante Mutex (`SemaphoreHandle_t`):

- **Núcleo 0 (Tarea de Comunicaciones - `Core0_NetworkTask`):**
  - Máquina de estados de conectividad WiFi no bloqueante. Si el WiFi no conecta tras los reintentos iniciales o se traslada el equipo a otro lugar, levanta un Access Point (`ESP32_Solar_Config`) sin bloquear el seguimiento solar ni requerir reflasheo.
  - Servidor Web HTTP en puerto 80 con una página HTML liviana que permita configurar SSID, contraseña WiFi, IP del broker MQTT, latitud y longitud, guardándolos en NVS.
  - Mantenimiento de conexión y reconexión automática no bloqueante al broker MQTT Mosquitto.
  - Publicación periódica de telemetría en formato JSON en el tópico `solar_tracker/telemetria` (`azimut_real`, `azimut_obj`, `elevacion_real`, `elevacion_obj`, `radiacion_v`).
  - Recepción de comandos por MQTT (`solar_tracker/comandos`), HTTP y Puerto Serie, enviándolos al `CommandProcessor`.

- **Núcleo 1 (Tarea de Control y Medición - `Core1_ControlTask`):**
  - Máquina de estados finitos (FSM) no bloqueante (prohibido usar `delay()` largos que congelen el lazo).
  - Lectura de configuración y persistencia en NVS.
  - Adquisición y filtrado de sensores en ADC1.
  - Cálculo astronómico periódico (según el intervalo dinámico guardado en NVS).
  - Lazo cerrado de posicionamiento paso a paso de ambos motores hasta entrar en la zona muerta de tolerancia y corte de energía en bobinas.
  - Actualización periódica de la pantalla OLED I2C.

## 5. Procesador de Comandos (`get` / `set`) Multicanal
El módulo `CommandProcessor` debe procesar comandos en texto plano o JSON provenientes de **Serial (obligatorio siempre activo)**, **MQTT** y **WebServer**:
- `get status` -> Devuelve JSON/texto con ángulos actuales, objetivos, tensión del panel, IP y estado WiFi/MQTT.
- `get wifi` / `set wifi <ssid> <pass>` -> Consulta o guarda credenciales WiFi en NVS.
- `get mqtt` / `set mqtt <ip> <port>` -> Consulta o guarda datos del broker Mosquitto en NVS.
- `get location` / `set location <lat> <lon>` -> Consulta o guarda latitud y longitud en NVS.
- `get interval` / `set interval <ms>` -> Consulta o modifica en tiempo real el intervalo de seguimiento astronómico en NVS.
- `get cal` / `set cal <az_min> <az_max> <el_min> <el_max>` -> Consulta o calibra los extremos crudos del ADC en NVS.

## 6. Validación y Cierre
- Compilar el proyecto con PlatformIO al finalizar para verificar que no existan errores (`[SUCCESS]`).
- **Si falta contexto o tienes cualquier duda sobre algún requerimiento, pregunta antes de empezar a programar.**