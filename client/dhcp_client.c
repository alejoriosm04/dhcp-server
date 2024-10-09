#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#define BUFFER_SIZE 1024

// Número de solicitudes (hilos) a enviar
#define NUM_REQUESTS 10

// Estructura para pasar datos al hilo
struct thread_data {
    int socket;
    struct sockaddr_in server_addr;
    int thread_id;
};

void *send_request(void *arg) {
    struct thread_data *data = (struct thread_data *)arg;
    char buffer[BUFFER_SIZE];
    struct timeval start, end;

    // Capturamos el tiempo al iniciar la solicitud
    gettimeofday(&start, NULL);

    // Enviar mensaje al servidor
    const char *message = "DHCPREQUEST: Solicitud de configuración";
    if (sendto(data->socket, message, strlen(message) + 1, 0, (struct sockaddr *)&data->server_addr, sizeof(data->server_addr)) < 0) {
        perror("No se pudo enviar el mensaje");
        pthread_exit(NULL);  // Terminar el hilo si falla el envío
    }

    printf("Hilo %d: Mensaje enviado al servidor DHCP en %s:%d\n", data->thread_id, inet_ntoa(data->server_addr.sin_addr), ntohs(data->server_addr.sin_port));

    // Recibir confirmación del servidor
    socklen_t server_addr_len = sizeof(data->server_addr);
    int bytes_received = recvfrom(data->socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&data->server_addr, &server_addr_len);

    if (bytes_received > 0) {
        // Capturamos el tiempo cuando se recibe la confirmación
        gettimeofday(&end, NULL);

        printf("Hilo %d: Confirmación recibida del servidor DHCP: %s\n", data->thread_id, buffer);

        // Calcular el tiempo de respuesta en segundos
        double seconds = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        printf("Hilo %d: Tiempo de respuesta = %.6f segundos\n", data->thread_id, seconds);
    } else {
        perror("No se pudo recibir la confirmación");
    }

    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Uso: %s <server_ip>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    struct sockaddr_in server_addr, client_addr;

    // Configuración del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);  // Puerto del servidor DHCP
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // Crear socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket <= 0) {
        perror("No se pudo crear el socket");
        return EXIT_FAILURE;
    }

    // Habilitar SO_REUSEADDR para permitir múltiples enlaces al mismo puerto
    int opt = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("No se pudo establecer SO_REUSEADDR");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    // Bind al puerto 68 (cliente DHCP)
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(68);  // Puerto DHCP del cliente
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("No se pudo enlazar al puerto 68");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    // Crear hilos para enviar solicitudes
    pthread_t threads[NUM_REQUESTS];
    struct thread_data thread_args[NUM_REQUESTS];

    for (int i = 0; i < NUM_REQUESTS; i++) {
        thread_args[i].socket = udp_socket;
        thread_args[i].server_addr = server_addr;
        thread_args[i].thread_id = i + 1;

        if (pthread_create(&threads[i], NULL, send_request, &thread_args[i]) != 0) {
            perror("No se pudo crear el hilo");
        }
    }

    // Esperar a que todos los hilos terminen
    for (int i = 0; i < NUM_REQUESTS; i++) {
        pthread_join(threads[i], NULL);
    }

    close(udp_socket);

    return EXIT_SUCCESS;
}
