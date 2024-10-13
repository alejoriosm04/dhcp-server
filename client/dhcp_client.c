#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define MAX_RETRIES 3

int stop_renewal = 0;  // Variable global para detener la renovación cuando se presiona Enter

// Estructura para pasar parámetros a la función del hilo de renovación
typedef struct {
    int udp_socket;
    struct sockaddr_in* server_addr;
    char* assigned_ip;
    const char* mac_address;
    long lease_time;
} lease_params_t;

// Función para generar una dirección MAC aleatoria (simulando diferentes clientes)
void generate_random_mac(char* mac) {
    srand(time(NULL) + getpid());
    snprintf(mac, 18, "00:%02x:%02x:%02x:%02x:%02x",
             rand() % 256, rand() % 256, rand() % 256,
             rand() % 256, rand() % 256);
}

// Función para enviar DHCPRELEASE al servidor
void send_dhcp_release(int udp_socket, struct sockaddr_in* server_addr, const char* assigned_ip, const char* mac_address) {
    char release_message[BUFFER_SIZE];
    snprintf(release_message, BUFFER_SIZE, "DHCPRELEASE: IP=%s; MAC %s", assigned_ip, mac_address);

    if (sendto(udp_socket, release_message, strlen(release_message) + 1, 0,
               (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("No se pudo enviar el mensaje DHCPRELEASE");
    } else {
        printf("\n---- IP Liberada ----\n");
        printf("IP %s liberada correctamente.\n", assigned_ip);
        printf("----------------------\n");
    }
}

// Solicitar renovación del lease
void renew_lease(int udp_socket, struct sockaddr_in* server_addr, char* assigned_ip, const char* mac_address) {
    char request_message[BUFFER_SIZE];
    snprintf(request_message, BUFFER_SIZE, "DHCPREQUEST: IP=%s; MAC %s", assigned_ip, mac_address);

    if (sendto(udp_socket, request_message, strlen(request_message) + 1, 0,
               (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("No se pudo enviar el mensaje DHCPREQUEST para renovación");
    } else {
        printf("\n---- Renovando Lease ----\n");
        printf("Solicitando renovación para IP: %s\n", assigned_ip);

        // Recibir confirmación del servidor (DHCPACK)
        char buffer[BUFFER_SIZE];
        socklen_t server_addr_len = sizeof(*server_addr);
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)server_addr, &server_addr_len);
        if (bytes_received > 0 && strstr(buffer, "DHCPACK")) {
            buffer[bytes_received] = '\0'; // Asegurar que el buffer es un string válido

            // Parsear el mensaje DHCPACK
            char subnet_mask[16];
            char default_gateway[16];
            char dns_server[16];
            long lease_time;

            int parsed_fields = sscanf(buffer,
                "DHCPACK: IP=%15[^;]; MASK=%15[^;]; GATEWAY=%15[^;]; DNS=%15[^;]; LEASE=%ld",
                assigned_ip, subnet_mask, default_gateway, dns_server, &lease_time);

            if (parsed_fields == 5) {
                printf("Renovación exitosa:\n");
                printf("IP Asignada: %s\n", assigned_ip);
                printf("Máscara de Subred: %s\n", subnet_mask);
                printf("Puerta de Enlace Predeterminada: %s\n", default_gateway);
                printf("Servidor DNS: %s\n", dns_server);
                printf("Duración del Lease: %ld segundos\n", lease_time);
                printf("------------------------\n");
            } else {
                printf("No se pudo parsear correctamente la confirmación de renovación del servidor.\n");
            }
        } else {
            printf("No se pudo recibir la confirmación DHCPACK de renovación.\n");
        }
    }
}

// Función para manejar la renovación automática del lease en un hilo
void* lease_renewal_loop(void* args) {
    lease_params_t* params = (lease_params_t*)args;
    while (!stop_renewal) {  // Mientras no se presione Enter
        long wait_time = params->lease_time / 2;  // Esperar el 50% del tiempo del lease
        printf("Esperando %ld segundos para la siguiente renovación...\n", wait_time);
        sleep(wait_time);

        // Si stop_renewal es 1, salir del bucle
        if (stop_renewal) break;

        // Renovar el lease
        renew_lease(params->udp_socket, params->server_addr, params->assigned_ip, params->mac_address);
    }

    return NULL;
}

// Función para escuchar Enter en otro hilo
void* listen_for_enter(void* arg) {
    (void)arg; 
    getchar();  // Espera a que el usuario presione Enter
    stop_renewal = 1;  // Cambia la variable para detener la renovación
    return NULL;
}

int main() {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    char mac_address[18];
    char assigned_ip[16] = ""; 

    // Generar una dirección MAC aleatoria
    generate_random_mac(mac_address);
    printf("\n---- Cliente DHCP ----\n");
    printf("MAC Address del cliente: %s\n", mac_address);
    printf("----------------------\n");

    // Crear socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket <= 0) {
        perror("No se pudo crear el socket");
        return EXIT_FAILURE;
    }

    // Habilitar el modo broadcast en el socket
    int broadcastEnable = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("No se pudo habilitar el modo broadcast");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    // Configuración del cliente
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(68);  
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind al puerto 68 (cliente DHCP)
    if (bind(udp_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("No se pudo enlazar al puerto 68");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    // Configuración para el servidor de broadcast
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);
    server_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); 

    // Manejo de reintentos para el mensaje DHCPDISCOVER
    int retries = 0;
    while (retries < MAX_RETRIES) {
        // Enviar mensaje DHCPDISCOVER al servidor mediante broadcast
        char discover_message[BUFFER_SIZE];
        snprintf(discover_message, BUFFER_SIZE, "DHCPDISCOVER: MAC %s Solicitud de configuración", mac_address);
        if (sendto(udp_socket, discover_message, strlen(discover_message) + 1, 0,
                   (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("No se pudo enviar el mensaje DHCPDISCOVER");
            close(udp_socket);
            return EXIT_FAILURE;
        }

        printf("\n---- Mensaje DHCPDISCOVER ----\n");
        printf("Mensaje DHCPDISCOVER enviado por broadcast al puerto 67\n");
        printf("-------------------------------\n");

        // Recibir respuesta del servidor
        socklen_t server_addr_len = sizeof(server_addr);
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&server_addr, &server_addr_len);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            if (strstr(buffer, "DHCPNOIP")) {
                printf("DHCPNOIP: El servidor DHCP informó que no hay direcciones IP disponibles.\n");
                close(udp_socket);
                return EXIT_FAILURE;
            } else if  (strstr(buffer, "DHCPOFFER")) {
                printf("\n---- Oferta Recibida ----\n");
                printf("Oferta del servidor DHCP: %s\n", buffer);
                printf("--------------------------\n");

                // Variables para almacenar la información
                char subnet_mask[16];
                char default_gateway[16];
                char dns_server[16];
                long lease_time;

                // Parsear el mensaje DHCPOFFER
                int parsed_fields = sscanf(buffer,
                    "DHCPOFFER: IP=%15[^;]; MASK=%15[^;]; GATEWAY=%15[^;]; DNS=%15[^;]; LEASE=%ld",
                    assigned_ip, subnet_mask, default_gateway, dns_server, &lease_time);

                if (parsed_fields == 5) {
                    printf("\n---- Información Recibida ----\n");
                    printf("IP Asignada: %s\n", assigned_ip);
                    printf("Máscara de Subred: %s\n", subnet_mask);
                    printf("Puerta de Enlace Predeterminada: %s\n", default_gateway);
                    printf("Servidor DNS: %s\n", dns_server);
                    printf("Duración del Lease: %ld segundos\n", lease_time);
                    printf("-------------------------------\n");

                    // Crear hilo para manejar la renovación automática del lease
                    lease_params_t params = {udp_socket, &server_addr, assigned_ip, mac_address, lease_time};
                    pthread_t renewal_thread;
                    pthread_create(&renewal_thread, NULL, lease_renewal_loop, (void*)&params);

                    // Crear hilo para escuchar el Enter
                    pthread_t enter_thread;
                    pthread_create(&enter_thread, NULL, listen_for_enter, NULL);

                    // Esperar a que el usuario presione Enter
                    pthread_join(enter_thread, NULL);

                    // Enviar DHCPRELEASE al servidor al terminar
                    send_dhcp_release(udp_socket, &server_addr, assigned_ip, mac_address);

                    // Cerrar el socket y terminar
                    close(udp_socket);
                    return EXIT_SUCCESS;
                } else {
                    printf("No se pudo parsear correctamente la oferta del servidor.\n");
                }
            } else {
                printf("Mensaje desconocido del servidor: %s\n", buffer);
            }
        } else {
            printf("No se pudo recibir la respuesta del servidor, reintentando... (%d/%d)\n", retries + 1, MAX_RETRIES);
            retries++;
        }
    }

    if (retries == MAX_RETRIES) {
        printf("No se pudo obtener una oferta del servidor después de %d intentos.\n", MAX_RETRIES);
        close(udp_socket);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
