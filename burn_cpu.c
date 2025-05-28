#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <math.h>

volatile int running = 1;

void* stress_cpu(void* arg) {
    double result = 0.0;
    while (running) {
        for (int i = 1; i < 10000000; ++i) {
            result += sin(i) * tan(i) * log(i + 1) / (cos(i) + 1.0001);
            result = fmod(result, 10000.0);  // mantiene resultado dentro de rango
        }
    }
    return NULL;
}

void stop_all(int signum) {
    running = 0;
}

int main() {
    int num_cores = get_nprocs();
    pthread_t* threads = malloc(sizeof(pthread_t) * num_cores);

    printf("🔥 Cargando CPU con %d hilos. Presiona Ctrl+C para detener...\n", num_cores);
    signal(SIGINT, stop_all);
    signal(SIGTERM, stop_all);

    for (int i = 0; i < num_cores; i++) {
        if (pthread_create(&threads[i], NULL, stress_cpu, NULL) != 0) {
            perror("Error al crear hilo");
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < num_cores; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("✅ Ejecución finalizada. CPU liberado.\n");

    free(threads);
    return 0;
}
