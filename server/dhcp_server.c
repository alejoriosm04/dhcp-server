#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int server_port = atoi(argv[1]);
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;

    // Configuración del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Crear socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket <= 0) {
        perror("Could not create a socket");
        return EXIT_FAILURE;
    }

    // Bind del socket
    if (bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Could not bind the socket");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    printf("Servidor escuchando en el puerto %d...\n", server_port);

    // Loop para recibir mensajes de clientes
    while (1) {
        socklen_t client_addr_len = sizeof(client_addr);
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);

        if (bytes_received > 0) {
            printf("Mensaje recibido de %s:%d -- %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

            // Enviar confirmación de vuelta al cliente
            const char *confirmation_message = "Mensaje recibido correctamente!";
            if (sendto(udp_socket, confirmation_message, strlen(confirmation_message) + 1, 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                perror("No se pudo enviar la confirmación");
            } else {
                printf("Confirmación enviada a %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            }
        } else {
            perror("No se pudo recibir el mensaje");
        }
    }

    close(udp_socket);
    return EXIT_SUCCESS;
}
