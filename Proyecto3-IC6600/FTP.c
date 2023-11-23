#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
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

void list_files(int sockfd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    FILE *ls_output = popen("ls -l", "r");
    if (ls_output == NULL)
        error("Error al ejecutar el comando ls");

    while (fgets(buffer, BUFFER_SIZE, ls_output) != NULL) {
        if (write(sockfd, buffer, strlen(buffer)) < 0)
            break;
    }

    pclose(ls_output);
}

void change_remote_directory(int sockfd, const char *directory) {
    if (chdir(directory) != 0) {
        write(sockfd, "Error: No se pudo cambiar el directorio remoto.\n", strlen("Error: No se pudo cambiar el directorio remoto.\n"));
    } else {
        write(sockfd, "Directorio remoto cambiado con éxito.\n", strlen("Directorio remoto cambiado con éxito.\n"));
    }
}

void change_local_directory(const char *directory) {
    if (chdir(directory) != 0) {
        printf("Error: No se pudo cambiar el directorio local.\n");
    } else {
        printf("Directorio local cambiado con éxito.\n");
    }
}

void print_remote_directory(int sockfd) {
    char current_directory[BUFFER_SIZE];

    if (getcwd(current_directory, BUFFER_SIZE) != NULL) {
        write(sockfd, current_directory, strlen(current_directory));
        write(sockfd, "\n", 1);
    } else {
        write(sockfd, "Error al obtener el directorio remoto actual.\n", strlen("Error al obtener el directorio remoto actual.\n"));
    }
}

void *client_handler(void *arg) {
    int newsockfd = *(int *)arg;
    char buffer[BUFFER_SIZE];

    // Lógica para atender la conexión con el cliente
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = read(newsockfd, buffer, BUFFER_SIZE - 1);
        if (n < 0)
            error("Error al leer desde el socket");

        buffer[strcspn(buffer, "\n")] = '\0'; // Eliminar el salto de línea del final

        char *command = strtok(buffer, " ");

        if (strcmp(command, "open") == 0) {
            // No se realiza ninguna acción adicional
        } else if (strcmp(command, "close") == 0) {
            // No se realiza ninguna acción adicional
        } else if (strcmp(command, "quit") == 0) {
            printf("Cliente desconectado.\n");
            break;
        } else if (strcmp(command, "cd") == 0) {
            char *directory = strtok(NULL, " ");
            change_remote_directory(newsockfd, directory);
        } else if (strcmp(command, "get") == 0) {
            char *filename = strtok(NULL, " ");
            printf("Recibiendo archivo '%s'...\n", filename);
            FILE *file = fopen(filename, "wb");
            if (file == NULL)
                error("Error al crear el archivo");

            receive_file(file, newsockfd);

            fclose(file);
        } else if (strcmp(command, "put") == 0) {
            char *filename = strtok(NULL, " ");
            printf("Enviando archivo '%s'...\n", filename);
            FILE *file = fopen(filename, "rb");
            if (file == NULL)
                error("Error al abrir el archivo");

            send_file(file, newsockfd);

            fclose(file);
        } else if (strcmp(command, "lcd") == 0) {
            char *directory = strtok(NULL, " ");
            change_local_directory(directory);
        } else if (strcmp(command, "ls") == 0) {
            list_files(newsockfd);
        } else if (strcmp(command, "pwd") == 0) {
            print_remote_directory(newsockfd);
        } else {
            printf("Comando no válido.\n");
        }
    }

    // Cerrar el socket de la conexión con el cliente
    close(newsockfd);

    pthread_exit(NULL);
}

int main() {
    int sockfd, newsockfd, clilen;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t tid;

    // Crear un socket servidor
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("Error al abrir el socket");

    // Establecer la dirección del servidor
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // Vincular el socket a la dirección del servidor
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("Error al vincular");

    // Escuchar en el puerto para nuevas conexiones
    if (listen(sockfd, 5) < 0)
        error("Error al escuchar");

    printf("Servidor FTP en ejecución. Esperando conexiones en el puerto %d...\n", PORT);

    while (1) {
        clilen = sizeof(cli_addr);

        // Aceptar una nueva conexión entrante
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, (socklen_t *) &clilen);
        if (newsockfd < 0)
            error("Error al aceptar");

        printf("Cliente conectado. IP: %s, Puerto: %d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        // Crear un nuevo hilo para manejar la conexión con el cliente
        if (pthread_create(&tid, NULL, client_handler, &newsockfd) != 0)
            error("Error al crear el hilo");
    }

    // Cerrar el socket principal
    close(sockfd);

    return 0;
}
