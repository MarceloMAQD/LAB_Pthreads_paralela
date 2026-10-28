#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define CATEGORIAS 10
#define REPETICIONES 3
#define MAX_THREADS 8

#define SIN_SINCRONIZACION 1
#define MUTEX_GLOBAL 2
#define MUTEX_POR_CATEGORIA 3
#define HISTOGRAMA_PRIVADO 4

/* datos de cada thread */
typedef struct {
    const int *arreglo;
    long inicio;
    long fin;
    long *histograma_compartido;
    long histograma_privado[CATEGORIAS];
    int estrategia;
} TareaThread;

static pthread_mutex_t mutex_global = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_categoria[CATEGORIAS];

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

/* version secuencial */
static void histograma_secuencial(const int *arreglo, long n, long *histograma) {
    memset(histograma, 0, CATEGORIAS * sizeof(long));
    for (long i = 0; i < n; i++) {
        int categoria = arreglo[i] / 10;
        histograma[categoria]++;
    }
}

/* ejecuta cada thread segun la estrategia */
static void *procesar_segmento(void *argumento) {
    TareaThread *tarea = (TareaThread *)argumento;

    for (long i = tarea->inicio; i < tarea->fin; i++) {
        int categoria = tarea->arreglo[i] / 10;

        switch (tarea->estrategia) {
        case SIN_SINCRONIZACION:
            tarea->histograma_compartido[categoria]++;
            break;
        case MUTEX_GLOBAL:
            pthread_mutex_lock(&mutex_global);
            tarea->histograma_compartido[categoria]++;
            pthread_mutex_unlock(&mutex_global);
            break;
        case MUTEX_POR_CATEGORIA:
            pthread_mutex_lock(&mutex_categoria[categoria]);
            tarea->histograma_compartido[categoria]++;
            pthread_mutex_unlock(&mutex_categoria[categoria]);
            break;
        case HISTOGRAMA_PRIVADO:
            tarea->histograma_privado[categoria]++;
            break;
        }
    }
    return NULL;
}

static void histograma_paralelo(const int *arreglo, long n, int T,
                                int estrategia, long *histograma) {
    pthread_t threads[MAX_THREADS];
    TareaThread tareas[MAX_THREADS];

    memset(histograma, 0, CATEGORIAS * sizeof(long));

    /* reparto del arreglo*/
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
        tareas[i].histograma_compartido = histograma;
        tareas[i].estrategia = estrategia;
        memset(tareas[i].histograma_privado, 0, sizeof(tareas[i].histograma_privado));
        posicion = posicion + tamano;
    }

    for (int i = 0; i < T; i++) {
        pthread_create(&threads[i], NULL, procesar_segmento, &tareas[i]);
    }
    for (int i = 0; i < T; i++) {
        pthread_join(threads[i], NULL);
    }

    /* histogramas privados se suman */
    if (estrategia == HISTOGRAMA_PRIVADO) {
        for (int i = 0; i < T; i++) {
            for (int c = 0; c < CATEGORIAS; c++) {
                histograma[c] += tareas[i].histograma_privado[c];
            }
        }
    }
}
static unsigned long long estado_aleatorio = 88172645463325252ULL;

static unsigned long long numero_aleatorio(void) {
    estado_aleatorio ^= estado_aleatorio << 13;
    estado_aleatorio ^= estado_aleatorio >> 7;
    estado_aleatorio ^= estado_aleatorio << 17;
    return estado_aleatorio;
}

static long sumar_categorias(const long *histograma) {
    long suma = 0;
    for (int c = 0; c < CATEGORIAS; c++) {
        suma += histograma[c];
    }
    return suma;
}

int main(int argc, char **argv) {
    long n = 10000000;
    if (argc > 1) {
        n = atol(argv[1]);
    }
    int cantidades_threads[] = {1, 2, 4, 8};
    const char *nombre_distribucion[] = {"uniforme", "sesgada", "constante"};
    const char *nombre_estrategia[] = {"", "sin sync", "mutex global",
                                       "mutex por categoria", "privado"};

    int *arreglo = malloc(n * sizeof(int));
    long hist_sec[CATEGORIAS];
    long hist_par[CATEGORIAS];

    for (int c = 0; c < CATEGORIAS; c++) {
        pthread_mutex_init(&mutex_categoria[c], NULL);
    }

    printf("PARTE II: HISTOGRAMA (N=%ld, REPS=%d, tiempos promedio en s) \n\n",
           n, REPETICIONES);

    for (int d = 0; d < 3; d++) {
        for (long i = 0; i < n; i++) {
            if (d == 0) {
                arreglo[i] = (int)(numero_aleatorio() % 100);
            } else if (d == 1) {
                if (numero_aleatorio() % 100 < 90) {
                    arreglo[i] = (int)(numero_aleatorio() % 10);
                } else {
                    arreglo[i] = (int)(numero_aleatorio() % 100);
                }
            } else {
                arreglo[i] = 55;
            }
        }

        /* medicion de la version secuencial */
        double tiempo_sec = 0;
        for (int r = 0; r < REPETICIONES; r++) {
            double inicio = tiempo_actual();
            histograma_secuencial(arreglo, n, hist_sec);
            tiempo_sec += tiempo_actual() - inicio;
        }
        tiempo_sec = tiempo_sec / REPETICIONES;

        printf("sistribucion: %s \n", nombre_distribucion[d]);
        printf("histograma secuencial: ");
        for (int c = 0; c < CATEGORIAS; c++) {
            printf("%ld ", hist_sec[c]);
        }
        printf("\nsuma categorias = %ld (N = %ld)  tiempo secuencial = %.6f s\n\n",
               sumar_categorias(hist_sec), n, tiempo_sec);
        printf("%-20s %-8s %-12s %-9s %-10s %-10s %-12s\n", "estrategia", "threads",
               "tiempo(s)", "speedup", "eficiencia", "suma=N", "igual a seq");

        /* medicion de cada estrategia paralela */
        for (int e = 1; e <= 4; e++) {
            for (int k = 0; k < 4; k++) {
                int T = cantidades_threads[k];
                double tiempo_par = 0;

                for (int r = 0; r < REPETICIONES; r++) {
                    double inicio = tiempo_actual();
                    histograma_paralelo(arreglo, n, T, e, hist_par);
                    tiempo_par += tiempo_actual() - inicio;
                }
                tiempo_par = tiempo_par / REPETICIONES;

                long suma = sumar_categorias(hist_par);
                int suma_correcta = 0;
                if (suma == n) {
                    suma_correcta = 1;
                }
                int igual_secuencial = 0;
                if (memcmp(hist_sec, hist_par, sizeof(hist_sec)) == 0) {
                    igual_secuencial = 1;
                }

                double speedup = tiempo_sec / tiempo_par;
                double eficiencia = speedup / T;
                printf("%-20s %-8d %-12.6f %-9.3f %-10.3f %-10s %-12s",
                       nombre_estrategia[e], T, tiempo_par, speedup, eficiencia,
                       texto_si_no(suma_correcta), texto_si_no(igual_secuencial));
                if (e == SIN_SINCRONIZACION) {
                    printf(" (suma=%ld, perdidos=%ld)", suma, n - suma);
                }
                printf("\n");
            }
        }
        printf("\n");
    }
    free(arreglo);
    return 0;
}
