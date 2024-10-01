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

### Flujo Básico de Comunicación

1. El **cliente DHCP** envía una solicitud (`DHCPREQUEST`) desde el puerto 68 al servidor en el puerto 67.
2. El **servidor DHCP** recibe la solicitud y responde con una confirmación (`DHCPACK`).
3. El cliente muestra el mensaje de confirmación recibido.