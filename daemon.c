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
#include <dirent.h>

#ifndef F_TLOCK
#define F_TLOCK 2
#endif

#ifndef F_ULOCK
#define F_ULOCK 0
#endif

static char *pid_file_name = "/run/cpu_temp_logger/daemon.pid";
static int pid_fd = -1;
static volatile int running = 1;
static FILE *log_file = NULL;
static const char *thermal_path = "/sys/class/thermal/";

#define TEMP_THRESHOLD 65000  // 65.0 C en miligrados

static void daemonize()
{
    pid_t pid = 0;
    int fd;

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    signal(SIGCHLD, SIG_IGN);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);

    chdir("/");

    for (fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--) close(fd);

    stdin = fopen("/dev/null", "r");
    stdout = fopen("/dev/null", "w+");
    stderr = fopen("/dev/null", "w+");

    if (pid_file_name != NULL) {
        char str[256];
        pid_fd = open(pid_file_name, O_RDWR|O_CREAT, 0640);
        if (pid_fd < 0) exit(EXIT_FAILURE);
        if (lockf(pid_fd, F_TLOCK, 0) < 0) exit(EXIT_FAILURE);
        sprintf(str, "%d\n", getpid());
        write(pid_fd, str, strlen(str));
    }
}

void signal_handler(int sig)
{
    running = 0;
}

void cleanup()
{
    if (log_file != NULL) fclose(log_file);
    if (pid_fd != -1) {
        lockf(pid_fd, F_ULOCK, 0);
        close(pid_fd);
    }
    if (pid_file_name != NULL) unlink(pid_file_name);
}

int read_temp_from_file(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f) return -1;
    int temp = -1;
    if (fscanf(f, "%d", &temp) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return temp;
}

int get_max_cpu_temp()
{
    DIR *dir;
    struct dirent *entry;
    int max_temp = -1;

    dir = opendir(thermal_path);
    if (!dir) return -1;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "thermal_zone", 12) == 0) {
            char temp_file[512];
            snprintf(temp_file, sizeof(temp_file), "%s%s/temp", thermal_path, entry->d_name);
            int temp = read_temp_from_file(temp_file);
            if (temp > max_temp) {
                max_temp = temp;
            }
        }
    }
    closedir(dir);
    return max_temp;
}

void send_notification(const char *message)
{
    char command[512];
    snprintf(command, sizeof(command), "notify-send 'Alerta CPU' '%s'", message);
    system(command);
}

void write_log(int temp_milli)
{
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';  // quitar '\n'

    log_file = fopen("/tmp/cpu_temp_log.txt", "a");
    if (!log_file) return;

    double temp_c = temp_milli / 1000.0;
    fprintf(log_file, "[%s] Temperatura CPU: %.1f°C\n", time_str, temp_c);
    fflush(log_file);
    fclose(log_file);
}

int main(int argc, char *argv[])
{
    int daemon_mode = 0;

    if (argc > 1 && (strcmp(argv[1], "-d") == 0 || strcmp(argv[1], "--daemon") == 0)) {
        daemon_mode = 1;
    }

    if (daemon_mode) {
        printf("Iniciando como daemon...\n");
        printf("El daemon registrará la temperatura máxima del CPU cada 10 segundos en /tmp/cpu_temp_log.txt\n");
        printf("Para detenerlo: kill $(cat /tmp/hello_daemon.pid)\n");
        printf("Para ver la salida: tail -f /tmp/cpu_temp_log.txt\n");
        daemonize();
    } else {
        printf("Ejecutando en primer plano...\n");
        printf("Presiona Ctrl+C para detener\n");
        printf("Registrando temperatura máxima CPU cada 10 segundos...\n");
    }

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    while (running) {
        int temp = get_max_cpu_temp();
        if (temp >= 0) {
            write_log(temp);
            if (temp >= TEMP_THRESHOLD) {
                char alert_msg[128];
                snprintf(alert_msg, sizeof(alert_msg), "¡Temperatura alta! %.1f°C", temp / 1000.0);
                send_notification(alert_msg);
            }
            if (!daemon_mode) {
                printf("Temperatura CPU: %.1f°C\n", temp / 1000.0);
            }
        } else {
            if (!daemon_mode) {
                printf("Error leyendo temperatura CPU\n");
            }
        }
        sleep(10);
    }

    cleanup();
    return EXIT_SUCCESS;
}
