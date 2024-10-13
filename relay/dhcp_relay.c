// relay/dhcp_relay.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>  // Inclusión necesaria para SO_REUSEPORT

#define SERVER_PORT 67
#define CLIENT_PORT 68
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in relay_addr, client_addr, server_addr;
    char buffer[BUFFER_SIZE];

    // Crear socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    // Habilitar reutilización de la dirección y puerto
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("No se pudo establecer opciones del socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del relay (escuchar en todas las interfaces)
    memset(&relay_addr, 0, sizeof(relay_addr));
    relay_addr.sin_family = AF_INET;
    relay_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    relay_addr.sin_port = htons(SERVER_PORT);

    // Enlazar el socket al puerto 67
    if (bind(sockfd, (struct sockaddr *)&relay_addr, sizeof(relay_addr)) < 0) {
        perror("No se pudo enlazar el socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor DHCP
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    // Reemplaza con la IP del servidor DHCP en la subred B
    server_addr.sin_addr.s_addr = inet_addr("192.168.2.2");
    server_addr.sin_port = htons(SERVER_PORT);

    printf("DHCP Relay iniciado y escuchando en el puerto %d...\n", SERVER_PORT);

    while (1) {
        socklen_t len = sizeof(client_addr);
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
        if (n < 0) {
            perror("Error al recibir datos");
            continue;
        }

        // Mostrar mensaje recibido
        buffer[n] = '\0';
        printf("Mensaje recibido de %s:%d -- %s\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

        // Reenviar el mensaje al servidor DHCP
        if (sendto(sockfd, buffer, n, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error al reenviar al servidor");
        } else {
            printf("Mensaje reenviado al servidor DHCP %s:%d\n",
                   inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
        }

        // Esperar respuesta del servidor DHCP
        n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);
        if (n < 0) {
            perror("Error al recibir datos del servidor DHCP");
            continue;
        }

        // Reenviar la respuesta al cliente original
        if (sendto(sockfd, buffer, n, 0, (struct sockaddr *)&client_addr, len) < 0) {
            perror("Error al reenviar al cliente");
        } else {
            printf("Respuesta reenviada al cliente %s:%d\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }
    }

    close(sockfd);
    return 0;
}
