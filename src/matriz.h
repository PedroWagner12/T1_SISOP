/* matriz.h - representacao e criacao das matrizes binarias.
 * Sistemas Operacionais - PUCRS - 2026/II
 * ANSI C (C89/C90).
 */

#ifndef MATRIZ_H
#define MATRIZ_H

/* Matriz binaria armazenada de forma linear (row-major).
 * dados[i * colunas + j] vale 0 (fundo) ou 1 (primeiro plano). */
typedef struct {
    int linhas;
    int colunas;
    unsigned char *dados;
} Matriz;

/* Acesso a uma celula. O cast para long evita estouro do indice
 * em matrizes grandes (linhas * colunas pode passar de 2^31). */
#define MAT(m, i, j) ((m)->dados[(long)(i) * (long)((m)->colunas) + (long)(j)])

/* Aloca uma matriz zerada. Retorna NULL em caso de falha. */
Matriz *matriz_criar(int linhas, int colunas);

/* Libera a matriz (aceita NULL). */
void matriz_liberar(Matriz *m);

/* Matrizes obrigatorias do enunciado, numeradas de 1 a 5.
 * Retorna NULL se o numero for invalido ou faltar memoria. */
Matriz *matriz_obrigatoria(int numero);

/* Quantidade de objetos esperada para a matriz obrigatoria informada.
 * Retorna -1 se o numero for invalido. */
int matriz_obrigatoria_esperado(int numero);

/* Gera uma matriz pseudoaleatoria reproduzivel.
 * densidade: percentual (0 a 100) de celulas com valor 1.
 * O gerador e um LCG proprio, portanto a mesma semente produz
 * exatamente a mesma matriz em qualquer maquina/compilador. */
Matriz *matriz_gerar(int linhas, int colunas, int densidade,
                     unsigned long semente);

/* Le uma matriz de arquivo texto. Formato:
 *   linha 1: <linhas> <colunas>
 *   demais : os valores 0/1 separados por espaco ou quebra de linha.
 * Linhas iniciadas por '#' sao ignoradas. */
Matriz *matriz_ler_arquivo(const char *caminho);

/* Grava a matriz no formato acima. Retorna 0 em caso de sucesso. */
int matriz_gravar_arquivo(const Matriz *m, const char *caminho);

/* Imprime a matriz na saida padrao (uso didatico, matrizes pequenas). */
void matriz_imprimir(const Matriz *m);

#endif /* MATRIZ_H */
