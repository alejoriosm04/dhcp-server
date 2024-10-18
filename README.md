

## ST0255 Telemática: Proyecto 1 - Programación en Red

## Integrantes:
- Alejandro Ríos Muñoz
- Lina Sofía Ballesteros Merchán
- Juan Esteban García Galvis

### Índice

* [Introducción](#introducción)  
* [Desarrollo](#desarrollo)  
* [Entorno de Desarrollo](#entorno-de-desarrollo)  
* [Uso de la API Sockets Berkeley y elección de UDP](#uso-de-la-api-sockets-berkeley-y-elección-de-udp)
* [Diagrama](#diagrama)
* [Componentes](#componentes)  
    * [Implementación del Cliente DHCP](#implementación-del-cliente-dhcp)  
    * [Implementación del Servidor DHCP](#implementación-del-servidor-dhcp)  
    * [Implementación del DHCP Relay](#implementación-del-dhcp-relay)  
* [Logs](#logs)  
* [Requisitos](#requisitos)  
* [Instrucciones de Uso](#instrucciones-de-uso)  
* [Guía para Utilizar Docker](#guía-para-utilizar-docker)  
* [Guía para Probar Multithreads](#guía-para-probar-multithreads)  
* [Flujo Básico de Comunicación](#flujo-básico-de-comunicación)  
* [Aspectos Logrados y No Logrados](#aspectos-logrados-y-no-logrados)  
    * [Aspectos Logrados](#aspectos-logrados)  
    * [Aspectos No Logrados y a Considerar para Implementaciones Futuras](#aspectos-no-logrados-y-a-considerar-para-implementaciones-futuras)  
* [Conclusiones](#conclusiones)  
* [Referencias](#referencias)

## Introducción 

Este proyecto es una simulación del protocolo DHCP (Dynamic Host Configuration Protocol), diseñado para gestionar de manera dinámica la asignación de direcciones IP y otros parámetros de red a dispositivos conectados dentro de una red. Implementado en C utilizando sockets UDP, el proyecto refleja el funcionamiento real del protocolo, que utiliza UDP como medio de transporte para enviar y recibir mensajes sin establecer conexiones persistentes, lo que lo hace ideal para este tipo de aplicaciones de red.

El sistema implementa el ciclo de mensajes estándar del protocolo DHCP, que incluye **DHCPDISCOVER**, **DHCPOFFER**, **DHCPREQUEST**, y **DHCPACK**. Estos mensajes permiten a los clientes solicitar y recibir configuraciones de red del servidor. A su vez, en casos de errores, el servidor puede enviar un **DHCPNAK** indicando que la solicitud fue rechazada. Por el lado del cliente, se envía un mensaje **DHCPRELEASE** cuando ya no necesita su dirección IP, devolviéndola al pool del servidor. Esto garantiza que las direcciones IP sean gestionadas y reutilizadas de manera eficiente.

El Servidor DHCP maneja múltiples solicitudes concurrentes de clientes, asignando direcciones IP de manera dinámica a partir de un rango predefinido y estableciendo un leasepara cada dirección asignada. Cuando un cliente solicita una IP, el servidor selecciona una dirección disponible del pool y asigna un lease con un tiempo de concesión específico. Si el cliente solicita la renovación antes de que expire el lease, el servidor renueva la concesión. Además, el servidor maneja errores como la falta de direcciones IP disponibles, asegurando que se pueda gestionar adecuadamente la situación.

El Cliente DHCP, por su parte, puede ser ejecutado de manera concurrente utilizando Docker, permitiendo que múltiples instancias del cliente soliciten configuraciones de red al servidor. A cada cliente se le asigna una dirección IP única dentro del rango disponible, evitando conflictos. Si no quedan direcciones IP disponibles, el sistema informa adecuadamente a los nuevos clientes sobre la falta de disponibilidad.

Así mismo, se implementó un DHCP Relay que facilita la comunicación entre clientes y servidores en diferentes subredes. Este relay actúa como intermediario entre los dispositivos y el servidor DHCP, reenviando las solicitudes de los clientes al servidor, asegurando que los clientes ubicados fuera de la subred del servidor puedan ser atendidos. Tanto el DHCP Relay, como el Cliente y el Servidor cuentan con un sistema de logs implementado con el fin de monitorear, registrar y analizar el comportamiento del protocolo en tiempo de ejecución.

El proyecto fue desplegado y probado en AWS, lo que permitió simular un entorno de red distribuida y llevar a cabo pruebas exhaustivas del servidor, cliente y relay en diferentes escenarios de red.

En las siguientes secciones, se detallará el Desarrollodel proyecto, describiendo las decisiones de implementación y el entorno de desarrollo utilizado. También se abordarán los Aspectos Logrados y No Logrados, seguido de un análisis en las Conclusiones sobre el aprendizaje obtenido y posibles mejoras. Finalmente, se presentarán las Referencias que guiaron la correcta implementación del trabajo.

-------

### Desarrollo

Para la realización de este proyecto se trabajó en **WSL** (Windows Subsystem for Linux) utilizando **Ubuntu** como sistema operativo. El desarrollo fue organizado mediante un **repositorio en GitHub**, donde se gestionó el control de versiones y se asignaron tareas a cada miembro del equipo. El sistema fue diseñado para incluir un servidor DHCP, un cliente DHCP y un relay DHCP, asegurando que todos los componentes estuvieran integrados adecuadamente y permitieran la interacción en entornos locales y remotos.

#### Entorno de desarrollo
- **Sistema operativo**: Ubuntu en WSL.
- **Repositorio GitHub**: Utilizado para llevar el control de versiones, realizar seguimiento de las tareas y colaborar de forma eficiente.
- **Lenguaje de programación**: Todo el proyecto fue desarrollado en **C**, haciendo uso de la **API de Sockets Berkeley** para la comunicación en red.
- **Protocolos**: Se utilizó **UDP** (User Datagram Protocol) como protocolo de transporte, ya que el protocolo DHCP se basa en la transmisión de mensajes sin conexión persistente. Esto asegura una rápida transmisión de paquetes y tiempos de respuesta adecuados en redes dinámicas.
- **Docker**: Se utilizó Docker para crear contenedores de cliente, servidor y relay DHCP, facilitando el despliegue y las pruebas en entornos aislados y replicables.
- **AWS**: El proyecto fue desplegado y probado en **AWS**, lo que permitió evaluar el comportamiento del sistema en un entorno en la nube, simulando la interacción entre diferentes subredes.

#### Uso de la API Sockets Berkeley y elección de UDP
Para la implementación de este proyecto, se utilizó la **API de Sockets Berkeley**, una API ampliamente utilizada en desarrollo de redes. Esta API permite crear aplicaciones que puedan enviar y recibir datos a través de la red utilizando sockets. Un **socket** es una abstracción utilizada por aplicaciones para conectarse a la red y comunicarse con otras aplicaciones también conectadas. En el caso de DHCP, se optó por el uso de **sockets tipo Datagram (SOCK_DGRAM)**, los cuales utilizan UDP como protocolo de transporte. UDP es un protocolo sin conexión que es ideal para DHCP, ya que requiere una transmisión rápida de paquetes sin la sobrecarga de gestionar conexiones persistentes.

Esta elección está justificada por varias razones:

1. DHCP no necesita una conexión persistente para el intercambio de mensajes entre el cliente y el servidor, por lo que UDP es ideal al permitir la transmisión de mensajes sin gestionar el estado de la conexión.
2. UDP ofrece un método rápido de envío de mensajes, reduciendo la latencia y permitiendo tiempos de respuesta más cortos, cruciales para el proceso de asignación dinámica de IPs.
3. DHCP utiliza mensajes de broadcast, como **DHCPDISCOVER** y **DHCPOFFER**, lo que es facilitado por el uso de **UDP** y los **sockets tipo Datagram**.
4. El servidor DHCP puede manejar múltiples clientes de forma eficiente sin abrir conexiones individuales para cada cliente, permitiendo una mejor gestión de recursos.

### Diagrama

![telematica drawio](https://github.com/user-attachments/assets/e62f64d9-6876-4caa-8334-85d8e0258032)


### Componentes

#### Implementación del cliente DHCP
El cliente DHCP fue diseñado para enviar solicitudes de configuración de red utilizando los mensajes estándar del protocolo (**DHCPDISCOVER**, **DHCPOFFER**, **DHCPREQUEST**, **DHCPACK**). Cada cliente fue ejecutado en su propio contenedor mediante **Docker**, lo que permitió simular múltiples dispositivos en una red. Se utilizó un **socket tipo Datagram (SOCK_DGRAM)** basado en la API de **Sockets Berkeley** para enviar y recibir estos mensajes a través del protocolo UDP.

Para gestionar los reintentos en caso de falta de respuesta, se implementó un **algoritmo de Exponential Backoff**, que regula el tiempo de espera entre intentos consecutivos de solicitud DHCP. Asimismo, el cliente puede liberar su dirección IP con el mensaje **DHCPRELEASE**, y cuenta con la funcionalidad de renovación de leases.

#### Implementación del servidor DHCP
El servidor DHCP gestiona la asignación de direcciones IP a los clientes de forma dinámica a partir de un pool de direcciones, utilizando el **algoritmo de asignación First Fit**. Este algoritmo asigna la primera dirección IP disponible en el pool a los clientes que lo solicitan. El servidor también maneja solicitudes concurrentes de clientes mediante **threads**, permitiendo que cada solicitud sea procesada de forma independiente, maximizando la eficiencia del servidor y evitando cuellos de botella en la asignación de IPs.

El servidor también gestiona los mensajes de error como **DHCPNAK** cuando una solicitud no es válida, y libera direcciones IP mediante el mensaje **DHCPRELEASE** enviado por el cliente. Para cada asignación de IP, el servidor mantiene un registro de los leases y sus tiempos de expiración, lo que permite gestionar de forma eficiente la reasignación de direcciones IP liberadas o expiradas.

#### Implementación del DHCP Relay
El **DHCP Relay** fue implementado para permitir la comunicación entre clientes y servidores en diferentes subredes. Este componente actúa como un intermediario que reenvía las solicitudes de los clientes al servidor DHCP y luego retransmite las respuestas de vuelta a los clientes. Esta funcionalidad es esencial para escenarios donde el servidor DHCP no está directamente accesible por los clientes debido a la segmentación de la red.

---

El archivo `network_config.txt` contiene los parámetros esenciales de red que utiliza el servidor DHCP para asignar las configuraciones a los clientes. A continuación, se detallan los valores definidos en este archivo:

```
SUBNET_MASK=255.255.255.0
DEFAULT_GATEWAY=192.168.2.1
DNS_SERVER=8.8.8.8
LEASE_TIME=60
```

- **SUBNET_MASK**: Define la máscara de subred utilizada en la red.
- **DEFAULT_GATEWAY**: Especifica la puerta de enlace predeterminada que se asigna a los clientes.
- **DNS_SERVER**: Dirección del servidor DNS que se entrega a los clientes.
- **LEASE_TIME**: El tiempo en segundos que un cliente puede utilizar la dirección IP asignada antes de tener que renovarla.

#### Logs

El proyecto implementa un sistema de logs para monitorear la ejecución de cada componente, generando tres archivos de log: `dhcp_server.log` para el servidor, donde se registran eventos como la asignación y renovación de direcciones IP, la recepción de solicitudes y errores; `dhcp_relay.log` para el relay, que documenta la recepción y reenvío de mensajes entre los clientes y el servidor DHCP; y `dhcp_client.log` para el cliente, que registra eventos como las solicitudes de IP, la recepción de configuraciones de red y la liberación de direcciones. Estos logs facilitan la depuración y el seguimiento del estado del sistema en tiempo real. Es necesario crear estos archivos en caso de que no existan dentro de los directorios de cada componente para que se pueda ir sobrescribiendo el archivo

---

### Requisitos

- **Compilador C**: Necesitas `gcc` o un compilador C equivalente para compilar el código fuente del proyecto.
- **Docker**: Es necesario contar con Docker para ejecutar los contenedores del cliente, servidor y relay DHCP. Puedes instalar Docker utilizando las instrucciones proporcionadas en la sección de instrucciones de uso.
- **Permisos de superusuario**: Tanto el cliente como el servidor utilizan los puertos **67** y **68**, los cuales son menores a 1024, por lo que se requiere ejecutar los comandos con permisos de superusuario.

---

### Instrucciones de Uso

1. **Clonar el proyecto o copiar los archivos** a tu máquina local.

    ```bash
    git clone git@github.com:alejoriosm04/dhcp-server.git
    ```

2. **Compilar el proyecto** utilizando el archivo `Makefile`. Esto compilará tanto el servidor como el cliente:

    ```bash
    make
    ```

3. **Ejecutar el servidor**:
   El servidor necesita ejecutarse antes que el cliente. Utiliza el siguiente comando para iniciar el servidor (con permisos de superusuario, ya que usa el puerto 67):

    ```bash
    sudo make run-server
    ```

4. **Ejecutar el cliente**:
   Una vez que el servidor esté en funcionamiento, puedes iniciar el cliente con este comando. Asegúrate de usar permisos de superusuario para usar el puerto 68:

    ```bash
    sudo make run-client
    ```

5. **Ejecutar el cliente multithread**:
   Si deseas probar el cliente multithread, puedes usar el siguiente comando. Este cliente envía múltiples solicitudes al servidor en paralelo:

    ```bash
    sudo make run-client-multithread
    ```

6. **Limpiar los archivos generados**:
   Si deseas eliminar los archivos binarios generados por la compilación (ejecutables y archivos objeto), puedes usar el siguiente comando:

    ```bash
    make clean
    ```

---

### Guía para Utilizar Docker

1. **Instalar Docker**:

    En Ubuntu:
    ```bash
    sudo apt-get update
    sudo apt-get install -y docker.io
    sudo systemctl start docker
    sudo systemctl enable docker
    ```

2. **Construir la Imagen del Cliente**:

    Ejecuta el siguiente comando en la terminal, en el directorio donde se encuentra el `Dockerfile.client`:

    ```bash
    sudo docker build -t dhcp_client -f Dockerfile.client .
    ```

3. **Construir la Imagen del Servidor**:

    Ejecuta el siguiente comando en la terminal, en el directorio donde se encuentra el `Dockerfile.server`:

    ```bash
    sudo docker build -t dhcp_server -f Dockerfile.server .
    ```

4. **Crear una Red Docker Personalizada**:

    Crea una red Docker tipo bridge para que los contenedores puedan comunicarse entre sí.

    ```bash
    sudo docker network create --subnet=192.168.1.0/24 dhcp_net_a
    ```

5. **Configurar la subred adicional**:

    Para configurar una segunda subred, ejecuta lo siguiente:

    ```bash
    sudo docker network create --subnet=192.168.2.0/24 dhcp_net_b
    ```

6. **Construir la Imagen del DHCP Relay**:

    ```bash
    sudo docker build -t dhcp_relay -f Dockerfile.relay .
    ```

7. **Ejecutar el Contenedor del Relay**:

    Conecta el relay a las dos subredes:

    ```bash
    sudo docker run -it --name dhcp_relay --net dhcp_net_a --ip 192.168.1.2 --cap-add=NET_ADMIN dhcp_relay
    ```

8. **Conectar el Relay a la segunda subred**:

    Conéctalo también a la segunda subred:

    ```bash
    sudo docker network connect dhcp_net_b dhcp_relay --ip 192.168.2.3
    ```

9. **Verificar la configuración de red del Relay**:

    En otra terminal, ejecuta:

    ```bash
    sudo docker exec -it dhcp_relay /bin/bash
    ```

    Luego, usa el comando `ifconfig` para verificar que el contenedor esté conectado a ambas subredes.

    ```bash
    ifconfig
    ```

10. **Habilitar el reenvío de paquetes en el contenedor Relay**:

    En otra terminal, ejecuta el contenedor con privilegios y habilita el reenvío de paquetes:

    ```bash
    sudo docker run --privileged -it dhcp_relay bash
    ```

    Luego habilita el reenvío de paquetes:

    ```bash
    echo 1 > /proc/sys/net/ipv4/ip_forward
    ```

11. **Ejecutar el Contenedor del Servidor DHCP**:

    Ejecuta el servidor DHCP en la segunda subred:

    ```bash
    sudo docker run -it --name dhcp_server --net dhcp_net_b --ip 192.168.2.2 --cap-add=NET_ADMIN dhcp_server
    ```

12. **Ejecutar Clientes DHCP**:

    Puedes ejecutar múltiples clientes en diferentes terminales:

    **Terminal1**:
    ```bash
    sudo docker run -it --name dhcp_client1 --net dhcp_net_a --cap-add=NET_ADMIN dhcp_client
    ```

    **Terminal2**:
    ```bash
    sudo docker run -it --name dhcp_client2 --net dhcp_net_b --cap-add=NET_ADMIN dhcp_client
    ```

13. **Limpiar Contenedores**:

    Para eliminar los contenedores en caso de errores o conflictos, puedes usar los siguientes comandos:

    ```bash
    sudo docker ps -a
    sudo docker stop <ID_DEL_CONTENEDOR>
    sudo docker rm <ID_DEL_CONTENEDOR>
    ```

---

### Guía para probar multithreads

1. **Realizar los pasos del 1 al 3 de la guía para utilizar docker**:
    Esto se debe a que necesitamos el docker y sus configuraciones. 

2. **Instalar xterm para hacer las pruebas en diferentes terminales**:
    ```bash
    sudo apt-get install -y xterm
    ```

3. **Ejecutar el servidor y lo clientes**
    El archivo que se encuentra en el direcotrio "run_clients.sh" es un script el cual ejecuta el docker y todos los componentes necesarios para que funcione. Ahi mismo se puede cambiar la cantidad de clients que se quieren crear. 

    En Ubuntu
    ```bash
    sudo ./run_clients.sh
    ```
4. **Detener y eliminar los servicios**

    - Cada vez que se ejecuta el script "run_clients.sh" se abren nuevas ventanas que pueden ser molestas.  
    - Al ejecutar el script "run_clients.sh" se crea el servidor y los contenedores respectivos.

    Para solucionar ese problema se creo el script "stop_dhcp.sh" el cual elimina todo, lo cual podemos ejecutar el script "run_clients.sh" vacio.

    ```bash
    sudo ./stop_dhcp.sh
    ```
    
-----

### Flujo Básico de Comunicación

1. El cliente DHCP inicia el proceso enviando un mensaje de descubrimiento (`DHCPDISCOVER`) desde su puerto 68 en formato broadcast, buscando servidores DHCP disponibles en la red.
2. El servidor DHCP responde con una oferta de configuración de red (`DHCPOFFER`), indicando una dirección IP disponible y otros parámetros de red, como la máscara de subred y la puerta de enlace.
3. El cliente DHCP envía una solicitud de confirmación (`DHCPREQUEST`), aceptando la oferta recibida del servidor y solicitando la asignación de la dirección IP propuesta.
4. El servidor DHCP confirma la asignación enviando un mensaje de aceptación final (`DHCPACK`), estableciendo la dirección IP y los parámetros de red para el cliente.
5. El cliente DHCP recibe y aplica la configuración de red, mostrando el mensaje de confirmación recibido e iniciando su conexión en la red.

---
### Aspectos Logrados y No Logrados

#### Aspectos Logrados

1. **Cliente DHCP**:
   - **Solicitud de dirección IP (DHCPDISCOVER)**: El cliente DHCP es capaz de enviar un mensaje **DHCPDISCOVER** al servidor, solicitando una dirección IP.
   - **Recepción de configuración de red**: El cliente recibe y muestra la información proporcionada por el servidor, incluyendo dirección IP, máscara de red, puerta de enlace predeterminada y servidor DNS.
   - **Renovación de IP**: El cliente puede solicitar la renovación de su dirección IP cuando el lease está a punto de expirar. Esta funcionalidad fue implementada mediante un hilo que gestiona la renovación automáticamente a mitad del tiempo del lease.
   - **Liberación de IP (DHCPRELEASE)**: Al terminar la ejecución, el cliente libera correctamente su dirección IP, enviando un mensaje **DHCPRELEASE** al servidor.

2. **Servidor DHCP**:
   - **Recepción de solicitudes en red local o remota**: El servidor DHCP puede escuchar solicitudes **DHCPDISCOVER** tanto de clientes en la red local como de subredes remotas a través del **DHCP relay**.
   - **Asignación dinámica de IPs**: El servidor asigna direcciones IP disponibles de manera dinámica a los clientes que lo solicitan, utilizando el algoritmo de **First Fit** para la asignación de las IPs.
   - **Gestión de leases**: El servidor gestiona correctamente la concesión de direcciones IP, incluyendo la renovación y liberación de las mismas. El sistema de logs registra todas las asignaciones, así como los tiempos de arrendamiento.
   - **Soporte de concurrencia**: El servidor puede manejar múltiples solicitudes simultáneamente, utilizando **hilos (threads)** para gestionar cada petición de cliente de forma independiente.
   - **Mensajes estándar DHCP**: Se implementaron las fases y mensajes principales del proceso DHCP, incluyendo **DISCOVER**, **OFFER**, **REQUEST**, **ACK**, y **RELEASE**.
   - **Manejo de clientes en subredes remotas**: Gracias al **DHCP Relay**, el servidor puede atender solicitudes de clientes que se encuentran en diferentes subredes.
   - **Manejo de errores**: El servidor responde adecuadamente cuando no hay más direcciones IP disponibles, enviando un mensaje **DHCPNOIP** para informar a los clientes.

#### Aspectos No Logrados y a Considerar para Implementaciones Futuras

- **Manejo del mensaje DHCPDECLINE**: Aunque no se implementó el manejo del mensaje **DHCPDECLINE** (utilizado por los clientes para informar que una dirección IP no es válida), no fue un aspecto crítico para el correcto funcionamiento del sistema en los escenarios evaluados. Sin embargo, su implementación puede ser recomendable en versiones futuras para cubrir casos de rechazo de IPs, mejorando así la robustez y el manejo de excepciones del sistema.

### Conclusiones

- El desarrollo de este proyecto permitió implementar con éxito un sistema funcional basado en el protocolo DHCP, logrando cumplir con los objetivos planteados para el cliente, servidor y relay DHCP. El uso de tecnologías como la API de Sockets Berkeley y Docker facilitó la simulación de entornos de red complejos, permitiendo probar el sistema en diferentes escenarios, tanto en redes locales como en subredes remotas.

- Se logró desarrollar tanto el cliente como el servidor DHCP, asegurando la correcta asignación, renovación y liberación de direcciones IP. Además, se integró un DHCP Relay que permite la comunicación entre clientes y servidores en subredes diferentes, extendiendo la funcionalidad más allá de una red local.

- Se implementaron mecanismos para gestionar situaciones como la falta de direcciones IP disponibles o solicitudes concurrentes, asegurando que el servidor pueda responder adecuadamente a los clientes en situaciones de error. Este tipo de manejo es crucial para asegurar la robustez del sistema.

- Este proyecto no solo permitió profundizar en el funcionamiento del protocolo DHCP, sino también en la importancia de la concurrencia y la robustez en el desarrollo de aplicaciones en red. El uso de Docker como entorno de pruebas fue clave para garantizar una simulación realista y efectiva de diferentes escenarios de red. Como recomendación futura, se sugiere mejorar la implementación del manejo de errores y completar la gestión de mensajes adicionales como **DHCPDECLINE** para obtener un sistema aún más completo y robusto.

## Referencias

1. Huawei. (n.d.). *How a DHCP client renews its IP address lease*. Huawei Enterprise. Retrieved October 7, 2024, from https://support.huawei.com/enterprise/en/doc/EDOC1100034071/4b2f29de/how-a-dhcp-client-renews-its-ip-address-lease

2. IBM. (n.d.). *Configuring a native DHCP server and BOOTP/DHCP relay agent*. IBM Documentation. Retrieved October 10, 2024, from https://www.ibm.com/docs/nl/i/7.3?topic=dhcp-configuring-native-server-bootpdhcp-relay-agent

3. IBM. (n.d.). *DHCP leases*. IBM Documentation. Retrieved October 7, 2024, from https://www.ibm.com/docs/en/i/7.4?topic=concepts-leases

4. IBM. (n.d.). *Configuring and viewing a DHCP server*. IBM Documentation. Retrieved September 30, 2024, from https://www.ibm.com/docs/nl/i/7.3?topic=agent-configuring-viewing-dhcp-server

5. Palo Alto Networks. (n.d.). *DHCP options*. Palo Alto Networks Documentation. Retrieved October 2, 2024, from https://docs.paloaltonetworks.com/pan-os/10-1/pan-os-networking-admin/dhcp/dhcp-options

6. Droms, R. (1997). *Dynamic host configuration protocol*. IETF. Retrieved September 30, 2024, from https://datatracker.ietf.org/doc/html/rfc2131

7. Goyal, M., & Pandey, R. (2003). *Dynamic host configuration protocol (DHCP) server provisioning*. IETF. Retrieved October 7, 2024, from https://datatracker.ietf.org/doc/rfc3456/

8. IONOS. (n.d.). *¿Qué es el DHCP y cómo funciona?*. IONOS. Retrieved September 25, 2024, from https://www.ionos.com/es-us/digitalguide/servidores/configuracion/que-es-el-dhcp-y-como-funciona/

9. Aruba Networks. (n.d.). *DHCP relay*. Aruba Networks. Retrieved October 7, 2024, from https://www.arubanetworks.com/techdocs/AFC/650/Content/afc65olh/dhc-rel.htm#:~:text=DHCP%20Relay%20provides%20a%20way,to%20a%20provisioned%20DHCP%20server
