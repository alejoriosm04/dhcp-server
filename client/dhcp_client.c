#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define BUFFER_SIZE 1024

int check(int exp, const char *msg);

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;

    // Configuración del cliente
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);  // El cliente envía al puerto 67 del servidor
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // Crear socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket <= 0) {
        perror("Could not create a socket");
        return EXIT_FAILURE;
    }

    // Bind al puerto 68 (cliente DHCP)
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(68);  // Puerto DHCP del cliente
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Could not bind to port 68");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    // Enviar mensaje al servidor
    const char *message = "DHCPREQUEST: Solicitud de configuración";
    if (sendto(udp_socket, message, strlen(message) + 1, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("No se pudo enviar el mensaje");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    printf("Mensaje enviado al servidor DHCP en %s:%d\n", server_ip, 67);

    // Recibir confirmación del servidor
    socklen_t server_addr_len = sizeof(server_addr);
    int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &server_addr_len);

    if (bytes_received > 0) {
        printf("Confirmación recibida del servidor DHCP: %s\n", buffer);
    } else {
        perror("No se pudo recibir la confirmación");
    }

    close(udp_socket);

    return EXIT_SUCCESS;
}
