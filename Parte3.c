#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define MAX_THREADS 64

/* datos compartidos */
typedef struct {
    int *buffer;
    int capacidad;
    int indice_insercion;
    int indice_extraccion;
    int cantidad;               /* elementos actualmente en el buffer */
    long total_a_producir;
    long total_consumido;
    int max_ocupacion;
    int *veces_consumido;       /* cuantas veces salio el valor v */
    long long suma_consumida;
    pthread_mutex_t mutex;
    pthread_cond_t no_lleno;
    pthread_cond_t no_vacio;
} DatosCompartidos;

/* datos de cada thread */
typedef struct {
    DatosCompartidos *compartidos;
    int id;
    long cantidad_items;     
    long procesados;
    long long suma;
} ArgumentoThread;

static double tiempo_actual(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void *productor(void *puntero) {
    ArgumentoThread *arg = (ArgumentoThread *)puntero;
    DatosCompartidos *datos = arg->compartidos;

    for (long j = 0; j < arg->cantidad_items; j++) {
        int valor = (int)(arg->id * arg->cantidad_items + j);

        pthread_mutex_lock(&datos->mutex);
        while (datos->cantidad == datos->capacidad) {
            pthread_cond_wait(&datos->no_lleno, &datos->mutex);
        }
        datos->buffer[datos->indice_insercion] = valor;
        datos->indice_insercion = (datos->indice_insercion + 1) % datos->capacidad;
        datos->cantidad++;
        if (datos->cantidad > datos->max_ocupacion) {
            datos->max_ocupacion = datos->cantidad;
        }
        pthread_cond_signal(&datos->no_vacio);
        pthread_mutex_unlock(&datos->mutex);

        arg->procesados++;
        arg->suma += valor;
    }
    return NULL;
}

static void *consumidor(void *puntero) {
    ArgumentoThread *arg = (ArgumentoThread *)puntero;
    DatosCompartidos *datos = arg->compartidos;

    while (1) {
        pthread_mutex_lock(&datos->mutex);
        while (datos->cantidad == 0 && datos->total_consumido < datos->total_a_producir) {
            pthread_cond_wait(&datos->no_vacio, &datos->mutex);
        }
        if (datos->cantidad == 0) {
            pthread_mutex_unlock(&datos->mutex);
            break;
        }
        int valor = datos->buffer[datos->indice_extraccion];
        datos->indice_extraccion = (datos->indice_extraccion + 1) % datos->capacidad;
        datos->cantidad--;
        datos->total_consumido++;
        datos->veces_consumido[valor]++;
        datos->suma_consumida += valor;
        pthread_cond_signal(&datos->no_lleno);
        if (datos->total_consumido == datos->total_a_producir) {
            pthread_cond_broadcast(&datos->no_vacio);
        }
        pthread_mutex_unlock(&datos->mutex);

        arg->procesados++;
    }
    return NULL;
}

static int ejecutar_prueba(int P, int C, int B, long items, int detallado) {
    DatosCompartidos datos;
    memset(&datos, 0, sizeof(datos));
    datos.capacidad = B;
    datos.buffer = malloc(B * sizeof(int));
    datos.total_a_producir = (long)P * items;
    datos.veces_consumido = calloc(datos.total_a_producir, sizeof(int));
    pthread_mutex_init(&datos.mutex, NULL);
    pthread_cond_init(&datos.no_lleno, NULL);
    pthread_cond_init(&datos.no_vacio, NULL);

    pthread_t threads_productores[MAX_THREADS];
    pthread_t threads_consumidores[MAX_THREADS];
    ArgumentoThread args_productores[MAX_THREADS];
    ArgumentoThread args_consumidores[MAX_THREADS];

    double inicio = tiempo_actual();

    for (int i = 0; i < C; i++) {
        args_consumidores[i].compartidos = &datos;
        args_consumidores[i].id = i;
        args_consumidores[i].cantidad_items = 0;
        args_consumidores[i].procesados = 0;
        args_consumidores[i].suma = 0;
        pthread_create(&threads_consumidores[i], NULL, consumidor, &args_consumidores[i]);
    }
    for (int i = 0; i < P; i++) {
        args_productores[i].compartidos = &datos;
        args_productores[i].id = i;
        args_productores[i].cantidad_items = items;
        args_productores[i].procesados = 0;
        args_productores[i].suma = 0;
        pthread_create(&threads_productores[i], NULL, productor, &args_productores[i]);
    }
    for (int i = 0; i < P; i++) {
        pthread_join(threads_productores[i], NULL);
    }
    for (int i = 0; i < C; i++) {
        pthread_join(threads_consumidores[i], NULL);
    }
    double tiempo = tiempo_actual() - inicio;

    long producidos = 0;
    long consumidos = 0;
    long long suma_producida = 0;
    for (int i = 0; i < P; i++) {
        producidos += args_productores[i].procesados;
        suma_producida += args_productores[i].suma;
    }
    for (int i = 0; i < C; i++) {
        consumidos += args_consumidores[i].procesados;
    }

    long no_exactos = 0;
    for (long v = 0; v < datos.total_a_producir; v++) {
        if (datos.veces_consumido[v] != 1) {
            no_exactos++;
        }
    }

    int correcto = 1;
    if (producidos != consumidos) {
        correcto = 0;
    }
    if (producidos != datos.total_a_producir) {
        correcto = 0;
    }
    if (no_exactos != 0) {
        correcto = 0;
    }
    if (suma_producida != datos.suma_consumida) {
        correcto = 0;
    }
    if (datos.cantidad != 0) {
        correcto = 0;
    }

    const char *texto_resultado = "ERROR";
    if (correcto == 1) {
        texto_resultado = "CORRECTO";
    }
    printf("P=%d C=%d B=%d items/prod=%ld | producidos=%ld consumidos=%ld | "
           "suma prod=%lld suma cons=%lld | no exactos=%ld | resto buffer=%d | "
           "max ocupacion=%d | tiempo=%.4f s | %s\n",
           P, C, B, items, producidos, consumidos, suma_producida,
           datos.suma_consumida, no_exactos, datos.cantidad, datos.max_ocupacion,
           tiempo, texto_resultado);

    if (detallado == 1) {
        printf("  por productor: ");
        for (int i = 0; i < P; i++) {
            printf("%ld ", args_productores[i].procesados);
        }
        printf("\n  por consumidor: ");
        for (int i = 0; i < C; i++) {
            printf("%ld ", args_consumidores[i].procesados);
        }
        printf("\n");
    }
    free(datos.buffer);
    free(datos.veces_consumido);
    return correcto;
}

int main(int argc, char **argv) {
    printf("=== PARTE III: PRODUCTORES Y CONSUMIDORES ===\n\n");

    if (argc == 5) {
        ejecutar_prueba(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atol(argv[4]), 1);
        return 0;
    }

    long items_por_productor = 100000;
    int configuraciones[][3] = {
        {2, 2, 1}, {2, 2, 8}, {2, 2, 64},
        {4, 4, 8}, {4, 4, 64},
        {1, 4, 16}, {4, 1, 16},
        {8, 8, 32}, {2, 2, 1024}
    };
    int cantidad_pruebas = sizeof(configuraciones) / sizeof(configuraciones[0]);
    int todas_correctas = 1;

    for (int i = 0; i < cantidad_pruebas; i++) {
        int resultado = ejecutar_prueba(configuraciones[i][0], configuraciones[i][1],
                                        configuraciones[i][2], items_por_productor, 1);
        if (resultado == 0) {
            todas_correctas = 0;
        }
    }

    if (todas_correctas == 1) {
        printf("\nResultado global: TODAS LAS PRUEBAS CORRECTAS\n");
    } else {
        printf("\nResultado global: HAY ERRORES\n");
    }
    return 0;
}
