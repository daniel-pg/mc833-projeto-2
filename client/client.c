#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#pragma comment(lib, "ws2_32.lib")

#define TCP_PORT "8080"
#define UDP_PORT 9090
#define SERVER_IP "127.0.0.1"  // Endereço IP do servidor

void downloadMusicUDP(const char *server_ip, int server_port, const char *music_id);

int main() {
    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server;
    char message[1000], server_reply[2000];
    char input[1024];

    // Inicializa o Winsock
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        printf("Failed. Error Code : %d", WSAGetLastError());
        return 1;
    }

    // Criação do socket TCP
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Could not create socket : %d", WSAGetLastError());
    }
    puts("Socket TCP created");

    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(TCP_PORT));

    // Conexão ao servidor remoto
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect failed. Error");
        return 1;
    }

    puts("Connected to TCP server\n");

    // Loop de comunicação TCP
    do {
        printf("Enter command (or type 'udp_download' to download music via UDP): ");
        fgets(input, sizeof(input), stdin);  // Lendo comando do usuário

        // Remove newline character, se existir
        input[strcspn(input, "\n")] = 0;

        // Verifica se o usuário digitou "quit"
        if (strcmp(input, "quit") == 0) {
            break;
        }

        // Se o usuário digitar "udp_download", executa a operação de download via UDP
        if (strcmp(input, "udp_download") == 0) {
            printf("Enter music ID for UDP download: ");
            char music_id[100];
            fgets(music_id, sizeof(music_id), stdin);
            music_id[strcspn(music_id, "\n")] = 0;
            downloadMusicUDP(SERVER_IP, UDP_PORT, music_id);
            continue;
        }

        // Envia a mensagem TCP
        if (send(sock, input, strlen(input), 0) < 0) {
            puts("Send failed");
            break;
        }

        // Limpa o buffer de resposta
        memset(server_reply, 0, sizeof(server_reply));

        // Recebe a resposta do servidor TCP
        int recv_size = recv(sock, server_reply, sizeof(server_reply) - 1, 0);
        if (recv_size < 0) {
            puts("recv failed");
            break;
        } else if (recv_size == 0) {
            puts("Server closed connection");
            break;
        } else {
            // Coloca um terminador de string no final da mensagem recebida antes de imprimir
            server_reply[recv_size] = '\0';
            puts("Server reply:");
            puts(server_reply);
        }
    } while(1);

    closesocket(sock);
    WSACleanup(); // Limpa o Winsock

    return 0;
}

void downloadMusicUDP(const char *server_ip, int server_port, const char *music_id) {
    WSADATA wsaData;
    SOCKET sockfd;
    struct sockaddr_in servaddr;
    char buffer[1024];
    int len, n;
    FILE *file;

    WSAStartup(MAKEWORD(2,2), &wsaData);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        perror("socket creation failed");
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(server_port);
    servaddr.sin_addr.s_addr = inet_addr(server_ip);

    sendto(sockfd, music_id, strlen(music_id), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));

    file = fopen("downloaded_music.mp3", "wb");
    if (file == NULL) {
        perror("Cannot open file");
        closesocket(sockfd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    while (1) {
        n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&servaddr, &len);
        if (n <= 0) break;
        fwrite(buffer, 1, n, file);
    }

    fclose(file);
    closesocket(sockfd);
    WSACleanup();
}