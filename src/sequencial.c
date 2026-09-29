/* sequencial.c - preenchimento por inundacao com conectividade 8.
 * Sistemas Operacionais - PUCRS - 2026/II
 * ANSI C (C89/C90).
 */

#include <stdio.h>
#include <stdlib.h>

#include "sequencial.h"

/* Deslocamentos dos 8 vizinhos (conectividade 8: arestas e cantos). */
static const int DL[8] = { -1, -1, -1,  0, 0,  1, 1, 1 };
static const int DC[8] = { -1,  0,  1, -1, 1, -1, 0, 1 };

long conta_objetos_sequencial(const Matriz *m)
{
    unsigned char *visitado;
    long *pilha;
    long topo, total, atual, vizinho, idx;
    long objetos;
    int linhas, colunas, i, j, k, li, cj, vl, vc;

    if (m == NULL) {
        return -1;
    }

    linhas = m->linhas;
    colunas = m->colunas;
    total = (long) linhas * (long) colunas;

    visitado = (unsigned char *) calloc((size_t) total, sizeof(unsigned char));
    if (visitado == NULL) {
        fprintf(stderr, "erro: memoria insuficiente (visitado)\n");
        return -1;
    }

    /* No pior caso todas as celulas entram na pilha uma unica vez,
     * porque cada celula e marcada antes de ser empilhada. */
    pilha = (long *) malloc((size_t) total * sizeof(long));
    if (pilha == NULL) {
        fprintf(stderr, "erro: memoria insuficiente (pilha)\n");
        free(visitado);
        return -1;
    }

    objetos = 0;

    for (i = 0; i < linhas; i++) {
        for (j = 0; j < colunas; j++) {
            idx = (long) i * (long) colunas + (long) j;

            if (m->dados[idx] == 0 || visitado[idx] != 0) {
                continue;
            }

            /* Celula semente: comeca um novo objeto. */
            objetos++;
            visitado[idx] = 1;
            topo = 0;
            pilha[topo++] = idx;

            while (topo > 0) {
                atual = pilha[--topo];
                li = (int) (atual / (long) colunas);
                cj = (int) (atual % (long) colunas);

                for (k = 0; k < 8; k++) {
                    vl = li + DL[k];
                    vc = cj + DC[k];

                    if (vl < 0 || vl >= linhas || vc < 0 || vc >= colunas) {
                        continue;
                    }

                    vizinho = (long) vl * (long) colunas + (long) vc;
                    if (m->dados[vizinho] != 0 && visitado[vizinho] == 0) {
                        visitado[vizinho] = 1;
                        pilha[topo++] = vizinho;
                    }
                }
            }
        }
    }

    free(pilha);
    free(visitado);
    return objetos;
}
