#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define BUFFER_SIZE 1024

int check(int exp, const char *msg);

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int my_port = atoi(argv[1]);
    int udp_rx_socket;
    struct sockaddr_in peer_addr;
    struct sockaddr_in my_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(my_port),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    char buffer[BUFFER_SIZE];


    if ((udp_rx_socket = socket(AF_INET, SOCK_DGRAM, 0)) <= 0) {
        perror("Could not create a socket");
        return EXIT_FAILURE;
    }

    // Bind the socket to the address/port
    int result = bind(udp_rx_socket, (struct sockaddr *)&my_addr, sizeof(my_addr));

    check(result, "Could not bind the socket");

    socklen_t address_length = sizeof(peer_addr);
    int bytes_received = recvfrom(udp_rx_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&peer_addr, &address_length);

    check(bytes_received, "Could not receive message");

    printf("Received a packet from: %s:%d --Message = %s\n", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port), buffer);

    close(udp_rx_socket);

    return EXIT_SUCCESS;
}

#define SOCKET_ERROR (-1)
int check(int exp, const char *msg) {
    if (exp == SOCKET_ERROR) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
    return exp;
}