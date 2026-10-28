# Laboratorio de Pthreads

**Curso:** Computación Paralela y Distribuida
**Universidad:** Universidad Nacional de San Agustín de Arequipa
**Alumno:** Quina Delgado, Marcelo Adrian
**Docente:** Mamani Aliaga, Alvaro Henry

Tres programas en C con Pthreads. En cada uno hice primero la versión secuencial y después la paralela, y comprobé que den los mismos resultados.

- `parte1.c`: suma, mínimo, máximo y promedio de un arreglo, dividiendo el arreglo entre threads.
- `parte2.c`: histograma de 10 categorías con cuatro estrategias (sin sincronización, mutex global, mutex por categoría e histograma privado por thread).
- `parte3.c`: productores y consumidores con un buffer circular de tamaño fijo, usando mutex y variables de condición.

## Cómo compilar y ejecutar

```bash
gcc -O2 -pthread parte1.c -o parte1_stats && ./parte1_stats
gcc -O2 -pthread parte2.c -o parte2_hist && ./parte2_hist
gcc -O2 -pthread parte3.c -o parte3_prodcons && ./parte3_prodcons
```
