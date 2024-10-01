#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define BUFFER_SIZE 1024

int check(int exp, const char *msg);

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr;

    // Configuración del cliente
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // Crear socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket <= 0) {
        perror("Could not create a socket");
        return EXIT_FAILURE;
    }

    // Enviar mensaje al servidor
    const char *message = "Hola, servidor!";
    if (sendto(udp_socket, message, strlen(message) + 1, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("No se pudo enviar el mensaje");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    printf("Mensaje enviado a %s:%d\n", server_ip, server_port);

    // Recibir confirmación del servidor
    socklen_t server_addr_len = sizeof(server_addr);
    int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &server_addr_len);

    if (bytes_received > 0) {
        printf("Confirmación recibida del servidor: %s\n", buffer);
    } else {
        perror("No se pudo recibir la confirmación");
    }

    close(udp_socket);

    return EXIT_SUCCESS;
}
