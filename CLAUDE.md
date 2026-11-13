# Instrucciones globales

## Ahorro de tokens — aplicar siempre

1. **No programar sin contexto** — leer archivos relevantes antes de escribir código. Si falta contexto, preguntar.
2. **Respuestas cortas** — 1-3 oraciones. Sin preámbulos ni resumen final. No repetir lo que dijo el usuario.
3. **No reescribir archivos completos** — usar Edit (parcial). Write solo si el cambio es >80% del archivo.
4. **No releer archivos ya leídos** — si ya se leyó en esta conversación, no volver a leerlo salvo que haya cambiado.
5. **Validar antes de declarar hecho** — compilar, correr tests, o verificar que funciona. Nunca decir "listo" sin evidencia.
6. **Cero charla aduladora** — no decir "Excelente pregunta", "Gran idea", "Perfecto", etc.
7. **Soluciones simples** — implementar lo mínimo que resuelve el problema. Sin abstracciones, helpers ni features no pedidos.
8. **No pelear con el usuario** — si el usuario dice "hazlo así", hacerlo. Mencionar concern en 1 oración si aplica, luego proceder.
9. **Leer solo lo necesario** — usar offset/limit. Read directo si se conoce la ruta. No hacer Glob+Grep+Read cuando Read basta.
10. **No narrar el plan** — no describir lo que se va a hacer. Solo hacerlo. El usuario ve los tool calls.
11. **Paralelizar tool calls** — leer múltiples archivos independientes en un solo mensaje.
12. **No duplicar código en la respuesta** — si se editó un archivo, no copiar el resultado en texto. El usuario lo ve en el diff.
13. **No usar Agent cuando Grep/Read basta** — Agent solo para búsquedas amplias o tareas complejas.
14. **No hacer commit**


## Reglas para sistemas embebidos

1. Framework: arduino.
2. IDE: VSC con plugin platformio.
3. Separación de los módulos en archivos .cpp y .h construidos como clases.
4. En la carpeta include se escribirán los .h
5. En la carpeta src se escribirán los .cpp
6. Se deberán usar los dos nucleos del S3, implementando rtos. El nucleo 1 se usará para las mediciones, mientras que el nucleo 0 se encargará de manejar la wifi y el webserver.
7. El firmware que se escriba debe ser escalable.
8. Los comentarios en el firmware se harán en inglés.
9. Se deben observar buenas prácticas de programación de sistemas embebidos, priorizando la eficiencia.
10. Utilizar siempre NVS como almacenamiento de variables persistentes y configuraciones.
11. Implementar un procesador de comandos get/set y otros a definir para configurar/persistir variables. Este preocesador será independiente de la capa física que transporte el comando (esta variará segun la infraestrucura de comunicación: serial, mqtt, http, etc), pudiendo coexistir en el mismo proyecto dos o más capas. La capa física serial para procesamiento de comando estará siempre.
12. Las credenciales de comunicaciones (wifi, claves de lorawan, etc), estarán persistidas en el NVS con valores por defecto harcodeados con macros. Se implementarán comandos get/set para configurarlas.
13. Siempre compilar al terminar un requerimiento.
14. Sólo cuando te escriba 'fin de sesion', actualizar el README.md e incluir la guia de uso del firmware y el archivo resumen.md con la memoria de lo que se hizo para retomar en sesiones futuras.
15. Todo README.md u otro archivo .md que se escriba debe incluir una tabla de contenidos (TOC) al inicio, con links a cada sección. Salvo las instrucciones que te doy para que ejecutes
