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
        puts("Client disconnected.");
        fflush(stdout);
    } else if(read_size == -1) {
        perror("recv falhou");
    }

    close(sock);
}

int main() {
    int server_fd, client_sock, c;
    struct sockaddr_in server, client;

    // Criação do socket do servidor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Unable to create socket");
    }
    puts("Socket created");

    // Preparação da estrutura sockaddr_in
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Bind
    if(bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed. Error");
        return 1;
    }
    puts("Bind done");

    // Listen
    listen(server_fd, 3);

    // Aceitar conexões entrantes
    puts("Waiting for connections...");
    c = sizeof(struct sockaddr_in);
    while ((client_sock = accept(server_fd, (struct sockaddr *)&client, (socklen_t*)&c))) {
        puts("Connection accepted");

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            return 1;
        }

        if (pid == 0) {
            // Este é o processo filho
            close(server_fd);
            handleClient(client_sock);
            exit(0);
        } else {
            // Este é o processo pai
            close(client_sock);
        }
    }

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}
