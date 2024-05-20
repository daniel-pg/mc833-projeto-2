#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080

int main() {
    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server;
    char message[1000], server_reply[2000];
    int slen = sizeof(server);
    int useUDP = 0;  // Flag para controle do protocolo usado

    // Inicializa o Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Failed. Error Code : %d", WSAGetLastError());
        return 1;
    }

    // Configuração comum do servidor
    memset(&server, 0, sizeof(server));
    server.sin_addr.s_addr = inet_addr("172.30.208.101");
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    // Criação inicial do socket TCP
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("Could not create socket : %d", WSAGetLastError());
        return 1;
    }

    // Conexão ao servidor TCP
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect failed. Error");
        return 1;
    }
    puts("Connected with TCP\n");

    do {
        printf("Enter message: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0; // Remove newline

        if (strcmp(message, "quit") == 0) {
            break;
        }

        //se a mensagem enviada tiver 'download ' altera a conexão para UDP
        if (strncmp(message, "download ", 9) == 0) { 
            closesocket(sock); // Fecha o socket atual
            sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); //conecta UDP e muda a flag
            useUDP = 1;
            puts("Switched to UDP\n");
        }
        else { //se não, altera para TCP
            closesocket(sock); // Fecha o socket atual
            sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            connect(sock, (struct sockaddr *)&server, sizeof(server)); //conecta TCP e muda a flag
            useUDP = 0;
            puts("Switched to TCP\n");
        }

        if (useUDP) {
            sendto(sock, message, strlen(message), 0, (struct sockaddr *)&server, slen);

            FILE *file = fopen("received_file.mp3", "wb");
            if (file == NULL) {
                perror("Failed to open file");
                return 1;
            }

            int end_of_file_received = 0;
            while (!end_of_file_received) {
                memset(server_reply, 0, sizeof(server_reply));
                int bytes_received = recvfrom(sock, server_reply, sizeof(server_reply) - 1, 0, (struct sockaddr *)&server, &slen);
                if (bytes_received < 0) {
                    perror("recvfrom failed");
                    break;
                }

                // Verificar se é uma mensagem de fim de arquivo
                if (bytes_received == 0 || strcmp(server_reply, "END OF FILE") == 0) {
                    puts("File received successfully.");
                    end_of_file_received = 1;
                    continue;
                }

                fwrite(server_reply, 1, bytes_received, file);
            }    

            fclose(file);
        } else {
            send(sock, message, strlen(message), 0);
            memset(server_reply, 0, sizeof(server_reply));
            recv(sock, server_reply, sizeof(server_reply) - 1, 0);
            puts("Server reply:");
            puts(server_reply);
        }
    } while (1);

    closesocket(sock);
    WSACleanup(); // Limpa o Winsock

    return 0;
}
