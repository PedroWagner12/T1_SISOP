/* matriz.c - implementacao do modulo de matrizes.
 * Sistemas Operacionais - PUCRS - 2026/II
 * ANSI C (C89/C90).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matriz.h"

/* ------------------------------------------------------------------ */
/* Matrizes obrigatorias do enunciado (secao 7).                       */
/* Os inicializadores seguem o formato gerado pelo Editor de tabelas C */
/* ------------------------------------------------------------------ */

/* Exemplo 1 - 5 x 5 - 3 objetos */
static const unsigned char EX1[] = {
    1,1,0,0,0,
    1,1,0,0,0,
    0,0,0,1,0,
    0,0,0,1,0,
    1,0,0,0,0
};

/* Exemplo 2 - 6 x 8 - 4 objetos */
static const unsigned char EX2[] = {
    0,0,0,0,0,0,1,1,
    0,1,1,1,1,0,1,0,
    0,0,1,1,0,0,0,0,
    0,0,0,1,1,0,0,0,
    0,0,0,0,1,0,0,1,
    1,1,0,0,0,0,1,1
};

/* Exemplo 3 - 8 x 8 - 5 objetos */
static const unsigned char EX3[] = {
    1,1,0,0,0,0,0,0,
    1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,1,0,
    0,0,0,1,1,0,1,0,
    0,0,0,1,1,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,1,0,0,0,0,1,
    0,0,1,0,0,0,1,1
};

/* Exemplo 4 - 9 x 12 - 6 objetos */
static const unsigned char EX4[] = {
    0,1,1,0,0,0,0,0,0,0,1,0,
    0,0,1,1,1,1,0,0,0,1,1,0,
    0,0,0,0,0,1,0,0,0,0,0,0,
    0,0,0,0,0,1,1,0,0,0,0,0,
    0,1,0,0,0,0,1,0,0,1,0,0,
    0,1,1,0,0,0,0,0,1,1,0,0,
    0,0,1,1,0,0,0,0,1,0,0,0,
    0,0,0,1,0,0,0,1,1,0,0,0,
    0,0,0,0,0,1,0,0,0,0,0,1
};

/* Exemplo 5 - 12 x 12 - 7 objetos */
static const unsigned char EX5[] = {
    1,0,0,0,0,1,1,1,1,0,1,1,
    0,1,0,0,0,1,0,0,1,0,1,0,
    0,0,1,0,0,0,0,0,0,0,0,0,
    0,0,0,1,0,0,0,0,0,0,0,0,
    0,0,0,0,1,0,0,0,0,0,0,0,
    1,1,0,0,0,1,0,0,0,0,0,0,
    1,0,0,0,0,0,1,0,0,0,0,0,
    0,0,0,1,1,0,0,1,0,0,0,0,
    0,0,0,1,1,0,0,0,1,0,0,0,
    0,0,0,0,0,0,0,0,0,1,0,0,
    0,1,0,0,0,0,0,0,0,0,1,0,
    0,1,1,0,0,0,1,0,0,0,0,1
};

typedef struct {
    int linhas;
    int colunas;
    int esperado;
    const unsigned char *dados;
} MatrizFixa;

static const MatrizFixa FIXAS[5] = {
    {  5,  5, 3, EX1 },
    {  6,  8, 4, EX2 },
    {  8,  8, 5, EX3 },
    {  9, 12, 6, EX4 },
    { 12, 12, 7, EX5 }
};

/* ------------------------------------------------------------------ */

Matriz *matriz_criar(int linhas, int colunas)
{
    Matriz *m;
    long total;

    if (linhas <= 0 || colunas <= 0) {
        return NULL;
    }

    total = (long) linhas * (long) colunas;

    m = (Matriz *) malloc(sizeof(Matriz));
    if (m == NULL) {
        return NULL;
    }

    m->dados = (unsigned char *) calloc((size_t) total, sizeof(unsigned char));
    if (m->dados == NULL) {
        free(m);
        return NULL;
    }

    m->linhas = linhas;
    m->colunas = colunas;
    return m;
}

void matriz_liberar(Matriz *m)
{
    if (m != NULL) {
        free(m->dados);
        free(m);
    }
}

Matriz *matriz_obrigatoria(int numero)
{
    Matriz *m;
    long total;

    if (numero < 1 || numero > 5) {
        return NULL;
    }

    m = matriz_criar(FIXAS[numero - 1].linhas, FIXAS[numero - 1].colunas);
    if (m == NULL) {
        return NULL;
    }

    total = (long) m->linhas * (long) m->colunas;
    memcpy(m->dados, FIXAS[numero - 1].dados, (size_t) total);
    return m;
}

int matriz_obrigatoria_esperado(int numero)
{
    if (numero < 1 || numero > 5) {
        return -1;
    }
    return FIXAS[numero - 1].esperado;
}

Matriz *matriz_gerar(int linhas, int colunas, int densidade,
                     unsigned long semente)
{
    Matriz *m;
    long total, i;
    unsigned long estado;

    if (densidade < 0 || densidade > 100) {
        return NULL;
    }

    m = matriz_criar(linhas, colunas);
    if (m == NULL) {
        return NULL;
    }

    /* LCG classico (Numerical Recipes / ANSI C), mascarado em 32 bits
     * para que o resultado nao dependa do tamanho de unsigned long. */
    estado = semente;
    total = (long) linhas * (long) colunas;
    for (i = 0; i < total; i++) {
        estado = (estado * 1103515245UL + 12345UL) & 0xFFFFFFFFUL;
        if ((int) ((estado >> 16) % 100UL) < densidade) {
            m->dados[i] = 1;
        }
    }

    return m;
}

Matriz *matriz_ler_arquivo(const char *caminho)
{
    FILE *f;
    Matriz *m;
    int linhas, colunas, valor, c;
    long total, i;

    f = fopen(caminho, "r");
    if (f == NULL) {
        fprintf(stderr, "erro: nao foi possivel abrir '%s'\n", caminho);
        return NULL;
    }

    /* pula comentarios iniciados por '#' */
    for (;;) {
        c = fgetc(f);
        if (c == '#') {
            while (c != '\n' && c != EOF) {
                c = fgetc(f);
            }
        } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            continue;
        } else {
            if (c != EOF) {
                ungetc(c, f);
            }
            break;
        }
    }

    if (fscanf(f, "%d %d", &linhas, &colunas) != 2) {
        fprintf(stderr, "erro: cabecalho invalido em '%s'\n", caminho);
        fclose(f);
        return NULL;
    }

    m = matriz_criar(linhas, colunas);
    if (m == NULL) {
        fprintf(stderr, "erro: memoria insuficiente para %d x %d\n",
                linhas, colunas);
        fclose(f);
        return NULL;
    }

    total = (long) linhas * (long) colunas;
    for (i = 0; i < total; i++) {
        if (fscanf(f, "%d", &valor) != 1) {
            fprintf(stderr, "erro: faltam valores em '%s' (lidos %ld de %ld)\n",
                    caminho, i, total);
            matriz_liberar(m);
            fclose(f);
            return NULL;
        }
        m->dados[i] = (unsigned char) (valor != 0 ? 1 : 0);
    }

    fclose(f);
    return m;
}

int matriz_gravar_arquivo(const Matriz *m, const char *caminho)
{
    FILE *f;
    int i, j;

    if (m == NULL) {
        return -1;
    }

    f = fopen(caminho, "w");
    if (f == NULL) {
        fprintf(stderr, "erro: nao foi possivel gravar '%s'\n", caminho);
        return -1;
    }

    fprintf(f, "%d %d\n", m->linhas, m->colunas);
    for (i = 0; i < m->linhas; i++) {
        for (j = 0; j < m->colunas; j++) {
            fprintf(f, "%d%c", (int) MAT(m, i, j),
                    (j == m->colunas - 1) ? '\n' : ' ');
        }
    }

    if (fclose(f) != 0) {
        fprintf(stderr, "erro: falha ao fechar '%s'\n", caminho);
        return -1;
    }
    return 0;
}

void matriz_imprimir(const Matriz *m)
{
    int i, j;

    if (m == NULL) {
        return;
    }

    for (i = 0; i < m->linhas; i++) {
        for (j = 0; j < m->colunas; j++) {
            printf("%d%c", (int) MAT(m, i, j),
                   (j == m->colunas - 1) ? '\n' : ' ');
        }
    }
}
