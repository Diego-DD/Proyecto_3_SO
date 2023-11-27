/*----------------------------------------------------------------------------------------------------------------------------------------
Estudiantes:
  - Diego José Masís Quirós - 2020403956.
  - Kevin Jose Rodriguez Lanuza - 2016112117.
Asignación: Proyecto 3 - Sincronización de directorios remotos.

*** Este proyecto fue realizado y testeado de manera LOCAL en el IDE Visual Studio Code y compilada a traves del WSL de Windows 11. ***

INSTRUCCIONES:

El siguiente programa permite sincronizar los archivos internos entre 2 directorios diferentes que pueden encontrarse dentro de la misma máquina, o dentro de 2 máquinas diferentes.

------- COMPILACIÓN -------

Para utilizar el programa primero debe compilar el archivo mediante el siguiente comando:

    g++ Proyecto_3.c

------- EJECUCIÓN -------

Luego, para ejecutar el programa en MODO SERVIDOR se hace de la siguiente manera:

    ./a.out star <ruta del directorio local a sincronizar>

Para ejecutar el programa en MODO CLIENTE se hace de la siguiente manera:

    ./a.out star <ruta del directorio local a sincronizar> <IP de la computadora donde se encuentra el directorio a sincronizar del servidor>

Algunos ejemplos:
[Ejecución local en modo servidor]  ./a.out /mnt/c/Users/diemq/Desktop/Proyecto/Directorio_Local
[Ejecución local en modo cliente]   ./a.out /mnt/c/Users/diemq/Desktop/Proyecto/Directorio_Remoto 127.0.0.1

----------------------------------------------------------------------------------------------------------------------------------------*/

// IMPORTACIÓN DE LIBRERÍAS.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#endif

// DEFINICIÓN DE CONSTANTES GLOBALES.

#define BUFFER_MAXIMO 131072 // 128 KB
#define ARCHIVOS_MAXIMO 500

// DEFINICIÓN DE ESTRUCTURAS.

struct Archivo{
  char nombre_archivo[256];
  long tamano;
  long ultima_modificacion;
  int presente_en_directorio;
};

// DEFINICIÓN DE FUNCIONES.

void generar_registro(const char *ruta_directorio, const char *nombre_archivo){

  /* Esta función genera un registro binario con la información de los archivos presentes en un directorio */

  // Se abre la ruta del directorio.

  DIR *directorio = opendir(ruta_directorio);
  if(directorio == NULL) {
    perror("Error abriendo la ruta del directorio");
    exit(EXIT_FAILURE);
  }

  // Se preparan las variables necesarias para el análisis de los archivos en el directorio.

  struct Archivo registro_archivos_entrantes[ARCHIVOS_MAXIMO];
  int cuenta_de_archivos = 0;

  // Se iterar sobre los archivos en el directorio para extraer la información necesaria.

  struct dirent *archivo_entrante;
  while((archivo_entrante = readdir(directorio)) != NULL && cuenta_de_archivos < ARCHIVOS_MAXIMO){
    if(archivo_entrante->d_type == DT_REG) {
      char ruta_archivo_entrante[256];
      snprintf(ruta_archivo_entrante, sizeof(ruta_archivo_entrante), "%s/%s", ruta_directorio, archivo_entrante->d_name);

      struct stat estadisticas_archivo_entrante;
      if(stat(ruta_archivo_entrante, &estadisticas_archivo_entrante) == 0) {
        strcpy(registro_archivos_entrantes[cuenta_de_archivos].nombre_archivo, archivo_entrante->d_name);
        registro_archivos_entrantes[cuenta_de_archivos].tamano = estadisticas_archivo_entrante.st_size;
        registro_archivos_entrantes[cuenta_de_archivos].ultima_modificacion = estadisticas_archivo_entrante.st_mtime;
        cuenta_de_archivos++;
      }
    }
  }

  // Se cierra el directorio.

  closedir(directorio);

  // Se crear el archivo binario que fungirá como registro.

  FILE *registro_binario = fopen(nombre_archivo, "wb");
  if(registro_binario == NULL) {
    perror("Error al crear el archivo");
    exit(EXIT_FAILURE);
  }

  // Se scribe la información de los archivos en el registro binario.

  fwrite(registro_archivos_entrantes, sizeof(struct Archivo), cuenta_de_archivos, registro_binario);

  // Se cierra el archivo.

  fclose(registro_binario);
}

// Función comentada para no permitir código muerto.
/* void imprimir(const char *registro){

  // Esta función imprime la información de los archivos presentes en un registro binario

  // Se abre el registro binario.

  FILE *archivo = fopen(registro, "rb");
  if(archivo == NULL){
    perror("Error abriendo el registro binario");
    exit(EXIT_FAILURE);
  }

  // Se lee la información de los archivos desde el registro binario.

  struct Archivo registro_archivos[ARCHIVOS_MAXIMO];
  int cuenta_de_archivos = fread(registro_archivos, sizeof(struct Archivo), 100, archivo);

  // Se muestra la información en consola.

  for(int i = 0; i < cuenta_de_archivos; i++){
    printf("---------------------------------------------------------\n");
    printf("Nombre de archivo: %s\n", registro_archivos[i].nombre_archivo);
    printf("Tamaño: %ld bytes\n", registro_archivos[i].tamano);
    printf("Última modificación: %ld\n", registro_archivos[i].ultima_modificacion);
  }

  // Se cierra el archivo.

  fclose(archivo);
} */

void borrar_archivo_de_registro(const char *registro, const char *nombre_archivo){

  /* Esta función borra la información de un único archivo en un registro binario */

  // Se abre el archivo binario para lectura y escritura.

  FILE *archivo = fopen(registro, "rb+");
  if(archivo == NULL){
    perror("Error al abrir el archivo");
    exit(EXIT_FAILURE);
  }

  // Se crea un archivo temporal para almacenar registros que no deben eliminarse.

  FILE *archivo_temporal = fopen("temporal.bin", "wb");
  if(archivo_temporal == NULL){
    perror("Error al crear el archivo temporal");
    fclose(archivo);
    exit(EXIT_FAILURE);
  }

  // Se leen registros, se copian los que no deben eliminarse y se elimina el que coincide con el nombre dado.

  struct Archivo info_archivo;
  while(fread(&info_archivo, sizeof(struct Archivo), 1, archivo) == 1){
    if(strcmp(info_archivo.nombre_archivo, nombre_archivo) != 0){

      // No se encontró el archivo: Se debe escribir este registro en el archivo temporal.

      fwrite(&info_archivo, sizeof(struct Archivo), 1, archivo_temporal);
    }
  }

  // Se cierran ambos archivos.

  fclose(archivo);
  fclose(archivo_temporal);

  // Se reemplaza el archivo original con el archivo temporal.

  remove(registro);
  rename("temporal.bin", registro);
}

void borrar_archivos_existentes_de_registro(const char *ruta_directorio, const char *registro_binario){

  /* Esta función borra la información de todos los archivos (presentes dentro de un directorio) en un registro binario. */

  // Se abre la ruta del directorio.

  DIR *archivo = opendir(ruta_directorio);
  if(archivo == NULL){
    perror("Error al abrir el directorio");
    exit(EXIT_FAILURE);
  }

  // Se lee el contenido del interior del directorio.

  struct dirent *archivo_entrante;
  while((archivo_entrante = readdir(archivo)) != NULL){

    // Ignorar entradas especiales "." y ".."
    if(strcmp(archivo_entrante->d_name, ".") != 0 && strcmp(archivo_entrante->d_name, "..") != 0){
      // Se eliminar el registro del archivo dentro del registro binario.
      borrar_archivo_de_registro(registro_binario, archivo_entrante->d_name);
    }
  }

  // Se cierra el directorio.

  closedir(archivo);
}

bool existe_registro(const char *nombre_archivo){

  /* Esta función verifica si un registro binario existe o no */

  // Se abre el archivo binario para lectura.

  FILE *archivo = fopen(nombre_archivo, "rb");
  if(archivo == NULL){
    perror("Error al abrir el archivo");
    exit(EXIT_FAILURE);
  }

  // Se mueve el puntero de lectura al final del archivo.

  fseek(archivo, 0, SEEK_END);

  // Se obtiene la posición actual, que es el tamaño del archivo.

  long tamano = ftell(archivo);

  // Se cierrar el archivo.

  fclose(archivo);

  // Se devuelve verdadero si el tamaño es cero (archivo vacío), falso en caso contrario.

  return (tamano == 0);
}

void borrar_archivos_de_directorio(const char *ruta_directorio, const char *registro_archivos_por_borrar){

  /* Esta función borra todos los archivos de un directorio en función de lo que indique un registro binario */

  // Se abre el registro binario para lectura.

  FILE *archivo = fopen(registro_archivos_por_borrar, "rb");
  if (archivo == NULL){
    perror("Error al abrir el archivo de registro");
    exit(EXIT_FAILURE);
  }

  // Se leen los registros.

  struct Archivo informacion_archivo;
  while(fread(&informacion_archivo, sizeof(struct Archivo), 1, archivo) == 1){

    // Se concatena la ruta del directorio con el nombre del archivo.

    char ruta_archivo[512];
    snprintf(ruta_archivo, sizeof(ruta_archivo), "%s/%s", ruta_directorio, informacion_archivo.nombre_archivo);

    // Se borrar el archivo del directorio.

    if(remove(ruta_archivo) != 0){
      perror("Error al borrar el archivo");
    }
  }

  // Se cierra el archivo de registro.

  fclose(archivo);
}

bool archivo_en_directorio(const char *ruta_directorio, const char *nombre_archivo){

  /* Esta función verifica si un archivo existe dentro de un directorio */

  // Se abre el directorio.

  DIR *directorio = opendir(ruta_directorio);
  
  if(directorio == NULL){
    perror("Error al abrir el directorio");
    exit(EXIT_FAILURE);
  }

  // Se itera sobre los archivos existentes en el directorio.

  struct dirent *archivo_entrante;
  while((archivo_entrante = readdir(directorio)) != NULL){

    // Se compara el nombre del archivo entrante en revisión con el nombre del archivo buscado.

    if (strcmp(archivo_entrante->d_name, nombre_archivo) == 0) {

      // Si el archivo se encuentra en el directorio se cierra el directorio y se retorna true.

      closedir(directorio);
      return true;
    }
  }

  // Si el archivo no se encuentra en el directorio se cierra el directorio y se retorna false.

  closedir(directorio);
  return false;
}

int comparar_informacion_archivos(const void *archivo_A, const void *archivo_B){

  /* Esta función compara dos archivos en función de su nombre */

  // Devuelve 0 si son iguales, 1 si no lo son.
  
  return strcmp(((struct Archivo *)archivo_A)->nombre_archivo, ((struct Archivo *)archivo_B)->nombre_archivo);
}

void enviar_archivo(int socket, const char *nombre_archivo){

  /* Esta función envía un archivo a través de un socket */

  // Se abre el archivo para lectura.

  FILE *archivo = fopen(nombre_archivo, "rb");
  if(archivo == NULL){
    perror("Error opening archivo");
    return;
  }

  // Se obtiene el tamaño del archivo.

  fseek(archivo, 0, SEEK_END);
  long tamano_archivo = ftell(archivo);
  fseek(archivo, 0, SEEK_SET);

  // Se envía el tamaño del archivo.

  if(send(socket, &tamano_archivo, sizeof(tamano_archivo), 0) == -1){
    perror("Error al enviar el tamaño del archivo");
    fclose(archivo);
    return;
  }

  // Se envía el archivo.

  char buffer[BUFFER_MAXIMO];
  size_t bytes_leidos;
  while((bytes_leidos = fread(buffer, 1, sizeof(buffer), archivo)) > 0){
    size_t total_bytes_enviados = 0;
    while(total_bytes_enviados < bytes_leidos){
      ssize_t bytes_enviados = send(socket, buffer + total_bytes_enviados, bytes_leidos - total_bytes_enviados, 0);
      if(bytes_enviados == -1){
        perror("Error enviando la información del archivo");
        fclose(archivo);
        return;
      }
      total_bytes_enviados += bytes_enviados;
    }
  }

  // Se cierra el archivo.

  fclose(archivo);
}

void recibir_archivo(int socket, const char *nombre_archivo){

  /* Esta función recibe un archivo a través de un socket */

  // Se recibe el tamaño del archivo.

  long tamano_archivo;
  if(recv(socket, &tamano_archivo, sizeof(tamano_archivo), 0) <= 0){
    perror("Error recibiendo el tamaño del archivo");
    return;
  }

  // Se crea el archivo receptor de los datos del archivo enviado.

  FILE *archivo = fopen(nombre_archivo, "wb");
  if(archivo == NULL){
    perror("Error creando el archivo receptor");
    return;
  }

  // Se reciben los datos del archivo enviado.

  char buffer[BUFFER_MAXIMO];
  size_t bytes_totales_recibidos = 0;
  while(bytes_totales_recibidos < tamano_archivo){
    size_t bytes_por_recibir = (tamano_archivo - bytes_totales_recibidos) > sizeof(buffer) ? sizeof(buffer) : (size_t)(tamano_archivo - bytes_totales_recibidos);
    ssize_t bytes_recibidos = recv(socket, buffer, bytes_por_recibir, 0);
    if(bytes_recibidos <= 0){
      perror("Error recibiendo los datos del archivo");
      fclose(archivo);
      return;
    }
    fwrite(buffer, 1, bytes_recibidos, archivo);
    bytes_totales_recibidos += bytes_recibidos;
  }

  // Se cierra el archivo.

  fclose(archivo);
}

void sincronizar_crear_actualizar(int socket, const char *directorio_local, int es_servidor){

  /* Esta función sincroniza dos directorios a través de un socket */

  // Obtener la lista de archivos locales.

  struct dirent *entrada;
  DIR *directorio = opendir(directorio_local);
  if(directorio == NULL){
    perror("Error al abrir el directorio local");
    return;
  }

  struct Archivo archivos_locales[ARCHIVOS_MAXIMO];
  int cuenta_archivos_locales = 0;

  while((entrada = readdir(directorio)) != NULL){
    if(entrada->d_type == DT_REG){
      char ruta_archivo[256];
      snprintf(ruta_archivo, sizeof(ruta_archivo), "%s/%s", directorio_local, entrada->d_name);

      struct stat estadistica_archivo;
      if(stat(ruta_archivo, &estadistica_archivo) == 0){
        strcpy(archivos_locales[cuenta_archivos_locales].nombre_archivo, entrada->d_name);
        archivos_locales[cuenta_archivos_locales].tamano = estadistica_archivo.st_size;
        archivos_locales[cuenta_archivos_locales].ultima_modificacion = estadistica_archivo.st_mtime;
        // Se marcar como presente por defecto.
        archivos_locales[cuenta_archivos_locales].presente_en_directorio = 1;
        cuenta_archivos_locales++;
      }
    }
  }

  closedir(directorio);

  // Ordenar la lista de archivos locales por nombre.

  qsort(archivos_locales, cuenta_archivos_locales, sizeof(struct Archivo), comparar_informacion_archivos);

  if(es_servidor){

    // SECCÍON DE CÓDIGO PARA EL SERVIDOR.

    // Enviar la lista de archivos del servidor al cliente.

    if(send(socket, &cuenta_archivos_locales, sizeof(cuenta_archivos_locales), 0) == -1){
      perror("SERVIDOR - Error enviando la cuenta de archivos locales");
      return;
    }

    if(send(socket, archivos_locales, sizeof(struct Archivo) * cuenta_archivos_locales, 0) == -1){
      perror("SERVIDOR - Error enviando los archivos locales");
      return;
    }

    // Recibir la lista de archivos del cliente.

    int cuenta_archivos_remotos;
    if(recv(socket, &cuenta_archivos_remotos, sizeof(cuenta_archivos_remotos), 0) <= 0){
      perror("SERVIDOR - Error al recibir la cuenta de archivos remotos");
      return;
    }

    struct Archivo archivos_remotos[ARCHIVOS_MAXIMO];
    if(recv(socket, archivos_remotos, sizeof(struct Archivo) * cuenta_archivos_remotos, 0) <= 0){
      perror("SERVIDOR - Error recibiendo los archivos remotos");
      return;
    }

    // Comparar y sincronizar archivos.

    int indice_local = 0;
    int indice_remoto = 0;

    while(indice_local < cuenta_archivos_locales || indice_remoto < cuenta_archivos_remotos){
      if(indice_local < cuenta_archivos_locales && indice_remoto < cuenta_archivos_remotos){
        int comparacion_de_archivos = comparar_informacion_archivos(&archivos_locales[indice_local], &archivos_remotos[indice_remoto]);

        if(comparacion_de_archivos == 0){

          // Archivo encontrado en ambas máquinas, comparar fechas de modificación.

          if(archivos_locales[indice_local].ultima_modificacion > archivos_remotos[indice_remoto].ultima_modificacion){

            // Enviar el archivo al cliente.

            enviar_archivo(socket, archivos_locales[indice_local].nombre_archivo);

          }else if(archivos_locales[indice_local].ultima_modificacion < archivos_remotos[indice_remoto].ultima_modificacion){

            // Recibir el archivo del cliente.

            recibir_archivo(socket, archivos_locales[indice_local].nombre_archivo);
          }

          indice_local++;
          indice_remoto++;

        }else if(comparacion_de_archivos < 0){

          // Archivo local más antiguo o no presente en el cliente, enviar al cliente.

          enviar_archivo(socket, archivos_locales[indice_local].nombre_archivo);
          indice_local++;

        }else{

          // Archivo del cliente más antiguo o no presente localmente, recibir del cliente.

          recibir_archivo(socket, archivos_remotos[indice_remoto].nombre_archivo);
          indice_remoto++;
        }

      }else if(indice_local < cuenta_archivos_locales){

        // Archivo local adicional, enviar al cliente
        
        enviar_archivo(socket, archivos_locales[indice_local].nombre_archivo);
        indice_local++;

      }else if(indice_remoto < cuenta_archivos_remotos){

        if(archivos_remotos[indice_remoto].presente_en_directorio){

          // Archivo del cliente adicional, recibir del cliente.

          recibir_archivo(socket, archivos_remotos[indice_remoto].nombre_archivo);

        }else{
          // Archivo del cliente eliminado, no hacer nada.
        }

        indice_remoto++;

      }
    }
  }else{

    // SECCÍON DE CÓDIGO PARA EL CLIENTE.

    // Recibir la lista de archivos del servidor.

    int cuenta_archivos_remotos;
    if(recv(socket, &cuenta_archivos_remotos, sizeof(cuenta_archivos_remotos), 0) <= 0){
      perror("CLIENTE - Error recibiendo la cuenta de archivos remotos");
      return;
    }

    struct Archivo archivos_remotos[ARCHIVOS_MAXIMO];
    if(recv(socket, archivos_remotos, sizeof(struct Archivo) * cuenta_archivos_remotos, 0) <= 0){
      perror("CLIENTE - Error recibiendo los archivos remotos");
      return;
    }

    // Enviar la lista de archivos al servidor.

    if(send(socket, &cuenta_archivos_locales, sizeof(cuenta_archivos_locales), 0) == -1){
      perror("CLIENTE - Error enviando la cuenta de archivos locales");
      return;
    }

    if(send(socket, archivos_locales, sizeof(struct Archivo) * cuenta_archivos_locales, 0) == -1){
      perror("CLIENTE - Error enviando los archivos locales");
      return;
    }

    // Comparar y sincronizar archivos.

    int indice_local = 0;
    int indice_remoto = 0;

    while(indice_local < cuenta_archivos_locales || indice_remoto < cuenta_archivos_remotos){
      if(indice_local < cuenta_archivos_locales && indice_remoto < cuenta_archivos_remotos){
        int comparacion_de_archivos = comparar_informacion_archivos(&archivos_locales[indice_local], &archivos_remotos[indice_remoto]);

        if(comparacion_de_archivos == 0){

          // Archivo encontrado en ambas máquinas, comparar fechas de modificación.

          if(archivos_locales[indice_local].ultima_modificacion > archivos_remotos[indice_remoto].ultima_modificacion){

            // Enviar el archivo al servidor.

            enviar_archivo(socket, archivos_locales[indice_local].nombre_archivo);

          }else if(archivos_locales[indice_local].ultima_modificacion < archivos_remotos[indice_remoto].ultima_modificacion){

            // Recibir el archivo del servidor.

            recibir_archivo(socket, archivos_locales[indice_local].nombre_archivo);
          }

          indice_local++;
          indice_remoto++;

        }else if(comparacion_de_archivos < 0){

          // Archivo local más antiguo o no presente en el servidor, enviar al servidor.

          enviar_archivo(socket, archivos_locales[indice_local].nombre_archivo);
          indice_local++;

        }else{

          // Archivo del servidor más antiguo o no presente localmente, recibir del servidor.

          recibir_archivo(socket, archivos_remotos[indice_remoto].nombre_archivo);
          indice_remoto++;

        }
      }else if(indice_local < cuenta_archivos_locales){

        // Archivo local adicional, enviar al servidor.

        enviar_archivo(socket, archivos_locales[indice_local].nombre_archivo);
        indice_local++;

      }else if(indice_remoto < cuenta_archivos_remotos){

        if(archivos_remotos[indice_remoto].presente_en_directorio){

          // Archivo del servidor adicional, recibir del servidor.

          recibir_archivo(socket, archivos_remotos[indice_remoto].nombre_archivo);

        }else{
          // Archivo del servidor eliminado, no hacer nada
        }
        
        indice_remoto++;

      }
    }
  }
}

void sincronizar_borrar(const char *ruta_directorio_local, int socket_local, int socket_remoto, int modo){

  /* Esta función sincroniza dos directorios a través de un socket tomando en cuenta archivos borrados entre ejecuciones */

  if(modo == 1){

    // Se borran del registro los archivos que aún existen en el directorio local.

    borrar_archivos_existentes_de_registro(ruta_directorio_local, "registro.bin");

    // Se renombra el registro para que el cliente lo pueda recibir.

    rename("registro.bin", "registroServidor.bin");
    enviar_archivo(socket_remoto, "registroServidor.bin");

    // Se borra el registro alterado.

    remove("registroServidor.bin");

    // Se recibe el registro alterado del cliente.

    recibir_archivo(socket_remoto, "registroCliente.bin");

    // Se borran del directorio los archivos que indique el registro enviado remotamente.

    if(!existe_registro("registroCliente.bin")){
      borrar_archivos_de_directorio(ruta_directorio_local, "registroCliente.bin");
    }

    // Se borra el registro recibido.

    remove("registroCliente.bin");

    // Se sincronizan los directorios normalmente y se genera el nuevo registro.

    sincronizar_crear_actualizar(socket_remoto, ruta_directorio_local, 1);
    generar_registro(ruta_directorio_local, "registro.bin");
  }else{

    // Se borran del registro los archivos que aún existen en el directorio local.
    borrar_archivos_existentes_de_registro(ruta_directorio_local, "registro.bin");

    // Se renombra el registro para que el servidor lo pueda recibir.

    rename("registro.bin", "registroCliente.bin");
    enviar_archivo(socket_local, "registroCliente.bin");

    // Se borra el registro alterado.

    remove("registroCliente.bin");

    // Se recibe el registro alterado del servidor.

    recibir_archivo(socket_local, "registroServidor.bin");

    // Se borran del directorio los archivos que indique el registro enviado remotamente.
    if(!existe_registro("registroServidor.bin")){
      borrar_archivos_de_directorio(ruta_directorio_local, "registroServidor.bin");
    }

    // Se borra el registro recibido.

    remove("registroServidor.bin");

    // Se sincronizan los directorios normalmente y se genera el nuevo registro.
    
    sincronizar_crear_actualizar(socket_local, ruta_directorio_local, 1);
    generar_registro(ruta_directorio_local, "registro.bin");
  }
}

// PROGRAMA PRINCIPAL.

int main(int argc, char *argv[]){
  if(argc < 2 || argc > 3){
    printf("Disculpe!!! El uso correcto es: %s <ruta_del_directorio_local> <IP_del_otro_extremo>\n", argv[0]);
    return 1;
  }

  const char *ruta_directorio_local = argv[1];

  if(argc == 2){
    
    // SECCIÓN DEL CÓDIGO PARA EL SERVIDOR.

    // Se inicializa Winsock en Windows.

    #ifdef _WIN32
      WSADATA wsa_data;
      if(WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0){
        perror("Fallo al inicializar Winsock");
        return 1;
      }
    #endif

    printf("Inicializando servidor...\n");

    // Se crear el socket para la conexión con el otro extremo.

    int socket_local = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_local == -1){
      perror("Error al crear el socket");
      return 1;
    }

    // Se configura la dirección del servidor.

    printf("Configurando dirección del servidor...\n");

    struct sockaddr_in direccion_servidor;
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(8889);
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;

    // Se realiza el bind y listen.

    if(bind(socket_local, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0){
      perror("Fallo en el bind");
      return 1;
    }

    printf("Esperando conexiones en el puerto %d...\n", ntohs(direccion_servidor.sin_port));

    listen(socket_local, 1);

    // Se acepta la conexión entrante.

    int socket_remoto;
    struct sockaddr_in direccion_remota;
    socklen_t tamano_direccion_remota = sizeof(direccion_remota);

    socket_remoto = accept(socket_local, (struct sockaddr *)&direccion_remota, &tamano_direccion_remota);
    if(socket_remoto < 0){
      perror("Fallo aceptando la conexión entrante del cliente");
      return 1;
    }

    printf("Conexión aceptada de %s:%d\n", inet_ntoa(direccion_remota.sin_addr), ntohs(direccion_remota.sin_port));

    // Lógica para la sincronización tomando en cuenta archivos borrados entre ejecuciones.

    printf("Sincronizando directorios...\n");

    if(!archivo_en_directorio(ruta_directorio_local, "registro.bin")){
      sincronizar_crear_actualizar(socket_remoto, ruta_directorio_local, 1);
      generar_registro(ruta_directorio_local, "registro.bin");
    }else{
      sincronizar_borrar(ruta_directorio_local, socket_local, socket_remoto, 1);
    }

    printf("Sincronización exitosa...\n");

    // Se cierra el socket del cliente.

    close(socket_remoto);

    // Se cierra el socket local.

    close(socket_local);

    // Limpiar Winsock en Windows
    #ifdef _WIN32
      WSACleanup();
    #endif

    printf("La instancia del servidor ha finalizado.\n");

  }else{
    
    // SECCIÓN DEL CÓDIGO PARA EL CLIENTE.

    printf("Inicializando cliente...\n");

    // Se crea socket para la conexión con el servidor.

    int socket_local = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_local == -1){
      perror("Error creando el socket");
      return 1;
    }

    // Se configura la dirección del servidor.

    printf("Configurando dirección del servidor para el cliente...\n");

    struct sockaddr_in direccion_servidor;
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(8889);

    // Se configura como cliente y se conecta al servidor.

    if(inet_pton(AF_INET, argv[2], &direccion_servidor.sin_addr) <= 0){
      perror("Error con la dirección IP del servidor");
      return 1;
    }

    if(connect(socket_local, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0){
      perror("Error conectando al servidor");
      return 1;
    }

    printf("Conexión exitosa al servidor. IP: %s, Puerto: %d\n", argv[2], ntohs(direccion_servidor.sin_port));

    // Lógica para la sincronización tomando en cuenta archivos borrados entre ejecuciones.

    printf("Sincronizando directorios...\n");

    if(!archivo_en_directorio(ruta_directorio_local, "registro.bin")){
      sincronizar_crear_actualizar(socket_local, ruta_directorio_local, 1);
      generar_registro(ruta_directorio_local, "registro.bin");
    }else{
      sincronizar_borrar(ruta_directorio_local, socket_local, 0, 2);
    }

    printf("Sincronización exitosa...\n");

    // Se cierra el socket local.

    close(socket_local);

    printf("La instancia del cliente ha finalizado.\n");
  }

  // Se termina la instancia del programa.

  return 0;
}
