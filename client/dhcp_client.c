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
    srand(time(NULL));
    snprintf(mac, 18, "00:%02x:%02x:%02x:%02x:%02x",
             rand() % 256, rand() % 256, rand() % 256,
             rand() % 256, rand() % 256);
}

int main() {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    char mac_address[18];

    // Generar una dirección MAC aleatoria
    generate_random_mac(mac_address);
    printf("MAC Address del cliente: %s\n", mac_address);

    // Crear socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket <= 0) {
        perror("Could not create a socket");
        return EXIT_FAILURE;
    }

    // Habilitar el modo broadcast en el socket
    int broadcastEnable = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("Could not enable broadcast mode");
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
        perror("Could not bind to port 68");
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
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0,
                                      (struct sockaddr *)&server_addr, &server_addr_len);

        if (bytes_received > 0 && strstr(buffer, "DHCPOFFER")) {
            printf("Oferta recibida del servidor DHCP: %s\n", buffer);

            // Enviar mensaje DHCPREQUEST al servidor para solicitar la IP ofrecida
            char request_message[BUFFER_SIZE];
            snprintf(request_message, BUFFER_SIZE, "DHCPREQUEST: MAC %s Solicitud de la IP ofrecida", mac_address);
            if (sendto(udp_socket, request_message, strlen(request_message) + 1, 0,
                       (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("No se pudo enviar el mensaje DHCPREQUEST");
                close(udp_socket);
                return EXIT_FAILURE;
            }

            printf("Mensaje DHCPREQUEST enviado al servidor DHCP\n");

            // Recibir confirmación del servidor (DHCPACK)
            bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0,
                                      (struct sockaddr *)&server_addr, &server_addr_len);
            if (bytes_received > 0 && strstr(buffer, "DHCPACK")) {
                // Extraer la duración del lease de la respuesta (simulado)
                char* lease_duration_str = strstr(buffer, "por");
                if (lease_duration_str != NULL) {
                    printf("Confirmación recibida del servidor DHCP: %s\n", buffer);
                    printf("Duración del lease: %s\n", lease_duration_str + 4);
                } else {
                    printf("Confirmación recibida del servidor DHCP, pero no se pudo determinar la duración del lease.\n");
                }
                break;
            } else {
                perror("No se pudo recibir la confirmación DHCPACK");
            }
        } else {
            printf("No se pudo recibir la oferta del servidor, reintentando... (%d/%d)\n", retries + 1, MAX_RETRIES);
            retries++;
        }
    }

    if (retries == MAX_RETRIES) {
        printf("No se pudo obtener una oferta del servidor después de %d intentos.\n", MAX_RETRIES);
    }

    close(udp_socket);
    return EXIT_SUCCESS;
}