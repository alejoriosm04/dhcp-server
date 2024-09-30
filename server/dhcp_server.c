#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char * argv[]) {
    if (argc != 4) {
        printf("Usage: %s <peer_ip> <peer_port> <message>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Get information bout our peer from our arguments
    const char * peer_ip = argv[1];
    int peer_port = atoi(argv[2]);
    const char * message = argv[3];

    struct sockaddr_in peer_addr = {.sin_family = AF_INET, 
                                    .sin_port = htons(peer_port)};

    if (inet_pton(AF_INET, peer_ip, &peer_addr.sin_addr) <= 0) {
        perror("Something wrong with the IP address");
        return EXIT_FAILURE;
    }

    // Create a socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket <= 0) {
        perror("Could not create a socket");
        return EXIT_FAILURE;
    }

    if (sendto(udp_socket, message, strlen(message) + 1, 0, 
           (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) {
        perror("Could not send the message");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    printf("Sent \"%s\" to %s:%d\n", message, peer_ip, peer_port);
    close(udp_socket);

    return EXIT_SUCCESS;
}