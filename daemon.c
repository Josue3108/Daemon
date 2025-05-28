#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/file.h>

// Definir F_TLOCK si no está disponible
#ifndef F_TLOCK
#define F_TLOCK 2
#endif

// Definir F_ULOCK si no está disponible  
#ifndef F_ULOCK
#define F_ULOCK 0
#endif

// Variables globales
static char *pid_file_name = "/tmp/hello_daemon.pid";
static int pid_fd = -1;
static volatile int running = 1;
static FILE *log_file = NULL;

// Tu función daemonize original
static void daemonize()
{
    pid_t pid = 0;
    int fd;
    
    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    /* On success: The child process becomes session leader */
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }
    
    /* Ignore signal sent from child to parent process */
    signal(SIGCHLD, SIG_IGN);
    
    /* Fork off for the second time*/
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    /* Set new file permissions */
    umask(0);
    
    /* Change the working directory to the root directory */
    chdir("/");
    
    /* Close all open file descriptors */
    for (fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--) {
        close(fd);
    }
    
    /* Reopen stdin (fd = 0), stdout (fd = 1), stderr (fd = 2) */
    stdin = fopen("/dev/null", "r");
    stdout = fopen("/dev/null", "w+");
    stderr = fopen("/dev/null", "w+");
    
    /* Try to write PID of daemon to lockfile */
    if (pid_file_name != NULL) {
        char str[256];
        pid_fd = open(pid_file_name, O_RDWR|O_CREAT, 0640);
        if (pid_fd < 0) {
            exit(EXIT_FAILURE);
        }
        if (lockf(pid_fd, F_TLOCK, 0) < 0) {
            exit(EXIT_FAILURE);
        }
        sprintf(str, "%d\n", getpid());
        write(pid_fd, str, strlen(str));
    }
}

// Manejador de señales para terminar limpiamente
void signal_handler(int sig)
{
    running = 0;
}

// Función para escribir Hello World con timestamp
void write_hello_world()
{
    time_t now;
    char *time_str;
    
    // Abrir archivo en modo append
    log_file = fopen("/tmp/hello_world.txt", "a");
    if (log_file == NULL) {
        return; // Si no puede abrir el archivo, simplemente continua
    }
    
    // Obtener timestamp actual
    time(&now);
    time_str = ctime(&now);
    // Remover el \n del final de ctime
    time_str[strlen(time_str) - 1] = '\0';
    
    // Escribir al archivo
    fprintf(log_file, "[%s] Hello World\n", time_str);
    fflush(log_file); // Asegurar que se escriba inmediatamente
    
    // Cerrar archivo
    fclose(log_file);
    log_file = NULL;
}

// Función de limpieza
void cleanup()
{
    if (log_file != NULL) {
        fclose(log_file);
    }
    
    if (pid_fd != -1) {
        lockf(pid_fd, F_ULOCK, 0);
        close(pid_fd);
    }
    
    if (pid_file_name != NULL) {
        unlink(pid_file_name);
    }
}

int main(int argc, char *argv[])
{
    int daemon_mode = 0;
    
    // Verificar si se quiere ejecutar como daemon
    if (argc > 1 && (strcmp(argv[1], "-d") == 0 || strcmp(argv[1], "--daemon") == 0)) {
        daemon_mode = 1;
    }
    
    if (daemon_mode) {
        printf("Iniciando como daemon...\n");
        printf("El daemon escribirá 'Hello World' cada 10 segundos en /tmp/hello_world.txt\n");
        printf("Para detenerlo: kill $(cat /tmp/hello_daemon.pid)\n");
        printf("Para ver la salida: tail -f /tmp/hello_world.txt\n");
        
        // Convertir a daemon
        daemonize();
    } else {
        printf("Ejecutando en primer plano...\n");
        printf("Presiona Ctrl+C para detener\n");
        printf("Escribiendo en /tmp/hello_world.txt cada 10 segundos...\n");
    }
    
    // Configurar manejador de señales
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    // Escribir mensaje inicial
    if (daemon_mode) {
        log_file = fopen("/tmp/hello_world.txt", "a");
        if (log_file) {
            fprintf(log_file, "=== Daemon iniciado ===\n");
            fclose(log_file);
            log_file = NULL;
        }
    }
    
    // Bucle principal
    while (running) {
        write_hello_world();
        
        if (!daemon_mode) {
            printf("Hello World escrito al archivo\n");
        }
        
        sleep(10); // Esperar 10 segundos
    }
    
    // Escribir mensaje final
    if (daemon_mode) {
        log_file = fopen("/tmp/hello_world.txt", "a");
        if (log_file) {
            fprintf(log_file, "=== Daemon terminado ===\n");
            fclose(log_file);
            log_file = NULL;
        }
    } else {
        printf("Programa terminado\n");
    }
    
    cleanup();
    return EXIT_SUCCESS;
}