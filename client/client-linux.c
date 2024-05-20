#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sysexits.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT "8080"

int main(int argc, char* argv[]) {
    struct addrinfo hints;
    struct addrinfo *srv_info;
    char* srv_hostname;
    char* srv_port;
    int sockfd;

    // Get server's ip:port from the command line arguments.
    if (argc > 3) {
        fprintf(stderr, "Usage: client [HOSTNAME [PORT]]\n");
        exit(EX_USAGE);
    } else {
        srv_hostname = (argc >= 2) ? argv[1] : NULL;
        srv_port = (argc == 3) ? argv[2] : DEFAULT_PORT;
    }

    // Get list of network addresses for server
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = getprotobyname("tcp")->p_proto;

    int errcode;
    if (0 != (errcode = getaddrinfo(srv_hostname, srv_port, &hints, &srv_info))) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(errcode));
        exit(1);
    }

    // TODO
    struct addrinfo* p;
    for(p = srv_info; p != NULL; p = p->ai_next) {
        if (-1 != (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))) {
            break;
        }
    }

    if (sockfd == -1) {
        perror("Failed to create socket");
        exit(1);
    }

    if (-1 == connect(sockfd, p->ai_addr, p->ai_addrlen)) {
        perror("Connection error");
        exit(1);
    }
    puts("Client connected succesfully!\n");

    char message[1024];
    char server_reply[2048];
    while (true) {
        printf("Enter message: ");
        if (NULL == fgets(message, sizeof(message), stdin)) {
            break;
        }
        message[strcspn(message, "\n")] = 0;  // Strip newline from input

        if ((0 == strcmp(message, "q")) || (0 == strcmp(message, "quit"))) {
            break;
        }

        // Send message
        if (-1 == send(sockfd, message, strlen(message), 0)) {
            perror("Send failed");
            break;
        }

        // Clear reply buffer
        memset(server_reply, 0, sizeof(server_reply));

        int recv_size = recv(sockfd, server_reply, sizeof(server_reply) - 1, 0);
        if (recv_size == -1) {
            perror("recv failed");
            break;
        } else if (recv_size == 0) {
            puts("Server closed connection");
            break;
        } else {
            server_reply[recv_size] = '\0';
            puts("Server reply:");
            puts(server_reply);
        }
    }

    freeaddrinfo(srv_info);
    return 0;
}
