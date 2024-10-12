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

   El cliente se conectará al servidor en `127.0.0.1` (localhost) y solicitará una configuración de red.

5. **Limpiar los archivos generados**:
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
    ssudo docker run -it --name dhcp_client1 --net dhcp_net --cap-add=NET_ADMIN dhcp_client
    ```

    **Terminal2**
    ```bash
    ssudo docker run -it --name dhcp_client1 --net dhcp_net --cap-add=NET_ADMIN dhcp_client
    ```

---

### Flujo Básico de Comunicación

1. El **cliente DHCP** envía una solicitud (`DHCPREQUEST`) desde el puerto 68 al servidor en el puerto 67.
2. El **servidor DHCP** recibe la solicitud y responde con una confirmación (`DHCPACK`).
3. El cliente muestra el mensaje de confirmación recibido.