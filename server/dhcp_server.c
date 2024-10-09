#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024

// Estructura para pasar datos al hilo
struct thread_data {
    int socket;
    struct sockaddr_in client_addr;
    char buffer[BUFFER_SIZE];
    int client_id;  // Agregamos un identificador para cada cliente
};

// Función que maneja las solicitudes de los clientes en un nuevo hilo
void *handle_client(void *data) {
    struct thread_data *client_data = (struct thread_data *)data;
    char confirmation_message[BUFFER_SIZE];

    struct timeval start, end;
    gettimeofday(&start, NULL);  // Capturamos el tiempo al iniciar el procesamiento

    printf("Cliente %d: Mensaje recibido de %s:%d -- %s\n",
           client_data->client_id,
           inet_ntoa(client_data->client_addr.sin_addr),
           ntohs(client_data->client_addr.sin_port),
           client_data->buffer);

    // Simulamos algún procesamiento
    //sleep(2);

    // Preparar el mensaje de confirmación con el ID del cliente
    snprintf(confirmation_message, sizeof(confirmation_message), "DHCPACK: Configuración enviada al cliente %d", client_data->client_id);

    // Enviar confirmación al cliente
    if (sendto(client_data->socket, confirmation_message, strlen(confirmation_message) + 1, 0,
               (struct sockaddr *)&client_data->client_addr, sizeof(client_data->client_addr)) < 0) {
        perror("No se pudo enviar la confirmación");
    } else {
        printf("Cliente %d: Confirmación enviada a %s:%d\n",
               client_data->client_id,
               inet_ntoa(client_data->client_addr.sin_addr),
               ntohs(client_data->client_addr.sin_port));
    }

    gettimeofday(&end, NULL);  // Capturamos el tiempo al finalizar el procesamiento

    // Calcular el tiempo de procesamiento en segundos
    double seconds = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Cliente %d: Tiempo de procesamiento = %.6f segundos\n", client_data->client_id, seconds);

    free(client_data);  // Liberar memoria asignada para la estructura de datos
    pthread_exit(NULL);
}

int main() {
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
        return EXIT_FAILURE;
    }

    // Bind del socket al puerto 67
    if (bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Could not bind the socket");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    printf("Servidor DHCP escuchando en el puerto 67...\n");

    int client_id = 1;  // Inicializamos el identificador de cliente

    // Loop para recibir mensajes de clientes
    while (1) {
        socklen_t client_addr_len = sizeof(client_addr);
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);

        if (bytes_received > 0) {
            // Asignar memoria para los datos del cliente
            struct thread_data *client_data = malloc(sizeof(struct thread_data));
            if (client_data == NULL) {
                perror("Error al asignar memoria");
                continue;
            }

            // Copiar los datos necesarios a la estructura
            client_data->socket = udp_socket;
            client_data->client_addr = client_addr;
            strncpy(client_data->buffer, buffer, bytes_received);
            client_data->client_id = client_id++;  // Asignar un identificador único a cada cliente

            // Crear un nuevo hilo para procesar la solicitud del cliente
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_client, (void *)client_data) != 0) {
                perror("Error al crear el hilo");
                free(client_data);  // Liberar memoria si falla la creación del hilo
            }

            // Desvincular el hilo para que se libere su memoria automáticamente al terminar
            pthread_detach(thread_id);
        } else {
            perror("No se pudo recibir el mensaje");
        }
    }

    close(udp_socket);
    return EXIT_SUCCESS;
}
