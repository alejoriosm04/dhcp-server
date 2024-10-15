// relay/dhcp_relay.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>  // Inclusión necesaria para SO_REUSEPORT
#include <time.h>

#define SERVER_PORT 67
#define CLIENT_PORT 68
#define BUFFER_SIZE 1024
#define RELAY_LOG_FILE "relay/dhcp_relay.log"

// Funcion que escribe mensajes en el archivo de log
void log_message(const char* level, const char* message) {
    // Open the log file in append mode
    FILE* log_file = fopen(RELAY_LOG_FILE, "a");
    if (log_file == NULL) {
        perror("No se pudo abrir el archivo de log del relay");
        return;
    }

    // Get the current time for the timestamp
    time_t now = time(NULL);
    struct tm* time_info = localtime(&now);
    char time_str[20];  // Buffer to hold the time string
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    // Write the log message with timestamp and level
    fprintf(log_file, "[%s] %s: %s\n", time_str, level, message);
    fclose(log_file);
}

int main() {
    FILE* log_file = fopen(RELAY_LOG_FILE, "w");
    if (log_file != NULL) {
        fclose(log_file);
    }
    int sockfd;
    struct sockaddr_in relay_addr, client_addr, server_addr;
    char buffer[BUFFER_SIZE];

    // Crear socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("No se pudo crear el socket");
        log_message("ERROR", "No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }
    log_message("INFO", "Socket UDP creado");

    // Habilitar reutilización de la dirección y puerto
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("No se pudo establecer opciones del socket");
        log_message("ERROR", "No se pudo establecer opciones del socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    log_message("INFO", "Opciones del socket establecidas");

    // Configurar la dirección del relay (escuchar en todas las interfaces)
    memset(&relay_addr, 0, sizeof(relay_addr));
    relay_addr.sin_family = AF_INET;
    relay_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    relay_addr.sin_port = htons(SERVER_PORT);

    // Enlazar el socket al puerto 67
    if (bind(sockfd, (struct sockaddr *)&relay_addr, sizeof(relay_addr)) < 0) {
        perror("No se pudo enlazar el socket");
        log_message("ERROR", "No se pudo enlazar el socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    log_message("INFO", "Socket enlazado al puerto 67");

    // Configurar la dirección del servidor DHCP
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    // Reemplaza con la IP del servidor DHCP en la subred B
    server_addr.sin_addr.s_addr = inet_addr("192.168.2.2");
    server_addr.sin_port = htons(SERVER_PORT);

    // Definir la dirección de red y la máscara de subred
    uint32_t netmask = inet_addr("255.255.255.0");
    uint32_t network_address = inet_addr("192.168.1.0");

    printf("DHCP Relay iniciado y escuchando en el puerto %d...\n", SERVER_PORT);
    log_message("INFO", "DHCP Relay iniciado y escuchando en el puerto 67");

    while (1) {
        socklen_t len = sizeof(client_addr);
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
        if (n < 0) {
            perror("Error al recibir datos");
            log_message("ERROR", "Error al recibir datos");
            continue;
        }

        // Comprobar si la dirección IP del cliente está dentro de la subred permitida
        if ((client_addr.sin_addr.s_addr & netmask) != (network_address)) {
            // El cliente no está en la subred permitida, ignorar el mensaje
            log_message("WARNING", "Mensaje de cliente fuera de la subred permitida ignorado");
            continue;
        }

        // Mostrar mensaje recibido
        buffer[n] = '\0';
        char log_buffer[BUFFER_SIZE + 100];
        snprintf(log_buffer, sizeof(log_buffer), "Mensaje recibido de %s:%d -- %s",
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);
        log_message("INFO", log_buffer);
        printf("%s\n", log_buffer);

        // Reenviar el mensaje al servidor DHCP
        if (sendto(sockfd, buffer, n, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error al reenviar al servidor");
            log_message("ERROR", "Error al reenviar al servidor");
        } else {
            snprintf(log_buffer, sizeof(log_buffer), "Mensaje reenviado al servidor DHCP %s:%d",
                     inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
            log_message("INFO", log_buffer);
            printf("%s\n", log_buffer);
        }

        // Esperar respuesta del servidor DHCP
        n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);
        if (n < 0) {
            perror("Error al recibir datos del servidor DHCP");
            log_message("ERROR", "Error al recibir datos del servidor DHCP");
            continue;
        }

        // Reenviar la respuesta al cliente original
        if (sendto(sockfd, buffer, n, 0, (struct sockaddr *)&client_addr, len) < 0) {
            perror("Error al reenviar al cliente");
            log_message("ERROR", "Error al reenviar al cliente");
        } else {
            snprintf(log_buffer, sizeof(log_buffer), "Respuesta reenviada al cliente %s:%d",
                     inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            log_message("INFO", log_buffer);
            printf("%s\n", log_buffer);
        }
    }

    close(sockfd);
    log_message("INFO", "Socket cerrado y programa terminado");
    return 0;
}
