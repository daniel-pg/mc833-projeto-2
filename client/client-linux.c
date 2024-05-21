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

static void _get_server_addrinfo(
    const char *restrict srv_hostname,
    const char *restrict srv_port,
    struct addrinfo **restrict srv_info_tcp,
    struct addrinfo **restrict srv_info_udp
) {

    struct addrinfo hints_tcp, hints_udp;

    // Get list of network addresses for server
    memset(&hints_tcp, 0, sizeof(hints_tcp));
    memset(&hints_udp, 0, sizeof(hints_udp));

    hints_tcp.ai_family = AF_UNSPEC;
    hints_tcp.ai_socktype = SOCK_STREAM;
    hints_tcp.ai_protocol = getprotobyname("tcp")->p_proto;

    hints_udp.ai_family = AF_UNSPEC;
    hints_udp.ai_socktype = SOCK_DGRAM;
    hints_udp.ai_protocol = getprotobyname("udp")->p_proto;

    int errcode;
    if (0 != (errcode = getaddrinfo(srv_hostname, srv_port, &hints_tcp, srv_info_tcp))) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(errcode));
        exit(1);
    }

    if (0 != (errcode = getaddrinfo(srv_hostname, srv_port, &hints_udp, srv_info_udp))) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(errcode));
        exit(1);
    }
}

static int _create_socket(struct addrinfo *srv_info, struct addrinfo **po) {
    int sockfd = -1;

    struct addrinfo *p;
    for(p = srv_info; p != NULL; p = p->ai_next) {
        if (-1 != (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))) {
            break;
        }
    }

    *po = p;
    return sockfd;
}

static void _send_command_mainloop(int sockfd_tcp, int sockfd_udp, struct addrinfo *p_tcp, struct addrinfo *p_udp) {
    char message[1024];
    char server_reply[2048];

    // Connect to TCP socket
    if (-1 == connect(sockfd_tcp, p_tcp->ai_addr, p_tcp->ai_addrlen)) {
        perror("Connection error");
        exit(1);
    }
    puts("Client connected succesfully!\n");

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
        if (-1 == send(sockfd_tcp, message, strlen(message), 0)) {
            perror("Send failed");
            break;
        }

        // Clear reply buffer
        memset(server_reply, 0, sizeof(server_reply));

        int recv_size = recv(sockfd_tcp, server_reply, sizeof(server_reply) - 1, 0);
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
}

int main(int argc, char *argv[]) {
    char *srv_hostname, *srv_port;

    // Get server's ip:port from the command line arguments.
    if (argc > 3) {
        fprintf(stderr, "Usage: client [HOSTNAME [PORT]]\n");
        exit(EX_USAGE);
    } else {
        srv_hostname = (argc >= 2) ? argv[1] : NULL;
        srv_port = (argc == 3) ? argv[2] : DEFAULT_PORT;
    }

    struct addrinfo *srv_info_tcp, *srv_info_udp;
    _get_server_addrinfo(srv_hostname, srv_port, &srv_info_tcp, &srv_info_udp);

    struct addrinfo *p_tcp, *p_udp;
    int sockfd_tcp, sockfd_udp;

    // Create TCP socket
    sockfd_tcp = _create_socket(srv_info_tcp, &p_tcp);
    if (sockfd_tcp == -1) {
        perror("Failed to create TCP socket");
        exit(1);
    }

    // Create UDP socket
    sockfd_udp = _create_socket(srv_info_udp, &p_udp);
    if (sockfd_udp == -1) {
        perror("Failed to create UDP socket");
        exit(1);
    }

    _send_command_mainloop(sockfd_tcp, sockfd_udp, p_tcp, p_udp);

    freeaddrinfo(srv_info_tcp);
    freeaddrinfo(srv_info_udp);
    return 0;
}
