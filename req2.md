# Requerimientos Fase 2: Refactorización del Webserver a Modo Estación (STA) e Interfaz Avanzada

## Contexto y Problemas a Resolver

1. **Error de Arquitectura de Red:** El servidor web de configuración actual funciona como un **portal cautivo** (Modo AP en `192.168.4.1`). Al levantar este portal, el ESP32 se desconecta del router local, perdiendo la comunicación con el broker MQTT y violando los requerimientos de la cátedra de mantener la conexión y permitir cambios en tiempo real.

2. **Interfaz Gráfica Básica:** Se requiere mejorar la interfaz de usuario agregando navegación y más parámetros dinámicos, además de mostrar la IP de acceso en el display físico del prototipo.

## Tareas a Ejecutar por el Asistente (Claude)

### 1. Refactorización de Red: Migrar a Modo Estación (STA)

* Desvincular el inicio del servidor web (`server.begin()`) de la lógica exclusiva de contingencia (Modo AP).

* Iniciar el servidor web inmediatamente después de que el ESP32 se conecte exitosamente al WiFi local (`WiFi.status() == WL_CONNECTED`).

* Mantener la llamada `server.handleClient()` corriendo continuamente de forma no bloqueante en el `loop()` (dentro del Core 0 si se usa FreeRTOS).

* **Seguridad:** Mantener el Modo AP solo como contingencia (fallback) en caso de que no existan credenciales guardadas o el router no responda.

### 2. Actualización de la Pantalla OLED (`display.cpp`)

* Una vez que el ESP32 se conecte a la red WiFi, la pantalla OLED SSD1306 debe mostrar la IP local asignada por el router (ej. `IP: 192.168.1.45`) para que el usuario sepa a qué dirección ingresar desde su navegador.

### 3. Nueva Interfaz Web (HTML/CSS/JS Embebido)

Reescribir el código HTML/CSS servido por el ESP32 para incluir un **Menú Hamburguesa** que permita navegar entre 3 secciones (puede ser implementado mediante pestañas/divs ocultos manejados con JavaScript simple en una sola página para no sobrecargar las rutas HTTP):

* **Sección 1: Configuración (Formulario actualizado)**
  Debe incluir los campos existentes y agregar los nuevos parámetros, persistiendo todos en la memoria flash (NVS / `Preferences`):

  * Red WiFi (SSID)

  * Contraseña WiFi

  * IP del Broker MQTT

  * **Puerto MQTT:** (Nuevo - Por defecto `1883`)

  * Latitud

  * Longitud

  * **Intervalo de Telemetría:** (Nuevo - Por defecto `5000` ms)

* **Sección 2: Estado (Status)**

  * Mostrar de forma visual (solo lectura) el estado actual del sistema: Si está conectado al WiFi, si está conectado a MQTT, el ángulo de azimut actual, el de elevación y el voltaje del panel solar. (Puede requerir crear una ruta web extra en el ESP32 que devuelva un JSON para que la página lo consulte por AJAX/Fetch).

* **Sección 3: Acerca de (About)**

  * Texto descriptivo estático del proyecto.

  * Autores a incluir textualmente: "Damián Molinari y Emiliano Tissera".

### 4. Persistencia Completa (Storage / NVS)

* Asegurar que el nuevo puerto MQTT y el intervalo de telemetría se graben correctamente usando `Preferences.h` y se apliquen en caliente sin necesidad de reiniciar físicamente la placa.