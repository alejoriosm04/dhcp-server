## DHCP Server

---

### Descripción

Este proyecto es una simulación básica del protocolo **DHCP** (Dynamic Host Configuration Protocol) utilizando **sockets UDP en C**. El objetivo es implementar la comunicación entre un **servidor** que actúa como un servidor DHCP y un **cliente de prueba** que solicita configuraciones de red.

El servidor escucha en el puerto **67** (puerto estándar para DHCP) y el cliente envía solicitudes desde el puerto **68**. Ambos programas intercambian mensajes simulando las fases básicas del protocolo DHCP.

---

### Estructura del Proyecto

```
/proyecto-dhcp
    /servidor
        server.c        # Código del servidor DHCP
    /cliente
        client.c        # Código del cliente DHCP
    Makefile            # Archivo para compilar y ejecutar el proyecto
```

---

### Requisitos

- **Compilador C**: Necesitas `gcc` o un compilador C equivalente.
- **Permisos de superusuario**: Los puertos **67** y **68** son menores a 1024, por lo que requieren ser ejecutados con permisos de superusuario.

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

   El cliente se conectará al servidor y solicitará una configuración de red.

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

    Ejecuta el siguiente comando en la terminal, en el directorio donde se encuentra el Dockerfile.client:

    En Ubuntu
    ```bash
    sudo docker build -t dhcp_client -f Dockerfile.client .
    ```
3. **Construir la Imagen del Servidor**:

    Ejecuta el siguiente comando en la terminal, en el directorio donde se encuentra el Dockerfile.server:

    En Ubuntu
    ```bash
    sudo docker build -t dhcp_server -f Dockerfile.server .
    ```

4. **Crear una Red Docker Personalizada**:

    Creamos una red Docker tipo bridge para que los contenedores puedan comunicarse entre sí.

    En Ubuntu
    ```bash
    sudo docker network create --subnet=192.168.1.0/24 dhcp_net
    ```

5. **Ejecutar el Contenedor del Servidor DHCP**:

    Inicia el servidor DHCP en un contenedor conectado a la red dhcp_net.

    En Ubuntu
    ```bash
    sudo docker run -it --name dhcp_server --net dhcp_net --ip 192.168.1.2 --cap-add=NET_ADMIN dhcp_server
    ```

6. **Ejecutar el Contenedor de los clientes DHCP**:

    Inicia tantos clientes como desees en diferentes terminales, cada uno en su propio contenedor.

    En Ubuntu

    **Terminal1**
    ```bash
    udo docker run -it --name dhcp_client1 --net dhcp_net --cap-add=NET_ADMIN dhcp_client
    ```

    **Terminal2**
    ```bash
    sudo docker run -it --name dhcp_client2 --net dhcp_net --cap-add=NET_ADMIN dhcp_client
    ```

---

### Guia para probar multithreads

1. **Realizar los pasos del 1 al 3 de la guia para utilizar docker**:
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

    (Ajusta el script "stop_dhcp.sh", de acuerdo a lo que quieras seguir ejecutando o eliminando)
    
---

### Flujo Básico de Comunicación

1. El **cliente DHCP** envía una solicitud (`DHCPREQUEST`) desde el puerto 68 al servidor en el puerto 67.
2. El **servidor DHCP** recibe la solicitud y responde con una confirmación (`DHCPACK`).
3. El cliente muestra el mensaje de confirmación recibido.