/* tempo.h - medicao de tempo de parede (wall clock).
 * Sistemas Operacionais - PUCRS - 2026/II
 */

#ifndef TEMPO_H
#define TEMPO_H

/* Instante atual, em segundos, a partir de um relogio monotonico.
 * clock() da biblioteca padrao nao serve aqui: ele mede tempo de CPU
 * e, na versao paralela, somaria o tempo de todas as threads. */
double tempo_agora(void);

/* Mediana de um vetor de tempos (o vetor e ordenado no processo). */
double tempo_mediana(double *v, int n);

/* Menor valor de um vetor de tempos. */
double tempo_minimo(const double *v, int n);

#endif /* TEMPO_H */
