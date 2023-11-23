#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define PORT 8889

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void send_file(FILE *file, int sockfd) {
    char buffer[BUFFER_SIZE];
    int n;

    while ((n = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (write(sockfd, buffer, n) < 0)
            error("Error al escribir en el socket");
    }

    if (n < 0)
        error("Error al leer el archivo");
}

void receive_file(FILE *file, int sockfd) {
    char buffer[BUFFER_SIZE];
    int n;

    while ((n = read(sockfd, buffer, BUFFER_SIZE)) > 0) {
        if (fwrite(buffer, 1, n, file) < n)
            error("Error al escribir en el archivo");
    }

    if (n < 0)
        error("Error al leer desde el socket");
}

void synchronize_directories(const char *local_dir, const char *remote_dir, int sockfd) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    printf("Directorio local: %s\n", local_dir);
    printf("Directorio remoto: %s\n", remote_dir);

    // Abrir el directorio local
    if ((dir = opendir(local_dir)) == NULL) {
        perror("Error al abrir el directorio local");
        return;
    }

    // Leer archivos del directorio local
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Es un archivo regular
            char file_path_local[PATH_MAX];
            char file_path_remote[PATH_MAX];

            snprintf(file_path_local, sizeof(file_path_local), "%s/%s", local_dir, entry->d_name);
            snprintf(file_path_remote, sizeof(file_path_remote), "%s/%s", remote_dir, entry->d_name);

            // Obtener información sobre el archivo local
            if (stat(file_path_local, &file_stat) == -1) {
                perror("Error al obtener información del archivo local");
                continue;
            }

            // Enviar información sobre el archivo al servidor
            if (write(sockfd, &file_stat, sizeof(file_stat)) < 0) {
                perror("Error al enviar información del archivo al servidor");
                continue;
            }

            // Abrir el archivo local
            FILE *file_local = fopen(file_path_local, "rb");
            if (file_local == NULL) {
                perror("Error al abrir el archivo local");
                continue;
            }

            // Enviar el contenido del archivo al servidor
            send_file(file_local, sockfd);

            fclose(file_local);
        }
    }

    // Enviar marca de finalización
    memset(&file_stat, 0, sizeof(file_stat));
    if (write(sockfd, &file_stat, sizeof(file_stat)) < 0)
        perror("Error al enviar marca de finalización al servidor");

    closedir(dir);
}

void *client_handler(void *arg) {
    int newsockfd = *(int *)arg;

    // Lógica para manejar la conexión con el cliente
    // Aquí, recibir información sobre los archivos desde el cliente
    // y realizar la sincronización de directorios

    struct stat file_stat;

    while (1) {
        // Recibir información sobre el archivo desde el cliente
        if (read(newsockfd, &file_stat, sizeof(file_stat)) <= 0)
            break;

        if (S_ISREG(file_stat.st_mode)) {  // Es un archivo regular
            // Recibir el contenido del archivo desde el cliente
            char file_path[PATH_MAX];
            snprintf(file_path, sizeof(file_path), "%s/%s", "/ruta/del/directorio_remoto", "nombre_archivo_remoto");

            FILE *file_remote = fopen(file_path, "wb");
            if (file_remote == NULL) {
                perror("Error al abrir el archivo remoto");
                break;
            }

            receive_file(file_remote, newsockfd);

            fclose(file_remote);
        }
    }

    close(newsockfd);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Uso:\n");
        fprintf(stderr, "  Para ejecutar como servidor: %s <directorio_local>\n", argv[0]);
        fprintf(stderr, "  Para ejecutar como cliente: %s <directorio_local> <directorio_remoto>\n", argv[0]);
        exit(1);
    }

    if (argc == 2) {
        // Modo servidor
        const char *local_dir = argv[1];

        int sockfd, newsockfd, clilen;
        struct sockaddr_in serv_addr, cli_addr;
        pthread_t tid;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
            error("Error al abrir el socket");

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(PORT);

        if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            error("Error al vincular");

        listen(sockfd, 5);
        printf("Servidor en ejecución. Esperando conexiones en el puerto %d...\n", PORT);

        while (1) {
            clilen = sizeof(cli_addr);
            newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen);
            if (newsockfd < 0)
                error("Error al aceptar la conexión");

            printf("Cliente conectado. IP: %s, Puerto: %d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

            pthread_t tid;
            if (pthread_create(&tid, NULL, client_handler, &newsockfd) != 0)
                error("Error al crear el hilo");
        }
    } else if (argc == 3) {
        // Modo cliente
        const char *local_dir = argv[1];
        const char *remote_dir = argv[2];
        int sock;
        struct sockaddr_in server;

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1)
            error("Error al abrir el socket");

        server.sin_addr.s_addr = inet_addr("127.0.0.1");
        server.sin_family = AF_INET;
        server.sin_port = htons(PORT);

        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
            error("Error al conectar con el servidor");

        // Enviar información sobre los archivos al servidor
        synchronize_directories(local_dir, remote_dir, sock);

        close(sock);
    }

    return 0;
}
