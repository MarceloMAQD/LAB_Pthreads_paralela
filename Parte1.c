#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define REPETICIONES 5
#define MAX_THREADS 64

typedef struct {
    const int *arreglo;
    long inicio;
    long fin;
    long long suma_parcial;
    int minimo_parcial;
    int maximo_parcial;
} TareaThread;

static double tiempo_actual(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static const char *texto_si_no(int valor) {
    if (valor == 1) {
        return "SI";
    }
    return "NO";
}

static void estadisticas_secuencial(const int *arreglo, long n,
                                    long long *suma, int *minimo, int *maximo) {
    long long suma_local = 0;
    int minimo_local = arreglo[0];
    int maximo_local = arreglo[0];

    for (long i = 0; i < n; i++) {
        suma_local += arreglo[i];
        if (arreglo[i] < minimo_local) {
            minimo_local = arreglo[i];
        }
        if (arreglo[i] > maximo_local) {
            maximo_local = arreglo[i];
        }
    }
    *suma = suma_local;
    *minimo = minimo_local;
    *maximo = maximo_local;
}

static void *calcular_segmento(void *argumento) {
    TareaThread *tarea = (TareaThread *)argumento;
    long long suma_local = 0;
    int minimo_local = tarea->arreglo[tarea->inicio];
    int maximo_local = tarea->arreglo[tarea->inicio];

    for (long i = tarea->inicio; i < tarea->fin; i++) {
        suma_local += tarea->arreglo[i];
        if (tarea->arreglo[i] < minimo_local) {
            minimo_local = tarea->arreglo[i];
        }
        if (tarea->arreglo[i] > maximo_local) {
            maximo_local = tarea->arreglo[i];
        }
    }
    tarea->suma_parcial = suma_local;
    tarea->minimo_parcial = minimo_local;
    tarea->maximo_parcial = maximo_local;
    return NULL;
}

static void repartir_trabajo(long n, int T, TareaThread *tareas, const int *arreglo) {
    long tamano_base = n / T;
    long sobrante = n % T;
    long posicion = 0;

    for (int i = 0; i < T; i++) {
        long tamano = tamano_base;
        if (i < sobrante) {
            tamano = tamano_base + 1;
        }
        tareas[i].arreglo = arreglo;
        tareas[i].inicio = posicion;
        tareas[i].fin = posicion + tamano;
        posicion = posicion + tamano;
    }
}

static void estadisticas_paralela(const int *arreglo, long n, int T,
                                  long long *suma, int *minimo, int *maximo) {
    pthread_t threads[MAX_THREADS];
    TareaThread tareas[MAX_THREADS];

    repartir_trabajo(n, T, tareas, arreglo);

    for (int i = 0; i < T; i++) {
        pthread_create(&threads[i], NULL, calcular_segmento, &tareas[i]);
    }
    for (int i = 0; i < T; i++) {
        pthread_join(threads[i], NULL);
    }

    long long suma_total = 0;
    int minimo_global = tareas[0].minimo_parcial;
    int maximo_global = tareas[0].maximo_parcial;

    for (int i = 0; i < T; i++) {
        suma_total += tareas[i].suma_parcial;
        if (tareas[i].minimo_parcial < minimo_global) {
            minimo_global = tareas[i].minimo_parcial;
        }
        if (tareas[i].maximo_parcial > maximo_global) {
            maximo_global = tareas[i].maximo_parcial;
        }
    }
    *suma = suma_total;
    *minimo = minimo_global;
    *maximo = maximo_global;
}

static unsigned long long estado_aleatorio = 88172645463325252ULL;

static unsigned long long numero_aleatorio(void) {
    estado_aleatorio ^= estado_aleatorio << 13;
    estado_aleatorio ^= estado_aleatorio >> 7;
    estado_aleatorio ^= estado_aleatorio << 17;
    return estado_aleatorio;
}

static void mostrar_reparto(long n, int T) {
    TareaThread tareas[MAX_THREADS];
    repartir_trabajo(n, T, tareas, NULL);

    printf("Reparto N=%ld con %d threads (N %% T = %ld):\n", n, T, n % T);
    for (int i = 0; i < T; i++) {
        printf("  thread %d: [%ld, %ld) tamano %ld\n", i, tareas[i].inicio,
               tareas[i].fin, tareas[i].fin - tareas[i].inicio);
    }
}

int main(int argc, char **argv) {
    long tamanos_por_defecto[] = {1000000, 10000000, 100000000};
    long tamanos[16];
    int cantidad_tamanos = 0;

    if (argc > 1) {
        for (int i = 1; i < argc && cantidad_tamanos < 16; i++) {
            tamanos[cantidad_tamanos] = atol(argv[i]);
            cantidad_tamanos++;
        }
    } else {
        for (int i = 0; i < 3; i++) {
            tamanos[cantidad_tamanos] = tamanos_por_defecto[i];
            cantidad_tamanos++;
        }
    }
    int cantidades_threads[] = {1, 2, 4, 8};

    printf("=== PARTE I: ESTADISTICAS (REPS=%d, tiempos promedio en segundos) ===\n\n",
           REPETICIONES);
    mostrar_reparto(1000003, 8);
    printf("\n");

    for (int t = 0; t < cantidad_tamanos; t++) {
        long n = tamanos[t];
        int *arreglo = malloc(n * sizeof(int));
        if (arreglo == NULL) {
            printf("sin memoria para N=%ld\n", n);
            continue;
        }

        for (long i = 0; i < n; i++) {
            arreglo[i] = (int)(numero_aleatorio() % 2000001ULL) - 1000000;
        }

        long long suma_sec;
        int minimo_sec, maximo_sec;
        double tiempo_sec = 0;
        for (int r = 0; r < REPETICIONES; r++) {
            double inicio = tiempo_actual();
            estadisticas_secuencial(arreglo, n, &suma_sec, &minimo_sec, &maximo_sec);
            tiempo_sec += tiempo_actual() - inicio;
        }
        tiempo_sec = tiempo_sec / REPETICIONES;

        double promedio_sec = (double)suma_sec / n;

        printf("N = %ld\n", n);
        printf("Secuencial: suma=%lld min=%d max=%d promedio=%.6f tiempo=%.6f s\n",
               suma_sec, minimo_sec, maximo_sec, promedio_sec, tiempo_sec);
        printf("%-8s %-12s %-10s %-10s %-8s\n",
               "threads", "t_par(s)", "speedup", "eficiencia", "correcto");

        for (int k = 0; k < 4; k++) {
            int T = cantidades_threads[k];
            long long suma_par;
            int minimo_par, maximo_par;
            double tiempo_par = 0;

            for (int r = 0; r < REPETICIONES; r++) {
                double inicio = tiempo_actual();
                estadisticas_paralela(arreglo, n, T, &suma_par, &minimo_par, &maximo_par);
                tiempo_par += tiempo_actual() - inicio;
            }
            tiempo_par = tiempo_par / REPETICIONES;

            double promedio_par = (double)suma_par / n;

            int correcto = 0;
            if (suma_par == suma_sec && minimo_par == minimo_sec &&
                maximo_par == maximo_sec && promedio_par == promedio_sec) {
                correcto = 1;
            }
            double speedup = tiempo_sec / tiempo_par;
            double eficiencia = speedup / T;
            printf("%-8d %-12.6f %-10.3f %-10.3f %-8s\n",
                   T, tiempo_par, speedup, eficiencia, texto_si_no(correcto));
        }
        printf("\n");
        free(arreglo);
    }
    return 0;
}
