#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define MAX_RETRIES 3

// Función para generar una dirección MAC aleatoria (simulando diferentes clientes)
void generate_random_mac(char* mac) {
    srand(time(NULL) + getpid());  // Usamos getpid() para mayor aleatoriedad en caso de ejecutar varios clientes
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
        printf("Mensaje DHCPRELEASE enviado al servidor DHCP liberando IP %s\n", assigned_ip);
    }
}

int main() {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    char mac_address[18];
    char assigned_ip[16] = ""; // Inicializar la IP asignada

    // Generar una dirección MAC aleatoria
    generate_random_mac(mac_address);
    printf("MAC Address del cliente: %s\n", mac_address);

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
    client_addr.sin_port = htons(68);  // Puerto DHCP del cliente
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
    server_addr.sin_port = htons(67);  // Puerto DHCP del servidor
    server_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);  // Dirección de broadcast

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

        printf("Mensaje DHCPDISCOVER enviado por broadcast al puerto 67\n");

        // Recibir oferta del servidor (DHCPOFFER)
        socklen_t server_addr_len = sizeof(server_addr);
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&server_addr, &server_addr_len);

        if (bytes_received > 0 && strstr(buffer, "DHCPOFFER")) {
            buffer[bytes_received] = '\0'; // Asegurar que el buffer es un string válido
            printf("Oferta recibida del servidor DHCP: %s\n", buffer);

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
                printf("Información recibida:\n");
                printf("IP Asignada: %s\n", assigned_ip);
                printf("Máscara de Subred: %s\n", subnet_mask);
                printf("Puerta de Enlace Predeterminada: %s\n", default_gateway);
                printf("Servidor DNS: %s\n", dns_server);
                printf("Duración del Lease: %ld segundos\n", lease_time);
            } else {
                printf("No se pudo parsear correctamente la oferta del servidor.\n");
                retries++;
                continue; // Intentar nuevamente
            }

            // Enviar mensaje DHCPREQUEST al servidor para solicitar la IP ofrecida
            char request_message[BUFFER_SIZE];
            snprintf(request_message, BUFFER_SIZE, "DHCPREQUEST: IP=%s; MAC %s", assigned_ip, mac_address);
            if (sendto(udp_socket, request_message, strlen(request_message) + 1, 0,
                       (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("No se pudo enviar el mensaje DHCPREQUEST");
                close(udp_socket);
                return EXIT_FAILURE;
            }

            printf("Mensaje DHCPREQUEST enviado al servidor DHCP solicitando IP %s\n", assigned_ip);

            // Recibir confirmación del servidor (DHCPACK)
            bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&server_addr, &server_addr_len);
            if (bytes_received > 0 && strstr(buffer, "DHCPACK")) {
                buffer[bytes_received] = '\0'; // Asegurar que el buffer es un string válido

                // Parsear el mensaje DHCPACK
                parsed_fields = sscanf(buffer,
                    "DHCPACK: IP=%15[^;]; MASK=%15[^;]; GATEWAY=%15[^;]; DNS=%15[^;]; LEASE=%ld",
                    assigned_ip, subnet_mask, default_gateway, dns_server, &lease_time);

                if (parsed_fields == 5) {
                    printf("Confirmación recibida del servidor DHCP:\n");
                    printf("IP Asignada: %s\n", assigned_ip);
                    printf("Máscara de Subred: %s\n", subnet_mask);
                    printf("Puerta de Enlace Predeterminada: %s\n", default_gateway);
                    printf("Servidor DNS: %s\n", dns_server);
                    printf("Duración del Lease: %ld segundos\n", lease_time);
                } else {
                    printf("No se pudo parsear correctamente la confirmación del servidor.\n");
                }
                break;
            } else {
                printf("No se pudo recibir la confirmación DHCPACK, reintentando... (%d/%d)\n", retries + 1, MAX_RETRIES);
                retries++;
            }
        } else {
            printf("No se pudo recibir la oferta del servidor, reintentando... (%d/%d)\n", retries + 1, MAX_RETRIES);
            retries++;
        }
    }

    if (retries == MAX_RETRIES) {
        printf("No se pudo obtener una oferta del servidor después de %d intentos.\n", MAX_RETRIES);
        close(udp_socket);
        return EXIT_FAILURE;
    }

    // Mantener el cliente activo hasta que el usuario decida terminar
    printf("\nCliente DHCP activo. Presione Enter para liberar la IP y salir...\n");
    getchar();  // Espera a que el usuario presione Enter

    // Enviar DHCPRELEASE al servidor al terminar
    send_dhcp_release(udp_socket, &server_addr, assigned_ip, mac_address);

    close(udp_socket);
    return EXIT_SUCCESS;
}
