#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define POOL_SIZE 100  // Ajusta según el tamaño máximo esperado del pool de direcciones
#define LOG_FILE "dhcp_server.log"

typedef struct {
    char ip[16];              // Dirección IP asignada
    char mac_address[18];     // Dirección MAC del cliente
    time_t lease_start;       // Tiempo de inicio del lease
    time_t lease_duration;    // Duración del lease en segundos
    int assigned;             // 0: libre, 1: asignada
} lease_record;

lease_record lease_table[POOL_SIZE];
char *ip_pool[POOL_SIZE];

// Función para escribir mensajes en el log
void log_message(const char* level, const char* message) {
    FILE* log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("No se pudo abrir el archivo de log");
        return;
    }

    time_t now = time(NULL);
    struct tm* time_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);
    
    fprintf(log_file, "[%s] %s: %s\n", time_str, level, message);
    fclose(log_file);
}

// Función para generar el rango de IPs
int generate_ip_pool(const char *ip_start, const char *ip_end) {
    struct in_addr start_addr, end_addr;
    if (inet_pton(AF_INET, ip_start, &start_addr) <= 0) {
        perror("Invalid start IP address");
        log_message("ERROR", "Dirección IP de inicio inválida.");
        return -1;
    }
    if (inet_pton(AF_INET, ip_end, &end_addr) <= 0) {
        perror("Invalid end IP address");
        log_message("ERROR", "Dirección IP de fin inválida.");
        return -1;
    }

    unsigned long start = ntohl(start_addr.s_addr);
    unsigned long end = ntohl(end_addr.s_addr);
    int count = 0;

    for (unsigned long ip = start; ip <= end && count < POOL_SIZE; ++ip) {
        struct in_addr addr;
        addr.s_addr = htonl(ip);
        ip_pool[count] = malloc(INET_ADDRSTRLEN);
        if (!ip_pool[count]) {
            perror("Memory allocation failed");
            log_message("ERROR", "Fallo de asignación de memoria al generar el pool de IPs.");
            return -1;
        }
        inet_ntop(AF_INET, &addr, ip_pool[count], INET_ADDRSTRLEN);
        lease_table[count].assigned = 0;
        strcpy(lease_table[count].ip, ip_pool[count]);
        count++;
    }

    return count;  // Retorna el número de direcciones generadas
}

// Función para liberar la memoria asignada al pool de IPs
void free_ip_pool(int size) {
    for (int i = 0; i < size; ++i) {
        free(ip_pool[i]);
    }
}

// Función para registrar un lease, ahora con la MAC correcta del cliente
void register_lease(const char* ip, const char* mac_address, time_t lease_duration) {
    for (int i = 0; i < POOL_SIZE; ++i) {
        if (strcmp(lease_table[i].ip, ip) == 0) {
            strcpy(lease_table[i].mac_address, mac_address);  // Registramos la MAC del cliente
            lease_table[i].lease_start = time(NULL);
            lease_table[i].lease_duration = lease_duration;
            lease_table[i].assigned = 1;

            char log_entry[BUFFER_SIZE];
            snprintf(log_entry, BUFFER_SIZE, "Lease registrado para la IP %s con MAC %s por %ld segundos", ip, mac_address, lease_duration);
            log_message("INFO", log_entry);

            printf("%s\n", log_entry);
            break;
        }
    }
}

// Función para renovar un lease
void renew_lease(const char* ip, const char* mac_address, time_t lease_duration) {
    int found = 0;
    for (int i = 0; i < POOL_SIZE; ++i) {
        if (strcmp(lease_table[i].ip, ip) == 0 && strcmp(lease_table[i].mac_address, mac_address) == 0) {
            lease_table[i].lease_start = time(NULL);
            lease_table[i].lease_duration = lease_duration;

            char log_entry[BUFFER_SIZE];
            snprintf(log_entry, BUFFER_SIZE, "Lease renovado para la IP %s con MAC %s por %ld segundos", ip, mac_address, lease_duration);
            log_message("INFO", log_entry);

            printf("%s\n", log_entry);
            found = 1;
            break;
        }
    }
    if (!found) {
        char log_entry[BUFFER_SIZE];
        snprintf(log_entry, BUFFER_SIZE, "Error al renovar lease: la IP %s no está asignada a la MAC %s", ip, mac_address);
        log_message("ERROR", log_entry);
        printf("%s\n", log_entry);
    }
}

// Función para liberar una IP
void release_ip(const char* ip) {
    for (int i = 0; i < POOL_SIZE; ++i) {
        if (strcmp(lease_table[i].ip, ip) == 0) {
            lease_table[i].assigned = 0;
            lease_table[i].lease_start = 0;
            lease_table[i].lease_duration = 0;
            memset(lease_table[i].mac_address, 0, sizeof(lease_table[i].mac_address));

            char log_entry[BUFFER_SIZE];
            snprintf(log_entry, BUFFER_SIZE, "IP %s liberada y disponible para nuevos clientes", ip);
            log_message("INFO", log_entry);

            printf("%s\n", log_entry);
            break;
        }
    }
}

// Verifica y libera leases expirados
void check_expired_leases() {
    time_t current_time = time(NULL);
    for (int i = 0; i < POOL_SIZE; ++i) {
        if (lease_table[i].assigned &&
            difftime(current_time, lease_table[i].lease_start) >= lease_table[i].lease_duration) {
            lease_table[i].assigned = 0;
            memset(lease_table[i].mac_address, 0, sizeof(lease_table[i].mac_address));

            char log_entry[BUFFER_SIZE];
            snprintf(log_entry, BUFFER_SIZE, "Lease expirado para la IP %s. Liberando la dirección.", lease_table[i].ip);
            log_message("INFO", log_entry);

            printf("%s\n", log_entry);
        }
    }
}

// Función para asignar una IP disponible
const char* assign_ip() {
    for (int i = 0; i < POOL_SIZE; ++i) {
        if (!lease_table[i].assigned) {
            return lease_table[i].ip;    // Retornar la dirección IP disponible
        }
    }
    return NULL; // No hay direcciones disponibles
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <IP inicio> <IP fin>\n", argv[0]);
        log_message("ERROR", "Uso incorrecto del servidor DHCP. Se requieren las IP de inicio y fin.");
        return EXIT_FAILURE;
    }

    int pool_size = generate_ip_pool(argv[1], argv[2]);
    if (pool_size < 0) {
        printf("Error al generar el pool de IPs.\n");
        log_message("ERROR", "Error al generar el pool de IPs.");
        return EXIT_FAILURE;
    }

    printf("Servidor DHCP inicializado con el rango de IPs de %s a %s\n", argv[1], argv[2]);

    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;

    // Configuración del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);  // Puerto DHCP para el servidor
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Crear socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket <= 0) {
        perror("Could not create a socket");
        log_message("ERROR", "No se pudo crear el socket UDP.");
        free_ip_pool(pool_size);
        return EXIT_FAILURE;
    }

    // Bind del socket al puerto 67
    if (bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Could not bind the socket");
        log_message("ERROR", "No se pudo enlazar el socket al puerto 67.");
        close(udp_socket);
        free_ip_pool(pool_size);
        return EXIT_FAILURE;
    }

    printf("Servidor DHCP escuchando en el puerto 67...\n");

    // Loop para recibir mensajes de clientes
    while (1) {
        check_expired_leases(); // Verificar y liberar leases expirados

        socklen_t client_addr_len = sizeof(client_addr);
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);

        if (bytes_received > 0) {
            printf("Mensaje recibido de %s:%d -- %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

            if (strstr(buffer, "DHCPDISCOVER")) {
                const char *offered_ip = assign_ip();
                if (offered_ip) {
                    // Extraer la dirección MAC del mensaje de DHCPDISCOVER
                    char* mac_start = strstr(buffer, "MAC ");
                    if (mac_start) {
                        char client_mac[18];
                        sscanf(mac_start + 4, "%17s", client_mac);

                        register_lease(offered_ip, client_mac, 3600); // Guardar la MAC correcta del cliente
                        char offer_message[BUFFER_SIZE];
                        snprintf(offer_message, BUFFER_SIZE, "DHCPOFFER: %s", offered_ip);
                        sendto(udp_socket, offer_message, strlen(offer_message) + 1, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                        printf("Oferta enviada: %s a %s:%d\n", offered_ip, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    } else {
                        printf("No se pudo extraer la MAC del cliente en DHCPDISCOVER.\n");
                        log_message("ERROR", "No se pudo extraer la MAC del cliente en DHCPDISCOVER.");
                    }
                } else {
                    printf("No hay direcciones IP disponibles para ofrecer.\n");
                    log_message("WARNING", "No hay direcciones IP disponibles para ofrecer a un cliente.");
                }

            } else if (strstr(buffer, "DHCPREQUEST")) {
                // Extraer la IP solicitada y la MAC del mensaje de DHCPREQUEST
                char requested_ip[16] = {0};
                char client_mac[18] = {0};

                const char* ip_start = strstr(buffer, "DHCPREQUEST: ");
                const char* mac_start = strstr(buffer, "MAC ");

                if (ip_start && mac_start) {
                    sscanf(ip_start + strlen("DHCPREQUEST: "), "%15s", requested_ip);
                    sscanf(mac_start + 4, "%17s", client_mac);

                    renew_lease(requested_ip, client_mac, 3600);

                    char ack_message[BUFFER_SIZE];
                    snprintf(ack_message, BUFFER_SIZE, "DHCPACK: %s por 3600 segundos", requested_ip);
                    sendto(udp_socket, ack_message, strlen(ack_message) + 1, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                    printf("Confirmación enviada: %s a %s:%d\n", ack_message, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                } else {
                    printf("No se pudo extraer la IP o la MAC del cliente en DHCPREQUEST.\n");
                    log_message("ERROR", "No se pudo extraer la IP o la MAC del cliente en DHCPREQUEST.");
                }
            } else if (strstr(buffer, "DHCPRELEASE")) {
                const char *released_ip = buffer + strlen("DHCPRELEASE: ");
                release_ip(released_ip);
                printf("IP liberada: %s\n", released_ip);
            }
        } else {
            perror("No se pudo recibir el mensaje");
            log_message("ERROR", "No se pudo recibir el mensaje del cliente.");
        }
    }

    free_ip_pool(pool_size);
    close(udp_socket);

    return EXIT_SUCCESS;
}