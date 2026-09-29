/* conta-objetos-sequencial.c - versao de referencia, um unico fluxo
 * de controle.
 *
 * Sistemas Operacionais - PUCRS - 2026/II
 * Prof. Filipo Novo Mor
 * ANSI C (C89/C90).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matriz.h"
#include "sequencial.h"
#include "tempo.h"

#define MODO_OBRIGATORIAS 0
#define MODO_ARQUIVO      1
#define MODO_GERADA       2
#define MODO_UMA_FIXA     3

#define MAX_REPETICOES 100

static void uso(const char *prog)
{
    printf("Uso: %s [opcoes]\n", prog);
    printf("  (sem opcoes)   executa as 5 matrizes obrigatorias\n");
    printf("  -m N           executa apenas a matriz obrigatoria N (1..5)\n");
    printf("  -a ARQUIVO     le a matriz de um arquivo texto\n");
    printf("  -g L C D S     gera matriz L x C, densidade D%% e semente S\n");
    printf("  -r REP         repete a medicao REP vezes (padrao 1)\n");
    printf("  -o ARQUIVO     grava a matriz usada em um arquivo texto\n");
    printf("  -p             imprime a matriz antes de contar\n");
    printf("  -h             mostra esta ajuda\n");
}

/* Executa a bateria obrigatoria (secao 7 do enunciado). */
static int executa_obrigatorias(int imprimir)
{
    int n, esperado, falhas;
    long obtidos;
    double t0, t1;
    Matriz *m;

    falhas = 0;

    printf("Versao sequencial - conectividade 8\n\n");
    printf("Ex  Dimensoes   Esperado  Obtido  Situacao  Tempo (s)\n");

    for (n = 1; n <= 5; n++) {
        m = matriz_obrigatoria(n);
        if (m == NULL) {
            fprintf(stderr, "erro: falha ao montar a matriz %d\n", n);
            return -1;
        }

        if (imprimir) {
            printf("\nMatriz %d:\n", n);
            matriz_imprimir(m);
            printf("\n");
        }

        esperado = matriz_obrigatoria_esperado(n);

        t0 = tempo_agora();
        obtidos = conta_objetos_sequencial(m);
        t1 = tempo_agora();

        if (obtidos < 0) {
            matriz_liberar(m);
            return -1;
        }

        if (obtidos != (long) esperado) {
            falhas++;
        }

        printf("%2d  %3d x %-5d %8d  %6ld  %-8s  %.6f\n",
               n, m->linhas, m->colunas, esperado, obtidos,
               (obtidos == (long) esperado) ? "OK" : "FALHOU",
               t1 - t0);

        matriz_liberar(m);
    }

    printf("\n");
    if (falhas == 0) {
        printf("Todos os 5 casos obrigatorios conferem.\n");
    } else {
        printf("%d caso(s) divergiram do esperado.\n", falhas);
    }

    return falhas;
}

/* Executa uma unica matriz, com repeticoes de medicao. */
static int executa_matriz(Matriz *m, int repeticoes, int imprimir,
                          int esperado)
{
    double tempos[MAX_REPETICOES];
    long objetos, anterior;
    double t0, t1;
    int i;

    if (imprimir) {
        matriz_imprimir(m);
        printf("\n");
    }

    anterior = -1;
    objetos = -1;

    for (i = 0; i < repeticoes; i++) {
        t0 = tempo_agora();
        objetos = conta_objetos_sequencial(m);
        t1 = tempo_agora();

        if (objetos < 0) {
            return -1;
        }
        if (anterior >= 0 && objetos != anterior) {
            fprintf(stderr, "erro: resultado nao deterministico\n");
            return -1;
        }
        anterior = objetos;
        tempos[i] = t1 - t0;
    }

    printf("Matriz .............: %d x %d (%ld celulas)\n",
           m->linhas, m->colunas,
           (long) m->linhas * (long) m->colunas);
    printf("Objetos encontrados : %ld\n", objetos);
    if (esperado >= 0) {
        printf("Esperado ...........: %d (%s)\n", esperado,
               (objetos == (long) esperado) ? "OK" : "FALHOU");
    }
    printf("Repeticoes .........: %d\n", repeticoes);
    printf("Tempo mediano ......: %.6f s\n",
           tempo_mediana(tempos, repeticoes));
    printf("Tempo minimo .......: %.6f s\n",
           tempo_minimo(tempos, repeticoes));

    return (esperado >= 0 && objetos != (long) esperado) ? 1 : 0;
}

int main(int argc, char *argv[])
{
    int modo = MODO_OBRIGATORIAS;
    int repeticoes = 1;
    int imprimir = 0;
    int numero_fixa = 0;
    int linhas = 0, colunas = 0, densidade = 0;
    unsigned long semente = 0;
    const char *arquivo_entrada = NULL;
    const char *arquivo_saida = NULL;
    Matriz *m = NULL;
    int i, esperado, resultado;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            uso(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-p") == 0) {
            imprimir = 1;
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            numero_fixa = atoi(argv[++i]);
            modo = MODO_UMA_FIXA;
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            arquivo_entrada = argv[++i];
            modo = MODO_ARQUIVO;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            arquivo_saida = argv[++i];
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            repeticoes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-g") == 0 && i + 4 < argc) {
            linhas = atoi(argv[++i]);
            colunas = atoi(argv[++i]);
            densidade = atoi(argv[++i]);
            semente = strtoul(argv[++i], NULL, 10);
            modo = MODO_GERADA;
        } else {
            fprintf(stderr, "erro: argumento invalido '%s'\n", argv[i]);
            uso(argv[0]);
            return 1;
        }
    }

    if (repeticoes < 1 || repeticoes > MAX_REPETICOES) {
        fprintf(stderr, "erro: repeticoes deve ficar entre 1 e %d\n",
                MAX_REPETICOES);
        return 1;
    }

    if (modo == MODO_OBRIGATORIAS) {
        resultado = executa_obrigatorias(imprimir);
        return (resultado == 0) ? 0 : 1;
    }

    esperado = -1;

    if (modo == MODO_UMA_FIXA) {
        m = matriz_obrigatoria(numero_fixa);
        if (m == NULL) {
            fprintf(stderr, "erro: matriz obrigatoria deve estar entre 1 e 5\n");
            return 1;
        }
        esperado = matriz_obrigatoria_esperado(numero_fixa);
        printf("Fonte ..............: matriz obrigatoria %d\n", numero_fixa);
    } else if (modo == MODO_ARQUIVO) {
        m = matriz_ler_arquivo(arquivo_entrada);
        if (m == NULL) {
            return 1;
        }
        printf("Fonte ..............: arquivo %s\n", arquivo_entrada);
    } else {
        m = matriz_gerar(linhas, colunas, densidade, semente);
        if (m == NULL) {
            fprintf(stderr, "erro: nao foi possivel gerar a matriz\n");
            return 1;
        }
        printf("Fonte ..............: gerada (densidade %d%%, semente %lu)\n",
               densidade, semente);
    }

    if (arquivo_saida != NULL) {
        if (matriz_gravar_arquivo(m, arquivo_saida) == 0) {
            printf("Matriz gravada em ..: %s\n", arquivo_saida);
        }
    }

    resultado = executa_matriz(m, repeticoes, imprimir, esperado);
    matriz_liberar(m);

    return (resultado == 0) ? 0 : 1;
}
