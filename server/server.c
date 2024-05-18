#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <locale.h>

#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct {
    char title[100];
    char artist[100];
    char language[50];
    char genre[50];
    char chorus[200];
    int release_year;
} Music;

struct ResponseBuffer {
    char* buffer;      // Buffer para acumular a resposta
    size_t bufferSize; // Tamanho total do buffer
    size_t length;     // Tamanho atual dos dados no buffer
};

// Função para obter o caminho do arquivo a partir do ID
int get_file_path(const char* id, char* file_path) {
    sqlite3 *db;
    sqlite3_stmt *res;
    int error = 0;

    int rc = sqlite3_open("MusicDatabase.db", &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    char sql[128];
    sprintf(sql, "SELECT file_path FROM music WHERE id = %s;", id);

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        if (sqlite3_step(res) == SQLITE_ROW) {
            strcpy(file_path, (char*)sqlite3_column_text(res, 0));
        } else {
            fprintf(stderr, "No match found\n");
            error = -1;
        }
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        error = -1;
    }

    sqlite3_finalize(res);
    sqlite3_close(db);

    return error;
}

// Função para enviar o arquivo
void send_file(const char* file_path, int udp_fd, struct sockaddr_in from, socklen_t from_len) {
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("File open failed");
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        sendto(udp_fd, buffer, bytes_read, 0, (struct sockaddr *)&from, from_len);
    }

    fclose(file);
    printf("File sent successfully\n");
}

static int callback(void* data, int argc, char** argv, char** azColName) {
    struct ResponseBuffer* resp = (struct ResponseBuffer*)data;
    
    for (int i = 0; i < argc; i++) {
        const char* column = azColName[i];
        const char* value = argv[i] ? argv[i] : "NULL";
        // Calcula o tamanho necessário para a string incluindo o terminador nulo
        size_t needed = snprintf(NULL, 0, "%s = %s\n", column, value) + 1;
        
        if (resp->length + needed < resp->bufferSize) {
            // Formata e adiciona a string ao buffer
            snprintf(resp->buffer + resp->length, needed, "%s = %s\n", column, value);
            resp->length += needed - 1; // Ajusta o tamanho atual dos dados no buffer
        } else {
            // Buffer cheio, interrompe a execução
            return 1;
        }
    }

    // Adiciona uma quebra de linha extra APENAS após processar todos os campos de UM registro
    if (resp->length + 2 < resp->bufferSize) { // +2 para incluir a quebra de linha extra e o terminador nulo
        strcat(resp->buffer + resp->length, "\n");
        resp->length += 1; // Apenas incrementa por 1 porque já consideramos o '\n' de cada campo
    } else {
        // Se não houver espaço suficiente no buffer para a quebra de linha extra
        return 1; // Interrompe a execução
    }

    return 0; // Continua processando
}

// Defina um tamanho máximo para a resposta
#define MAX_RESPONSE_SIZE 8192
#define MAX_QUERY_SIZE 1024
void listMusic(int sock, const char* language, const char* year, const char* genre, const char* id) {
    sqlite3 *db;
    char* errMsg = 0;
    char sql[MAX_QUERY_SIZE] = "SELECT id, title, artist FROM music WHERE 1=1";
    if(id && strlen(id) > 0){
        strcpy(sql, "SELECT * FROM music WHERE 1=1");
    }

    char condition[256] = "";
    int rc;
    
    // Adiciona condições à consulta baseada nos parâmetros fornecidos
    if (language && strlen(language) > 0) {
        snprintf(condition, sizeof(condition), " AND language = '%s'", language);
        strcat(sql, condition);
    }
    if (year && strlen(year) > 0) {
        snprintf(condition, sizeof(condition), " AND release_year = %s", year);
        strcat(sql, condition);
    }
    if (genre && strlen(genre) > 0) {
        snprintf(condition, sizeof(condition), " AND genre = '%s'", genre);
        strcat(sql, condition);
    }
    if (id && strlen(id) > 0) {
        snprintf(condition, sizeof(condition), " AND id = %s", id);
        strcat(sql, condition);
    }
    
    // Se todos os parâmetros são NULL ou "", lista todas as informações de todas as músicas
    if (!(language || year || genre || id)) {
        strcpy(sql, "SELECT * FROM music");
    }
    
    char response[MAX_RESPONSE_SIZE]; // Define um tamanho apropriado para seu caso
    struct ResponseBuffer resp = {response, MAX_RESPONSE_SIZE, 0};

    rc = sqlite3_open("MusicDatabase.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return;
    }

    rc = sqlite3_exec(db, sql, callback, &resp, &errMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
    } else if (resp.length == 0) {
        // Se nenhum registro foi encontrado, resp.length não terá aumentado
        snprintf(response, sizeof(response), "Nenhuma música encontrada.\n");
    } else {
        fprintf(stdout, "All music listed successfully\n");
    }

    sqlite3_close(db);

    // Verifica se algo foi acumulado no buffer e envia para o cliente
    if (resp.length > 0) {
        send(sock, response, resp.length, 0); // Envia a resposta acumulada para o cliente
    }else{
        char *no_results = "Sem resultados\n";
        send(sock, no_results, strlen(no_results), 0);
    }
}

int removeMusic(int id){
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char sql[256];
    int rc;

    rc = sqlite3_open("MusicDatabase.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 0;
    } else {
        fprintf(stderr, "Opened database successfully\n");
    }

    snprintf(sql, sizeof(sql), "DELETE FROM music WHERE id = %d;", id);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 0;
    } else {
        fprintf(stdout, "Music removed successfully\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 1;
}

int insertMusic(Music music) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO music (title, artist, language, genre, chorus, release_year) VALUES (?, ?, ?, ?, ?, ?);";
    int rc;

    rc = sqlite3_open("MusicDatabase.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 0;
    } else {
        fprintf(stderr, "Opened database successfully\n");
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    // Vincula os dados da estrutura Music aos placeholders SQL
    sqlite3_bind_text(stmt, 1, music.title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, music.artist, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, music.language, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, music.genre, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, music.chorus, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, music.release_year);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 0;
    } else {
        fprintf(stdout, "Record created successfully\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 1;
}

void handleClient(int sock) {
    char client_message[BUFFER_SIZE];
    int read_size;
    char *saveptr;

    // Limpa a mensagem buffer
    memset(client_message, 0, BUFFER_SIZE);

    // Recebe mensagem do cliente e responde
    while ((read_size = recv(sock, client_message, BUFFER_SIZE - 1, 0)) > 0) {
        // Coloca um terminador de string no final da mensagem recebida
        client_message[read_size] = '\0';

        char *command = strtok_r(client_message, " ", &saveptr);
        if (command != NULL) {
            char *response = "Comando não encontrado.";
            //determina o comando que foi enviado
            if (strcmp(command, "addmusic") == 0) { //adicionar musica 
                Music newMusic;
                strncpy(newMusic.title, strtok_r(NULL, "|", &saveptr), sizeof(newMusic.title) - 1);
                strncpy(newMusic.artist, strtok_r(NULL, "|", &saveptr), sizeof(newMusic.artist) - 1);
                strncpy(newMusic.language, strtok_r(NULL, "|", &saveptr), sizeof(newMusic.language) - 1);
                strncpy(newMusic.genre, strtok_r(NULL, "|", &saveptr), sizeof(newMusic.genre) - 1);
                strncpy(newMusic.chorus, strtok_r(NULL, "|", &saveptr), sizeof(newMusic.chorus) - 1);
                char *year = strtok_r(NULL, "|", &saveptr);
                newMusic.release_year = year ? atoi(year) : 0;

                // Insere a música
                int inserted = insertMusic(newMusic);
                response = inserted ? "Música adicionada com sucesso!" : "Erro ao adicionar música.";
            } else if (strcmp(command, "removemusic") == 0) { //remover musica
                char *idStr = strtok_r(NULL, " ", &saveptr);
                int id = atoi(idStr);

                int removed = removeMusic(id);
                response = removed ? "Música removida com sucesso!" : "Erro ao remover música.";
            } else if (strcmp(command, "listall") == 0) { //listar todas as musicas
                char *language = NULL, *year = NULL, *genre = NULL, *id = NULL;
                char *param;
                while ((param = strtok_r(NULL, " ", &saveptr)) != NULL) {
                    if (strncmp(param, "-language=", 10) == 0) {
                        language = param + 10;
                    } else if (strncmp(param, "-year=", 6) == 0) {
                        year = param + 6;
                    } else if (strncmp(param, "-genre=", 7) == 0) {
                        genre = param + 7;
                    } else if (strncmp(param, "-id=", 4) == 0) {
                        id = param + 4;
                    }
                }

                listMusic(sock, language, year, genre, id);
                continue;
            }
            //envia a mensagem de resposta ao cliente
            send(sock, response, strlen(response), 0);
        }
        
        // Limpa a mensagem buffer novamente
        memset(client_message, 0, BUFFER_SIZE);
    }

    if(read_size == 0) {
        puts("TCP Client disconnected.");
        fflush(stdout);
    } else if(read_size == -1) {
        perror("recv falhou");
    }

    close(sock);
}

int main() {
    int server_fd, client_sock, c, udp_fd;
    struct sockaddr_in server, client, server_udp;

    // Criação do socket TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Could not create socket");
        return 1;
    }
    puts("TCP Socket created");

    // Criação do socket UDP
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd == -1) {
        perror("Could not create UDP socket");
        return 1;
    }
    puts("UDP Socket created");

    // Preparação da estrutura sockaddr_in para TCP
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Preparação da estrutura sockaddr_in para UDP
    server_udp = server;

    // Bind TCP
    if(bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed. TCP Error");
        return 1;
    }
    puts("Bind done for TCP");

    // Bind UDP
    if(bind(udp_fd, (struct sockaddr *)&server_udp, sizeof(server_udp)) < 0) {
        perror("bind failed. UDP Error");
        return 1;
    }
    puts("Bind done for UDP");

    // Listen para TCP
    listen(server_fd, 3);
    puts("Waiting for connections...");

    fd_set readfds;
    int max_sd;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(udp_fd, &readfds);
        max_sd = (server_fd > udp_fd) ? server_fd : udp_fd;

        // Espera por uma atividade em um dos sockets, timeout é NULL, então espera indefinidamente
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("select error");
            continue;
        }

        // Checa se é atividade no socket TCP para aceitar uma nova conexão
        if (FD_ISSET(server_fd, &readfds)) {
            client_sock = accept(server_fd, (struct sockaddr *)&client, (socklen_t*)&c);
            if (client_sock < 0) {
                perror("accept failed");
                return 1;
            }
            puts("Connection accepted");

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                return 1;
            }
            if (pid == 0) {
                close(server_fd);
                handleClient(client_sock);
                exit(0);
            } else {
                close(client_sock);
            }
        }

        // Checa se é atividade no socket UDP para receber dados
        if (FD_ISSET(udp_fd, &readfds)) {
            char buffer[1024];
            struct sockaddr_in from;
            socklen_t from_len = sizeof(from);
            ssize_t bytes_received = recvfrom(udp_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&from, &from_len);
            if (bytes_received < 0) {
                perror("UDP recvfrom error");
            } else {
                buffer[bytes_received] = '\0';  // Certifica que a string é terminada corretamente

                char command[10];
                char id[512];
                char file_path[1024];
                char *error_msg = "File not found";
                sendto(udp_fd, error_msg, strlen(error_msg), 0, (struct sockaddr *)&from, from_len);
                // Analisa o buffer para extrair o comando e o id
                if (sscanf(buffer, "%s %s", command, id) == 2) {
                    if (strcmp(command, "download") == 0) {
                        if (strcmp(command, "download") == 0) {
                            if (get_file_path(id, file_path) == 0) {
                                send_file(file_path, udp_fd, from, from_len);
                                sendto(udp_fd, "END OF FILE", strlen("END OF FILE"), 0, (struct sockaddr *)&from, from_len);
                            } else {
                                char *error_msg = "File not found";
                                sendto(udp_fd, error_msg, strlen(error_msg), 0, (struct sockaddr *)&from, from_len);
                            }
                        }
                    }
                } else {
                    // Resposta padrão caso o parsing falhe
                    char *errorMsg = "Invalid command format";
                    sendto(udp_fd, errorMsg, strlen(errorMsg), 0, (struct sockaddr *)&from, from_len);
                    printf("Error: %s\n", errorMsg);
                }
            }
        }
    }

    return 0;
}