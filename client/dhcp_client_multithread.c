#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>  // Añadido para multithreading

#define BUFFER_SIZE 1024
#define CLIENTMULTI_LOG_FILE "client/dhcp_client_multithread.log"

#define MAX_RETRIES 3
#define NUM_CLIENTS 5  // Número de clientes a simular

// Función para generar una dirección MAC aleatoria (simulando diferentes clientes)
void generate_random_mac(char* mac) {
    srand(time(NULL) + getpid() + (unsigned long)pthread_self());  // Mayor aleatoriedad
    snprintf(mac, 18, "00:%02x:%02x:%02x:%02x:%02x",
             rand() % 256, rand() % 256, rand() % 256,
             rand() % 256, rand() % 256);
}

void log_message(const char* level, const char* message, const char* ip, const char* mac) {
    FILE* log_file = fopen(CLIENTMULTI_LOG_FILE, "a");
    if (log_file == NULL) {
        perror("No se pudo abrir el archivo de log del cliente");
        return;
    }

    time_t now = time(NULL);
    struct tm* time_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    fprintf(log_file, "[%s] %s: %s | IP: %s | MAC: %s\n", time_str, level, message, ip, mac);
    fclose(log_file);
}

// Función que simula un cliente DHCP
void* simulate_client(void* arg) {
    (void)arg;  // Para evitar el warning de parámetro sin usar

    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr;
    char mac_address[18];

    // Generar una dirección MAC aleatoria
    generate_random_mac(mac_address);
    printf("MAC Address del cliente: %s\n", mac_address);
    log_message("INFO", "Cliente iniciado", "N/A", mac_address);

    // Crear socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket <= 0) {
        perror("No se pudo crear el socket");
        log_message("ERROR", "No se pudo crear el socket", "N/A", mac_address);
        pthread_exit(NULL);
    }

    // Habilitar el modo broadcast en el socket
    int broadcastEnable = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("No se pudo habilitar el modo broadcast");
        log_message("ERROR", "No se pudo habilitar el modo broadcast", "N/A", mac_address);
        close(udp_socket);
        pthread_exit(NULL);
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
            log_message("ERROR", "No se pudo enviar el mensaje DHCPDISCOVER", "N/A", mac_address);
            close(udp_socket);
            pthread_exit(NULL);
        }

        printf("Mensaje DHCPDISCOVER enviado por broadcast al puerto 67\n");
        log_message("INFO", "Mensaje DHCPDISCOVER enviado", "N/A", mac_address);

        // Recibir oferta del servidor (DHCPOFFER)
        struct sockaddr_in recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&recv_addr, &recv_addr_len);

        if (bytes_received > 0 && strstr(buffer, "DHCPOFFER")) {
            buffer[bytes_received] = '\0'; // Asegurar que el buffer es un string válido
            printf("Oferta recibida del servidor DHCP: %s\n", buffer);
            log_message("INFO", "Oferta recibida del servidor DHCP", "N/A", mac_address);

            // Variables para almacenar la información
            char assigned_ip[16];
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
                log_message("INFO", "Información recibida del servidor DHCP", assigned_ip, mac_address);
            } else {
                printf("No se pudo parsear correctamente la oferta del servidor.\n");
                log_message("ERROR", "No se pudo parsear correctamente la oferta del servidor", "N/A", mac_address);
                retries++;
                continue; // Intentar nuevamente
            }

            // Enviar mensaje DHCPREQUEST al servidor para solicitar la IP ofrecida
            char request_message[BUFFER_SIZE];
            snprintf(request_message, BUFFER_SIZE, "DHCPREQUEST: IP=%s; MAC %s", assigned_ip, mac_address);
            if (sendto(udp_socket, request_message, strlen(request_message) + 1, 0,
                       (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("No se pudo enviar el mensaje DHCPREQUEST");
                log_message("ERROR", "No se pudo enviar el mensaje DHCPREQUEST", assigned_ip, mac_address);
                close(udp_socket);
                pthread_exit(NULL);
            }

            printf("Mensaje DHCPREQUEST enviado al servidor DHCP solicitando IP %s\n", assigned_ip);
            log_message("INFO", "Mensaje DHCPREQUEST enviado", assigned_ip, mac_address);

            // Recibir confirmación del servidor (DHCPACK)
            bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&recv_addr, &recv_addr_len);
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
                    log_message("INFO", "Confirmación recibida del servidor DHCP", assigned_ip, mac_address);
                } else {
                    printf("No se pudo parsear correctamente la confirmación del servidor.\n");
                    log_message("ERROR", "No se pudo parsear correctamente la confirmación del servidor", assigned_ip, mac_address);
                }
                break;
            } else {
                printf("No se pudo recibir la confirmación DHCPACK, reintentando... (%d/%d)\n", retries + 1, MAX_RETRIES);
                log_message("ERROR", "No se pudo recibir la confirmación DHCPACK", assigned_ip, mac_address);
                retries++;
            }
        } else {
            printf("No se pudo recibir la oferta del servidor, reintentando... (%d/%d)\n", retries + 1, MAX_RETRIES);
            log_message("ERROR", "No se pudo recibir la oferta del servidor", "N/A", mac_address);
            retries++;
        }
    }

    if (retries == MAX_RETRIES) {
        printf("No se pudo obtener una oferta del servidor después de %d intentos.\n", MAX_RETRIES);
        log_message("ERROR", "No se pudo obtener una oferta del servidor", "N/A", mac_address);
    }

    close(udp_socket);
    log_message("INFO", "Cliente terminado", "N/A", mac_address);
    pthread_exit(NULL);
}

int main() {

    FILE* log_file = fopen(CLIENTMULTI_LOG_FILE, "w");
    if (log_file != NULL) {
        fclose(log_file);
    }
    pthread_t clients[NUM_CLIENTS];

    // Crear múltiples hilos para simular varios clientes
    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (pthread_create(&clients[i], NULL, simulate_client, NULL) != 0) {
            perror("No se pudo crear el hilo del cliente");
        }
    }

    // Esperar a que todos los hilos terminen
    for (int i = 0; i < NUM_CLIENTS; i++) {
        pthread_join(clients[i], NULL);
    }

    return EXIT_SUCCESS;
}
