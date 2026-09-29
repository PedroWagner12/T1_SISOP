/* tempo.c - implementacao da medicao de tempo.
 * Sistemas Operacionais - PUCRS - 2026/II
 *
 * _POSIX_C_SOURCE precisa ser definido antes de qualquer include para
 * que clock_gettime() e CLOCK_MONOTONIC fiquem visiveis quando o
 * compilador roda em modo estrito (-std=c89 -pedantic).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "tempo.h"

double tempo_agora(void)
{
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
    }
#endif
    {
        /* Alternativa para sistemas sem CLOCK_MONOTONIC. */
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (double) tv.tv_sec + (double) tv.tv_usec / 1e6;
    }
}

static int compara_double(const void *a, const void *b)
{
    double x = *(const double *) a;
    double y = *(const double *) b;

    if (x < y) {
        return -1;
    }
    if (x > y) {
        return 1;
    }
    return 0;
}

double tempo_mediana(double *v, int n)
{
    if (v == NULL || n <= 0) {
        return 0.0;
    }

    qsort(v, (size_t) n, sizeof(double), compara_double);

    if (n % 2 == 1) {
        return v[n / 2];
    }
    return (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

double tempo_minimo(const double *v, int n)
{
    int i;
    double menor;

    if (v == NULL || n <= 0) {
        return 0.0;
    }

    menor = v[0];
    for (i = 1; i < n; i++) {
        if (v[i] < menor) {
            menor = v[i];
        }
    }
    return menor;
}
