#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define POOL_SIZE 100
#define LOG_FILE "dhcp_server.log"

typedef struct {
    char ip[16];              // Dirección IP asignada
    char mac_address[18];     // Dirección MAC del cliente
    time_t lease_start;       // Tiempo de inicio del lease
    time_t lease_duration;    // Duración del lease en segundos
    int assigned;             // 0: libre, 1: asignada
    char subnet_mask[16];     // Máscara de subred
    char default_gateway[16]; // Puerta de enlace predeterminada
    char dns_server[16];      // Servidor DNS
} lease_record;

lease_record lease_table[POOL_SIZE];

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

// Función para leer los parámetros de red desde el archivo de configuración
int load_network_config(const char* filename, char* subnet_mask, char* default_gateway, char* dns_server, int* lease_time) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("No se pudo abrir el archivo de configuración");
        return -1;
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "SUBNET_MASK=", 12) == 0) {
            strcpy(subnet_mask, line + 12);
            subnet_mask[strcspn(subnet_mask, "\n")] = '\0'; // Eliminar el salto de línea
        } else if (strncmp(line, "DEFAULT_GATEWAY=", 16) == 0) {
            strcpy(default_gateway, line + 16);
            default_gateway[strcspn(default_gateway, "\n")] = '\0';
        } else if (strncmp(line, "DNS_SERVER=", 11) == 0) {
            strcpy(dns_server, line + 11);
            dns_server[strcspn(dns_server, "\n")] = '\0';
        } else if (strncmp(line, "LEASE_TIME=", 11) == 0) {
            *lease_time = atoi(line + 11);
        }
    }

    fclose(file);
    return 0;
}

// Función para generar el rango de IPs y asignar parámetros de red
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
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);

        lease_table[count].assigned = 0;
        strcpy(lease_table[count].ip, ip_str);
        lease_table[count].lease_start = 0;
        lease_table[count].lease_duration = 0;
        memset(lease_table[count].mac_address, 0, sizeof(lease_table[count].mac_address));

        count++;
    }

    return count;  // Retorna el número de direcciones generadas
}

// Función para registrar un lease
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

// Función para liberar una IP (actualizada para recibir la MAC y validar)
void release_ip(const char* ip, const char* mac_address) {
    for (int i = 0; i < POOL_SIZE; ++i) {
        if (strcmp(lease_table[i].ip, ip) == 0) {
            if (strcmp(lease_table[i].mac_address, mac_address) == 0) {
                lease_table[i].assigned = 0;
                lease_table[i].lease_start = 0;
                lease_table[i].lease_duration = 0;
                memset(lease_table[i].mac_address, 0, sizeof(lease_table[i].mac_address));

                char log_entry[BUFFER_SIZE];
                snprintf(log_entry, BUFFER_SIZE, "IP %s liberada y disponible para nuevos clientes", ip);
                log_message("INFO", log_entry);

                printf("%s\n", log_entry);
            } else {
                printf("La MAC %s no coincide con el registro para la IP %s\n", mac_address, ip);
                log_message("WARNING", "Intento de liberar una IP con una MAC que no coincide.");
            }
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
            lease_table[i].lease_start = 0;
            lease_table[i].lease_duration = 0;
            memset(lease_table[i].mac_address, 0, sizeof(lease_table[i].mac_address));

            char log_entry[BUFFER_SIZE];
            snprintf(log_entry, BUFFER_SIZE, "Lease expirado para la IP %s. Liberando la dirección.", lease_table[i].ip);
            log_message("INFO", log_entry);

            printf("%s\n", log_entry);
        }
    }
}

// Función para asignar una IP disponible
lease_record* assign_ip(const char* client_mac, const char* subnet_mask, const char* default_gateway, const char* dns_server) {
    for (int i = 0; i < POOL_SIZE; ++i) {
        if (!lease_table[i].assigned) {
            // Asignamos la MAC al registro
            strcpy(lease_table[i].mac_address, client_mac);

            // Asignar los parámetros de red
            strcpy(lease_table[i].subnet_mask, subnet_mask);
            strcpy(lease_table[i].default_gateway, default_gateway);
            strcpy(lease_table[i].dns_server, dns_server);

            return &lease_table[i];  // Retornar el registro del lease
        }
    }
    return NULL; // No hay direcciones disponibles
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <IP inicio> <IP fin> <archivo de configuración>\n", argv[0]);
        log_message("ERROR", "Uso incorrecto del servidor DHCP. Se requieren las IP de inicio, fin y el archivo de configuración.");
        return EXIT_FAILURE;
    }

    char subnet_mask[16];
    char default_gateway[16];
    char dns_server[16];
    int lease_time;

    // Cargar la configuración de red
    if (load_network_config(argv[3], subnet_mask, default_gateway, dns_server, &lease_time) != 0) {
        printf("Error al cargar la configuración de red.\n");
        log_message("ERROR", "Error al cargar la configuración de red.");
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
        return EXIT_FAILURE;
    }

    // Bind del socket al puerto 67
    if (bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Could not bind the socket");
        log_message("ERROR", "No se pudo enlazar el socket al puerto 67.");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    printf("Servidor DHCP escuchando en el puerto 67...\n");

    // Loop para recibir mensajes de clientes
    while (1) {
        check_expired_leases(); // Verificar y liberar leases expirados

        socklen_t client_addr_len = sizeof(client_addr);
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Asegurarse de que el buffer es un string válido
            printf("Mensaje recibido de %s:%d -- %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

            if (strstr(buffer, "DHCPDISCOVER")) {
                // Extraer la dirección MAC del mensaje de DHCPDISCOVER
                char* mac_start = strstr(buffer, "MAC ");
                if (mac_start) {
                    char client_mac[18];
                    sscanf(mac_start + 4, "%17s", client_mac);

                    // Asignar una IP disponible al cliente, pasando los parámetros de red
                    lease_record* lease = assign_ip(client_mac, subnet_mask, default_gateway, dns_server);
                    if (lease) {
                        // Registrar el lease con el tiempo de lease leído desde el archivo de configuración
                        register_lease(lease->ip, client_mac, lease_time);

                        // Construir el mensaje DHCPOFFER con la información adicional
                        char offer_message[BUFFER_SIZE];
                        snprintf(offer_message, BUFFER_SIZE,
                            "DHCPOFFER: IP=%s; MASK=%s; GATEWAY=%s; DNS=%s; LEASE=%ld",
                            lease->ip, lease->subnet_mask, lease->default_gateway, lease->dns_server, lease->lease_duration);

                        // Enviar el DHCPOFFER al cliente
                        sendto(udp_socket, offer_message, strlen(offer_message) + 1, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                        printf("Oferta enviada a %s:%d -- %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), offer_message);
                    } else {
                        printf("No hay direcciones IP disponibles para ofrecer.\n");
                        log_message("WARNING", "No hay direcciones IP disponibles para ofrecer a un cliente.");
                    }
                } else {
                    printf("No se pudo extraer la MAC del cliente en DHCPDISCOVER.\n");
                    log_message("ERROR", "No se pudo extraer la MAC del cliente en DHCPDISCOVER.");
                }
            } else if (strstr(buffer, "DHCPREQUEST")) {
                // Extraer la IP solicitada y la MAC del mensaje de DHCPREQUEST
                char requested_ip[16] = {0};
                char client_mac[18] = {0};

                int parsed_fields = sscanf(buffer, "DHCPREQUEST: IP=%15[^;]; MAC %17s", requested_ip, client_mac);

                if (parsed_fields == 2) {
                    // Verificar si la IP solicitada está asignada al cliente
                    int found = 0;
                    for (int i = 0; i < POOL_SIZE; ++i) {
                        if (lease_table[i].assigned &&
                            strcmp(lease_table[i].ip, requested_ip) == 0 &&
                            strcmp(lease_table[i].mac_address, client_mac) == 0) {
                            // Renovar el lease con el tiempo de lease leído desde el archivo de configuración
                            renew_lease(requested_ip, client_mac, lease_time);

                            // Construir el mensaje DHCPACK con la información adicional
                            char ack_message[BUFFER_SIZE];
                            snprintf(ack_message, BUFFER_SIZE,
                                "DHCPACK: IP=%s; MASK=%s; GATEWAY=%s; DNS=%s; LEASE=%ld",
                                lease_table[i].ip, lease_table[i].subnet_mask, lease_table[i].default_gateway,
                                lease_table[i].dns_server, lease_table[i].lease_duration);

                            // Enviar el DHCPACK al cliente
                            sendto(udp_socket, ack_message, strlen(ack_message) + 1, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                            printf("Confirmación enviada a %s:%d -- %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), ack_message);

                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        printf("La IP solicitada %s no está asignada a la MAC %s\n", requested_ip, client_mac);
                        log_message("WARNING", "La IP solicitada no está asignada al cliente.");

                        // Enviar DHCPNAK al cliente
                        char nak_message[BUFFER_SIZE];
                        snprintf(nak_message, BUFFER_SIZE, "DHCPNAK: Solicitud inválida para IP %s y MAC %s", requested_ip, client_mac);
                        sendto(udp_socket, nak_message, strlen(nak_message) + 1, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                        printf("DHCPNAK enviado a %s:%d -- %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), nak_message);
                    }
                } else {
                    printf("No se pudo extraer la IP o la MAC del cliente en DHCPREQUEST.\n");
                    log_message("ERROR", "No se pudo extraer la IP o la MAC del cliente en DHCPREQUEST.");
                }
            } else if (strstr(buffer, "DHCPRELEASE")) {
                // Manejo del mensaje DHCPRELEASE
                // Extraer la IP y MAC del cliente
                char released_ip[16] = {0};
                char client_mac[18] = {0};

                int parsed_fields = sscanf(buffer, "DHCPRELEASE: IP=%15[^;]; MAC %17s", released_ip, client_mac);

                if (parsed_fields == 2) {
                    // Liberar la IP
                    release_ip(released_ip, client_mac);
                    printf("IP liberada: %s por cliente %s\n", released_ip, client_mac);
                } else {
                    printf("No se pudo extraer la IP o la MAC del cliente en DHCPRELEASE.\n");
                    log_message("ERROR", "No se pudo extraer la IP o la MAC del cliente en DHCPRELEASE.");
                }
            } else {
                printf("Mensaje no reconocido: %s\n", buffer);
            }
        } else {
            perror("No se pudo recibir el mensaje");
            log_message("ERROR", "No se pudo recibir el mensaje del cliente.");
        }
    }

    close(udp_socket);

    return EXIT_SUCCESS;
}
