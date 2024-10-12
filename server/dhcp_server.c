// Archivo: dhcp_server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define POOL_SIZE 100
#define DEFAULT_LEASE_TIME 20  // Tiempo de lease predeterminado en segundos

// Estructura para almacenar los registros de lease
typedef struct {
    char ip[16];
    char mac_address[18];
    int assigned;
    time_t lease_start_time;
    long lease_duration;
    int is_new;  // Indica si el lease es nuevo
    char subnet_mask[16];
    char default_gateway[16];
    char dns_server[16];
} lease_record;

// Estructura para manejar las solicitudes de los clientes
typedef struct {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    int udp_socket;
} client_request;

// Variables globales
lease_record lease_table[POOL_SIZE];
pthread_mutex_t lease_table_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_running = 1;
int udp_socket;

// Prototipos de funciones
void generate_ip_pool(const char* start_ip, const char* end_ip);
void* handle_client(void* arg);
lease_record* assign_ip(const char* client_mac);
void release_ip(const char* ip, const char* client_mac);
void register_lease(lease_record* lease, const char* client_mac);
void renew_lease(lease_record* lease);
void check_expired_leases();
void log_message(const char* level, const char* message);
void signal_handler(int signal);

int main() {
    const char* start_ip = "192.168.1.10";
    const char* end_ip = "192.168.1.100";
    const char* subnet_mask = "255.255.255.0";
    const char* default_gateway = "192.168.1.1";
    const char* dns_server = "8.8.8.8";
    int server_port = 67;  // Puerto DHCP estándar

    // Generar el pool de direcciones IP
    generate_ip_pool(start_ip, end_ip);

    // Inicializar los valores adicionales en el lease_table
    for (int i = 0; i < POOL_SIZE; ++i) {
        strncpy(lease_table[i].subnet_mask, subnet_mask, sizeof(lease_table[i].subnet_mask) - 1);
        strncpy(lease_table[i].default_gateway, default_gateway, sizeof(lease_table[i].default_gateway) - 1);
        strncpy(lease_table[i].dns_server, dns_server, sizeof(lease_table[i].dns_server) - 1);
    }

    printf("Servidor DHCP inicializado con el rango de IPs de %s a %s\n", start_ip, end_ip);

    // Crear el socket UDP
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        perror("Error al crear el socket UDP");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);  // Puerto DHCP
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Enlazar el socket con la dirección del servidor
    if (bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al enlazar el socket UDP");
        close(udp_socket);
        exit(EXIT_FAILURE);
    }

    printf("Servidor DHCP escuchando en el puerto %d...\n", server_port);

    // Manejar señales para una terminación limpia
    signal(SIGINT, signal_handler);

    // Bucle principal del servidor
    while (server_running) {
        client_request* request = (client_request*)malloc(sizeof(client_request));
        request->client_addr_len = sizeof(request->client_addr);

        // Recibir mensaje del cliente
        ssize_t bytes_received = recvfrom(udp_socket, request->buffer, BUFFER_SIZE - 1, 0,
            (struct sockaddr *)&request->client_addr, &request->client_addr_len);

        if (bytes_received < 0) {
            perror("Error al recibir datos del cliente");
            free(request);
            continue;
        }

        request->buffer[bytes_received] = '\0';  // Asegurarse de que el buffer es una cadena
        request->udp_socket = udp_socket;

        printf("Mensaje recibido de %s:%d -- %s\n",
            inet_ntoa(request->client_addr.sin_addr),
            ntohs(request->client_addr.sin_port), request->buffer);

        // Crear un hilo para manejar la solicitud del cliente
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)request) != 0) {
            perror("Error al crear el hilo para manejar la solicitud del cliente");
            free(request);
            continue;
        }

        pthread_detach(thread_id);  // No necesitamos esperar a que el hilo termine

        // Verificar leases expirados
        check_expired_leases();
    }

    // Cerrar el socket UDP
    close(udp_socket);
    printf("Servidor DHCP terminado.\n");

    return 0;
}

// Función para generar el pool de direcciones IP
void generate_ip_pool(const char* start_ip, const char* end_ip) {
    struct in_addr start_addr, end_addr;
    inet_aton(start_ip, &start_addr);
    inet_aton(end_ip, &end_addr);

    unsigned long start = ntohl(start_addr.s_addr);
    unsigned long end = ntohl(end_addr.s_addr);

    int count = 0;
    for (unsigned long ip = start; ip <= end && count < POOL_SIZE; ++ip) {
        struct in_addr ip_addr;
        ip_addr.s_addr = htonl(ip);
        strncpy(lease_table[count].ip, inet_ntoa(ip_addr), sizeof(lease_table[count].ip) - 1);
        lease_table[count].assigned = 0;
        lease_table[count].lease_start_time = 0;
        lease_table[count].lease_duration = 0;
        lease_table[count].is_new = 0;
        count++;
    }
}

// Función para manejar las solicitudes de los clientes
void* handle_client(void* arg) {
    client_request* request = (client_request*)arg;
    char* buffer = request->buffer;
    struct sockaddr_in client_addr = request->client_addr;
    socklen_t client_addr_len = request->client_addr_len;
    int udp_socket = request->udp_socket;

    if (strstr(buffer, "DHCPDISCOVER")) {
        // Extraer la dirección MAC del mensaje de DHCPDISCOVER
        char* mac_start = strstr(buffer, "MAC ");
        if (mac_start) {
            char client_mac[18];
            sscanf(mac_start + 4, "%17s", client_mac);

            // Asignar una IP disponible al cliente
            lease_record* lease = assign_ip(client_mac);
            if (lease) {
                register_lease(lease, client_mac);

                // Construir el mensaje DHCPOFFER
                char offer_message[BUFFER_SIZE];
                snprintf(offer_message, BUFFER_SIZE,
                    "DHCPOFFER: IP=%s; MASK=%s; GATEWAY=%s; DNS=%s; LEASE=%ld",
                    lease->ip, lease->subnet_mask, lease->default_gateway,
                    lease->dns_server, lease->lease_duration);

                // Enviar el DHCPOFFER al cliente
                sendto(udp_socket, offer_message, strlen(offer_message) + 1, 0,
                    (struct sockaddr *)&client_addr, client_addr_len);
                printf("Oferta enviada a %s:%d -- %s\n",
                    inet_ntoa(client_addr.sin_addr),
                    ntohs(client_addr.sin_port), offer_message);
            } else {
                printf("No hay direcciones IP disponibles para ofrecer.\n");
                log_message("WARNING",
                    "No hay direcciones IP disponibles para ofrecer a un cliente.");
            }
        } else {
            printf("No se pudo extraer la MAC del cliente en DHCPDISCOVER.\n");
            log_message("ERROR",
                "No se pudo extraer la MAC del cliente en DHCPDISCOVER.");
        }
    } else if (strstr(buffer, "DHCPREQUEST")) {
        // Extraer la IP solicitada y la MAC del mensaje de DHCPREQUEST
        char requested_ip[16] = {0};
        char client_mac[18] = {0};

        int parsed_fields = sscanf(buffer,
            "DHCPREQUEST: IP=%15[^;]; MAC %17s", requested_ip, client_mac);

        if (parsed_fields == 2) {
            // Verificar si la IP solicitada está asignada al cliente
            lease_record* lease = NULL;
            pthread_mutex_lock(&lease_table_mutex);
            for (int i = 0; i < POOL_SIZE; ++i) {
                if (strcmp(lease_table[i].ip, requested_ip) == 0 &&
                    strcmp(lease_table[i].mac_address, client_mac) == 0) {
                    lease = &lease_table[i];

                    // Si el lease ha expirado, permitir que el cliente lo renueve
                    if (!lease->assigned) {
                        lease->assigned = 1;
                        lease->lease_start_time = time(NULL);
                        lease->lease_duration = DEFAULT_LEASE_TIME;
                        lease->is_new = 0;
                        printf("Lease renovado para la IP %s con MAC %s por %ld segundos\n",
                            lease->ip, lease->mac_address, lease->lease_duration);
                    } else {
                        // Renovar el lease
                        renew_lease(lease);
                    }

                    // Construir el mensaje DHCPACK
                    char ack_message[BUFFER_SIZE];
                    snprintf(ack_message, BUFFER_SIZE,
                        "DHCPACK: IP=%s; MASK=%s; GATEWAY=%s; DNS=%s; LEASE=%ld",
                        lease->ip, lease->subnet_mask, lease->default_gateway,
                        lease->dns_server, lease->lease_duration);

                    // Enviar el DHCPACK al cliente
                    sendto(udp_socket, ack_message,
                        strlen(ack_message) + 1, 0,
                        (struct sockaddr *)&client_addr, client_addr_len);
                    printf("Confirmación enviada a %s:%d -- %s\n",
                        inet_ntoa(client_addr.sin_addr),
                        ntohs(client_addr.sin_port), ack_message);

                    break;
                }
            }
            pthread_mutex_unlock(&lease_table_mutex);

            if (!lease) {
                printf("La IP solicitada %s no está asignada a la MAC %s\n",
                    requested_ip, client_mac);
                log_message("WARNING",
                    "La IP solicitada no está asignada al cliente.");

                // Enviar DHCPNAK al cliente
                char nak_message[BUFFER_SIZE];
                snprintf(nak_message, BUFFER_SIZE,
                    "DHCPNAK: Solicitud inválida para IP %s y MAC %s",
                    requested_ip, client_mac);
                sendto(udp_socket, nak_message, strlen(nak_message) + 1, 0,
                    (struct sockaddr *)&client_addr, client_addr_len);
                printf("DHCPNAK enviado a %s:%d -- %s\n",
                    inet_ntoa(client_addr.sin_addr),
                    ntohs(client_addr.sin_port), nak_message);
            }
        } else {
            printf("No se pudo extraer la IP o la MAC del cliente en DHCPREQUEST.\n");
            log_message("ERROR",
                "No se pudo extraer la IP o la MAC del cliente en DHCPREQUEST.");
        }
    } else if (strstr(buffer, "DHCPRELEASE")) {
        // Manejo del mensaje DHCPRELEASE
        // Extraer la IP y MAC del cliente
        char released_ip[16] = {0};
        char client_mac[18] = {0};

        int parsed_fields = sscanf(buffer,
            "DHCPRELEASE: IP=%15[^;]; MAC %17s", released_ip, client_mac);

        if (parsed_fields == 2) {
            // Liberar la IP
            release_ip(released_ip, client_mac);
            printf("IP liberada: %s por cliente %s\n",
                released_ip, client_mac);
        } else {
            printf("No se pudo extraer la IP o la MAC del cliente en DHCPRELEASE.\n");
            log_message("ERROR",
                "No se pudo extraer la IP o la MAC del cliente en DHCPRELEASE.");
        }
    } else {
        printf("Mensaje no reconocido: %s\n", buffer);
    }

    free(request);  // Liberar la memoria asignada para la solicitud
    pthread_exit(NULL);  // Terminar el hilo
}

// Función para asignar una IP disponible al cliente
lease_record* assign_ip(const char* client_mac) {
    lease_record* lease = NULL;
    pthread_mutex_lock(&lease_table_mutex);

    // Buscar si el cliente ya tiene una IP asignada
    for (int i = 0; i < POOL_SIZE; ++i) {
        if (lease_table[i].assigned &&
            strcmp(lease_table[i].mac_address, client_mac) == 0) {
            lease = &lease_table[i];
            break;
        }
    }

    // Si no tiene una IP asignada, asignar una nueva
    if (!lease) {
        for (int i = 0; i < POOL_SIZE; ++i) {
            if (!lease_table[i].assigned) {
                lease = &lease_table[i];
                break;
            }
        }
    }

    pthread_mutex_unlock(&lease_table_mutex);
    return lease;
}

// Función para registrar un nuevo lease
void register_lease(lease_record* lease, const char* client_mac) {
    pthread_mutex_lock(&lease_table_mutex);

    strncpy(lease->mac_address, client_mac, sizeof(lease->mac_address) - 1);
    lease->assigned = 1;
    lease->lease_start_time = time(NULL);
    lease->lease_duration = DEFAULT_LEASE_TIME;  // Establecer la duración del lease
    lease->is_new = 1;  // Marcar el lease como nuevo

    printf("Lease registrado para la IP %s con MAC %s por %ld segundos\n",
        lease->ip, lease->mac_address, lease->lease_duration);

    pthread_mutex_unlock(&lease_table_mutex);
}

// Función para renovar un lease existente
void renew_lease(lease_record* lease) {
    lease->lease_start_time = time(NULL);
    lease->lease_duration = DEFAULT_LEASE_TIME;  // Renovar la duración del lease

    printf("Lease renovado para la IP %s con MAC %s por %ld segundos\n",
        lease->ip, lease->mac_address, lease->lease_duration);
}

// Función para liberar una IP asignada
void release_ip(const char* ip, const char* client_mac) {
    pthread_mutex_lock(&lease_table_mutex);

    for (int i = 0; i < POOL_SIZE; ++i) {
        if (lease_table[i].assigned &&
            strcmp(lease_table[i].ip, ip) == 0 &&
            strcmp(lease_table[i].mac_address, client_mac) == 0) {

            lease_table[i].assigned = 0;
            lease_table[i].lease_start_time = 0;
            lease_table[i].lease_duration = 0;
            lease_table[i].is_new = 0;
            memset(lease_table[i].mac_address, 0, sizeof(lease_table[i].mac_address));
            break;
        }
    }

    pthread_mutex_unlock(&lease_table_mutex);
}

// Función para verificar y liberar leases expirados
void check_expired_leases() {
    pthread_mutex_lock(&lease_table_mutex);

    time_t current_time = time(NULL);

    for (int i = 0; i < POOL_SIZE; ++i) {
        if (lease_table[i].assigned) {
            double elapsed_time = difftime(current_time, lease_table[i].lease_start_time);
            if (elapsed_time >= lease_table[i].lease_duration) {
                printf("Lease expirado para la IP %s.\n", lease_table[i].ip);
                // No liberamos inmediatamente el lease, permitimos que el cliente lo renueve
                // Puedes implementar una política de gracia aquí si lo deseas
                // Por simplicidad, marcaremos el lease como no asignado
                lease_table[i].assigned = 0;
                lease_table[i].lease_start_time = 0;
                lease_table[i].lease_duration = 0;
                lease_table[i].is_new = 0;
                memset(lease_table[i].mac_address, 0, sizeof(lease_table[i].mac_address));
            }
        }
    }

    pthread_mutex_unlock(&lease_table_mutex);
}

// Función para registrar mensajes en un archivo de log
void log_message(const char* level, const char* message) {
    FILE* log_file = fopen("dhcp_server.log", "a");
    if (log_file) {
        time_t now = time(NULL);
        char* timestamp = ctime(&now);
        // Eliminar el carácter de nueva línea
        timestamp[strlen(timestamp) - 1] = '\0';
        fprintf(log_file, "[%s] %s: %s\n", timestamp, level, message);
        fclose(log_file);
    } else {
        perror("No se pudo abrir el archivo de log");
    }
}

// Función para manejar señales del sistema
void signal_handler(int signal) {
    if (signal == SIGINT) {
        printf("\nSeñal de interrupción recibida. Cerrando el servidor...\n");
        server_running = 0;
        close(udp_socket);
        exit(0);
    }
}
