# Requerimientos Fase 3: MQTT remoto

1. El host del broker debe aceptar nombre de dominio (resolución DNS) además de IP.
   Verificar que "set mqtt <host> <puerto>" y el formulario web lo permitan.
2. El prefijo de los tópicos (hoy "solar_tracker") debe ser configurable,
   persistente en NVS, con comandos get/set topic y campo en el formulario.
   Valor por defecto por macro: "utn_frr/seguidor_tissera_molinari".
3. El client ID MQTT debe ser único (por ejemplo con el MAC del equipo),
   porque dos clientes con el mismo ID se desconectan entre sí en el broker.
4. Actualizar la tabla de comandos y la sección MQTT del README.