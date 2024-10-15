#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>  // Agregar este include

#define BUFFER_SIZE 1024
#define CLIENT_LOG_FILE "client/dhcp_client.log"
// Valores para el algoritmo exponential backoff
#define INITIAL_INTERVAL 10   // Intervalo inicial en segundos
#define MAX_INTERVAL 900       // Intervalo máximo en segundos
#define MAX_TOTAL_WAIT 3600    // Tiempo máximo total de espera en segundos (opcional)

// Variable global para detener la renovación cuando se presiona Enter
int stop_renewal = 0;

// Estructura para pasar parámetros a la función del hilo de renovación
typedef struct {
    int udp_socket;
    struct sockaddr_in* server_addr;
    char* assigned_ip;
    const char* mac_address;
    long lease_time;
} lease_params_t;

//Funcion para generar un mensaje de log
void log_message(const char* level, const char* message, const char* ip, const char* mac) {
    FILE* log_file = fopen(CLIENT_LOG_FILE, "a");
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

// Función para generar una dirección MAC aleatoria (simulando diferentes clientes)
void generate_random_mac(char* mac) {
    srand(time(NULL) + getpid());
    snprintf(mac, 18, "00:%02x:%02x:%02x:%02x:%02x",
             rand() % 256, rand() % 256, rand() % 256,
             rand() % 256, rand() % 256);
}

// Función para enviar DHCPRELEASE al servidor
void send_dhcp_release(int udp_socket, struct sockaddr_in* server_addr, const char* assigned_ip, const char* mac_address) {
    char release_message[BUFFER_SIZE];
    snprintf(release_message, BUFFER_SIZE, "DHCPRELEASE: IP=%s; MAC %s", assigned_ip, mac_address);

    if (sendto(udp_socket, release_message, strlen(release_message) + 1, 0,
               (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("No se pudo enviar el mensaje DHCPRELEASE");
        log_message("ERROR", "No se pudo enviar el mensaje DHCPRELEASE", assigned_ip, mac_address);
    } else {
        printf("\n---- IP Liberada ----\n");
        printf("IP %s liberada correctamente.\n", assigned_ip);
        printf("----------------------\n");
        log_message("INFO", "IP liberada correctamente", assigned_ip, mac_address);
    }
}

// Solicitar renovación del lease
void renew_lease(int udp_socket, struct sockaddr_in* server_addr, char* assigned_ip, const char* mac_address) {
    char request_message[BUFFER_SIZE];
    snprintf(request_message, BUFFER_SIZE, "DHCPREQUEST: IP=%s; MAC %s", assigned_ip, mac_address);

    if (sendto(udp_socket, request_message, strlen(request_message) + 1, 0,
               (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("No se pudo enviar el mensaje DHCPREQUEST para renovación");
        log_message("ERROR", "No se pudo enviar el mensaje DHCPREQUEST para renovación", assigned_ip, mac_address);
    } else {
        printf("\n---- Renovando Lease ----\n");
        printf("Solicitando renovación para IP: %s\n", assigned_ip);
        log_message("INFO", "Solicitando renovación de lease", assigned_ip, mac_address);

        // Recibir confirmación del servidor (DHCPACK)
        char buffer[BUFFER_SIZE];
        socklen_t server_addr_len = sizeof(*server_addr);

        // Establecer tiempo de espera para recvfrom
        struct timeval tv;
        tv.tv_sec = 5;  // Tiempo de espera de 5 segundos
        tv.tv_usec = 0;
        setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)server_addr, &server_addr_len);
        if (bytes_received > 0 && strstr(buffer, "DHCPACK")) {
            buffer[bytes_received] = '\0'; // Asegurar que el buffer es un string válido

            // Parsear el mensaje DHCPACK
            char subnet_mask[16];
            char default_gateway[16];
            char dns_server[16];
            long lease_time;

            int parsed_fields = sscanf(buffer,
                "DHCPACK: IP=%15[^;]; MASK=%15[^;]; GATEWAY=%15[^;]; DNS=%15[^;]; LEASE=%ld",
                assigned_ip, subnet_mask, default_gateway, dns_server, &lease_time);

            if (parsed_fields == 5) {
                printf("Renovación exitosa:\n");
                printf("IP Asignada: %s\n", assigned_ip);
                printf("Máscara de Subred: %s\n", subnet_mask);
                printf("Puerta de Enlace Predeterminada: %s\n", default_gateway);
                printf("Servidor DNS: %s\n", dns_server);
                printf("Duración del Lease: %ld segundos\n", lease_time);
                printf("------------------------\n");
                log_message("INFO", "Renovación de lease exitosa", assigned_ip, mac_address);
            } else {
                printf("No se pudo parsear correctamente la confirmación de renovación del servidor.\n");
                log_message("ERROR", "No se pudo parsear correctamente la confirmación de renovación del servidor", assigned_ip, mac_address);
            }
        } else {
            printf("No se pudo recibir la confirmación DHCPACK de renovación.\n");
            log_message("ERROR", "No se pudo recibir la confirmación DHCPACK de renovación", assigned_ip, mac_address);
        }
    }
}

// Función para manejar la renovación automática del lease en un hilo
void* lease_renewal_loop(void* args) {
    lease_params_t* params = (lease_params_t*)args;
    while (!stop_renewal) {  // Mientras no se presione Enter
        long wait_time = params->lease_time / 2;  // Esperar el 50% del tiempo del lease
        printf("Esperando %ld segundos para la siguiente renovación...\n", wait_time);
        log_message("INFO", "Esperando para la siguiente renovación", params->assigned_ip, params->mac_address);
        sleep(wait_time);

        // Si stop_renewal es 1, salir del bucle
        if (stop_renewal) break;

        // Renovar el lease
        renew_lease(params->udp_socket, params->server_addr, params->assigned_ip, params->mac_address);
    }

    return NULL;
}

// Función para escuchar Enter en otro hilo
void* listen_for_enter(void* arg) {
    (void)arg; 
    getchar();  // Espera a que el usuario presione Enter
    stop_renewal = 1;  // Cambia la variable para detener la renovación
    return NULL;
}

int main() {
    FILE* log_file = fopen(CLIENT_LOG_FILE, "w");
    if (log_file != NULL) {
        fclose(log_file);
    }

    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    char mac_address[18];
    char assigned_ip[16] = ""; 

    // Generar una dirección MAC aleatoria
    generate_random_mac(mac_address);
    printf("\n---- Cliente DHCP ----\n");
    printf("MAC Address del cliente: %s\n", mac_address);
    printf("----------------------\n");
    log_message("INFO", "Cliente DHCP iniciado", assigned_ip, mac_address);

    // Crear socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket <= 0) {
        perror("No se pudo crear el socket");
        log_message("ERROR", "No se pudo crear el socket", assigned_ip, mac_address);
        return EXIT_FAILURE;
    }

    // Habilitar el modo broadcast en el socket
    int broadcastEnable = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("No se pudo habilitar el modo broadcast");
        log_message("ERROR", "No se pudo habilitar el modo broadcast", assigned_ip, mac_address);
        close(udp_socket);
        return EXIT_FAILURE;
    }

    // Configuración del cliente
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(68);  
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind al puerto 68 (cliente DHCP)
    if (bind(udp_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("No se pudo enlazar al puerto 68");
        log_message("ERROR", "No se pudo enlazar al puerto 68", assigned_ip, mac_address);
        close(udp_socket);
        return EXIT_FAILURE;
    }

    // Configuración para el servidor de broadcast
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);
    server_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); 

    // Variables para el algoritmo exponential backoff
    int current_interval = INITIAL_INTERVAL;
    int total_wait_time = 0;

    // Bucle de reintentos con exponential backoff
    while (1) {
        // Enviar mensaje DHCPDISCOVER al servidor mediante broadcast
        char discover_message[BUFFER_SIZE];
        snprintf(discover_message, BUFFER_SIZE, "DHCPDISCOVER: MAC %s Solicitud de configuración", mac_address);
        if (sendto(udp_socket, discover_message, strlen(discover_message) + 1, 0,
                   (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("No se pudo enviar el mensaje DHCPDISCOVER");
            log_message("ERROR", "No se pudo enviar el mensaje DHCPDISCOVER", assigned_ip, mac_address);
            close(udp_socket);
            return EXIT_FAILURE;
        }

        printf("\n---- Mensaje DHCPDISCOVER ----\n");
        printf("Mensaje DHCPDISCOVER enviado por broadcast al puerto 67\n");
        printf("-------------------------------\n");
        log_message("INFO", "Mensaje DHCPDISCOVER enviado", assigned_ip, mac_address);

        // Establecer tiempo de espera para recvfrom
        struct timeval tv;
        tv.tv_sec = current_interval;  // Tiempo de espera actual
        tv.tv_usec = 0;
        setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // Recibir respuesta del servidor
        socklen_t server_addr_len = sizeof(server_addr);
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&server_addr, &server_addr_len);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            if (strstr(buffer, "DHCPNOIP")) {
                printf("DHCPNOIP: El servidor DHCP informó que no hay direcciones IP disponibles.\n");
                log_message("INFO", "El servidor DHCP informó que no hay direcciones IP disponibles", assigned_ip, mac_address);

                // Incrementar el tiempo total de espera
                total_wait_time += current_interval;

                // Comprobar si se ha alcanzado el tiempo máximo total de espera
                if (total_wait_time >= MAX_TOTAL_WAIT) {
                    printf("Tiempo máximo de espera alcanzado. Saliendo.\n");
                    log_message("ERROR", "Tiempo máximo de espera alcanzado", assigned_ip, mac_address);
                    close(udp_socket);
                    return EXIT_FAILURE;
                }

                // Incrementar el intervalo de espera con exponential backoff
                current_interval *= 2;
                if (current_interval > MAX_INTERVAL) {
                    current_interval = MAX_INTERVAL;
                }

                // Esperar un tiempo aleatorio entre 0 y current_interval
                int wait_time = rand() % current_interval;
                printf("Esperando %d segundos antes de reintentar...\n", wait_time);
                log_message("INFO", "Esperando antes de reintentar", assigned_ip, mac_address);
                sleep(wait_time);

                continue;  // Reintentar enviar DHCPDISCOVER
            } else if  (strstr(buffer, "DHCPOFFER")) {
                printf("\n---- Oferta Recibida ----\n");
                printf("Oferta del servidor DHCP: %s\n", buffer);
                printf("--------------------------\n");
                log_message("INFO", "Oferta del servidor DHCP recibida", assigned_ip, mac_address);

                // Variables para almacenar la información
                char subnet_mask[16];
                char default_gateway[16];
                char dns_server[16];
                long lease_time;

                // Parsear el mensaje DHCPOFFER
                int parsed_fields = sscanf(buffer,
                    "DHCPOFFER: IP=%15[^;]; MASK=%15[^;]; GATEWAY=%15[^;]; DNS=%15[^;]; LEASE=%ld",
                    assigned_ip, subnet_mask, default_gateway, dns_server, &lease_time);

                if (parsed_fields == 5) {
                    printf("\n---- Información Recibida ----\n");
                    printf("IP Asignada: %s\n", assigned_ip);
                    printf("Máscara de Subred: %s\n", subnet_mask);
                    printf("Puerta de Enlace Predeterminada: %s\n", default_gateway);
                    printf("Servidor DNS: %s\n", dns_server);
                    printf("Duración del Lease: %ld segundos\n", lease_time);
                    printf("-------------------------------\n");
                    log_message("INFO", "Información de lease recibida", assigned_ip, mac_address);

                    // Crear hilo para manejar la renovación automática del lease
                    lease_params_t params = {udp_socket, &server_addr, assigned_ip, mac_address, lease_time};
                    pthread_t renewal_thread;
                    pthread_create(&renewal_thread, NULL, lease_renewal_loop, (void*)&params);

                    // Crear hilo para escuchar el Enter
                    pthread_t enter_thread;
                    pthread_create(&enter_thread, NULL, listen_for_enter, NULL);

                    // Esperar a que el usuario presione Enter
                    pthread_join(enter_thread, NULL);

                    // Enviar DHCPRELEASE al servidor al terminar
                    send_dhcp_release(udp_socket, &server_addr, assigned_ip, mac_address);

                    // Cerrar el socket y terminar
                    close(udp_socket);
                    log_message("INFO", "Cliente DHCP terminado", assigned_ip, mac_address);
                    return EXIT_SUCCESS;
                } else {
                    printf("No se pudo parsear correctamente la oferta del servidor.\n");
                    log_message("ERROR", "No se pudo parsear correctamente la oferta del servidor", assigned_ip, mac_address);
                    // Podemos decidir si reintentar o salir
                    close(udp_socket);
                    return EXIT_FAILURE;
                }
            } else {
                printf("Mensaje desconocido del servidor: %s\n", buffer);
                log_message("ERROR", "Mensaje desconocido del servidor", assigned_ip, mac_address);
                // Podemos decidir si reintentar o salir
                close(udp_socket);
                return EXIT_FAILURE;
            }
        } else {
            // No se recibió respuesta dentro del tiempo de espera
            printf("No se recibió respuesta del servidor dentro del tiempo de espera.\n");
            log_message("ERROR", "No se recibió respuesta del servidor dentro del tiempo de espera", assigned_ip, mac_address);

            // Incrementar el tiempo total de espera
            total_wait_time += current_interval;

            // Comprobar si se ha alcanzado el tiempo máximo total de espera
            if (total_wait_time >= MAX_TOTAL_WAIT) {
                printf("Tiempo máximo de espera alcanzado. Saliendo.\n");
                log_message("ERROR", "Tiempo máximo de espera alcanzado", assigned_ip, mac_address);
                close(udp_socket);
                return EXIT_FAILURE;
            }

            // Incrementar el intervalo de espera con exponential backoff
            current_interval *= 2;
            if (current_interval > MAX_INTERVAL) {
                current_interval = MAX_INTERVAL;
            }

            // Esperar el tiempo actual antes de reintentar (sin aleatorización)
            int wait_time = current_interval;
            printf("Esperando %d segundos antes de reintentar...\n", wait_time);
            log_message("INFO", "Esperando antes de reintentar", assigned_ip, mac_address);
            sleep(wait_time);

            continue;  // Reintentar enviar DHCPDISCOVER
        }
    }

    // Nunca debería llegar aquí
    close(udp_socket);
    return EXIT_FAILURE;
}