/* conta-objetos-paralelo.c - contagem de objetos com conectividade 8
 * usando Pthreads (POSIX Threads).
 *
 * Estrategia:
 *   1. Decomposicao por faixas de linhas: a matriz e dividida em T
 *      faixas contiguas, uma por thread. Cada thread rotula apenas as
 *      celulas das suas linhas e nunca olha para fora da faixa.
 *   2. Cada componente local recebe como rotulo o indice linear da sua
 *      celula semente somado de 1. Como a semente pertence a uma unica
 *      faixa, o rotulo e globalmente unico sem precisar de contador
 *      compartilhado (nao ha regiao critica na rotulagem).
 *   3. Consolidacao: as faixas vizinhas sao costuradas com uma estrutura
 *      union-find compartilhada. Para cada celula 1 na primeira linha de
 *      uma faixa, olham-se os tres vizinhos da ultima linha da faixa de
 *      cima (diagonal esquerda, vertical e diagonal direita). Cada uniao
 *      ocorre dentro de uma regiao critica protegida por mutex.
 *   4. Contagem final: um objeto por rotulo que e raiz da sua classe.
 *
 * Sistemas Operacionais - PUCRS - 2026/II
 * Prof. Filipo Novo Mor
 * ANSI C (C89/C90) + POSIX Threads.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "matriz.h"
#include "sequencial.h"
#include "tempo.h"

#define MODO_OBRIGATORIAS 0
#define MODO_ARQUIVO      1
#define MODO_GERADA       2
#define MODO_UMA_FIXA     3

#define MAX_REPETICOES 100
#define MAX_THREADS    256
#define CAP_SEMENTES_INICIAL 256

/* Deslocamentos dos 8 vizinhos (conectividade 8). */
static const int DL[8] = { -1, -1, -1,  0, 0,  1, 1, 1 };
static const int DC[8] = { -1,  0,  1, -1, 1, -1, 0, 1 };

typedef struct {
    long objetos;             /* total consolidado */
    long componentes_locais;  /* somatorio das componentes por faixa */
    long unioes_fronteira;    /* fusoes efetivas nas fronteiras */
    int  threads;
    double t_rotulagem;       /* fase paralela 1 */
    double t_fusao;           /* fase paralela 2 */
    double t_contagem;        /* fase sequencial */
    double t_total;
} Resultado;

typedef struct {
    int id;
    const Matriz *m;
    int linha_ini;            /* inclusivo */
    int linha_fim;            /* exclusivo */
    int *rotulos;             /* compartilhado; cada thread so escreve nas suas linhas */
    int *pai;                 /* union-find compartilhado */
    pthread_mutex_t *mutex;   /* protege o union-find na consolidacao */
    long *pilha;              /* pilha privada do flood fill */
    long *sementes;           /* indices das celulas semente da faixa */
    long n_sementes;
    long cap_sementes;
    long unioes;
    int erro;
} Tarefa;

/* ------------------------------------------------------------------ */
/* Union-find (conjuntos disjuntos) com compressao de caminho.        */
/* A raiz e sempre o menor rotulo da classe, o que torna o resultado  */
/* independente da ordem em que as fusoes acontecem.                  */
/* ------------------------------------------------------------------ */

static int uf_raiz(int *pai, int x)
{
    int raiz = x;
    int proximo;

    while (pai[raiz] != raiz) {
        raiz = pai[raiz];
    }
    while (pai[x] != raiz) {
        proximo = pai[x];
        pai[x] = raiz;
        x = proximo;
    }
    return raiz;
}

/* Retorna 1 se houve fusao efetiva, 0 se ja eram do mesmo objeto. */
static int uf_unir(int *pai, int a, int b)
{
    a = uf_raiz(pai, a);
    b = uf_raiz(pai, b);

    if (a == b) {
        return 0;
    }
    if (a < b) {
        pai[b] = a;
    } else {
        pai[a] = b;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Fase 1 (paralela): rotulagem local de cada faixa.                   */
/* ------------------------------------------------------------------ */

static void *fase_rotular(void *arg)
{
    Tarefa *t = (Tarefa *) arg;
    const Matriz *m = t->m;
    int colunas = m->colunas;
    int i, j, k, li, cj, vl, vc;
    int rotulo;
    long idx, atual, viz, topo;
    long *novo;

    for (i = t->linha_ini; i < t->linha_fim; i++) {
        for (j = 0; j < colunas; j++) {
            idx = (long) i * (long) colunas + (long) j;

            if (m->dados[idx] == 0 || t->rotulos[idx] != 0) {
                continue;
            }

            /* Nova componente local: a propria semente da o rotulo. */
            rotulo = (int) (idx + 1);

            if (t->n_sementes == t->cap_sementes) {
                novo = (long *) realloc(t->sementes,
                        (size_t) (t->cap_sementes * 2) * sizeof(long));
                if (novo == NULL) {
                    t->erro = 1;
                    return NULL;
                }
                t->sementes = novo;
                t->cap_sementes *= 2;
            }
            t->sementes[t->n_sementes++] = idx;

            t->pai[rotulo] = rotulo;
            t->rotulos[idx] = rotulo;

            topo = 0;
            t->pilha[topo++] = idx;

            while (topo > 0) {
                atual = t->pilha[--topo];
                li = (int) (atual / (long) colunas);
                cj = (int) (atual % (long) colunas);

                for (k = 0; k < 8; k++) {
                    vl = li + DL[k];
                    vc = cj + DC[k];

                    /* A faixa e o limite do trabalho desta thread. */
                    if (vl < t->linha_ini || vl >= t->linha_fim) {
                        continue;
                    }
                    if (vc < 0 || vc >= colunas) {
                        continue;
                    }

                    viz = (long) vl * (long) colunas + (long) vc;
                    if (m->dados[viz] != 0 && t->rotulos[viz] == 0) {
                        t->rotulos[viz] = rotulo;
                        t->pilha[topo++] = viz;
                    }
                }
            }
        }
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/* Fase 2 (paralela): consolidacao da fronteira superior de cada faixa.*/
/* Cada thread trata apenas a fronteira entre a sua faixa e a de cima, */
/* portanto nenhuma fronteira e processada duas vezes.                 */
/* ------------------------------------------------------------------ */

static void *fase_fundir(void *arg)
{
    Tarefa *t = (Tarefa *) arg;
    int colunas = t->m->colunas;
    int b, j, dc, vc;
    long idx, viz;

    if (t->id == 0) {
        return NULL;   /* a primeira faixa nao tem faixa acima */
    }

    b = t->linha_ini;  /* primeira linha desta faixa */

    for (j = 0; j < colunas; j++) {
        idx = (long) b * (long) colunas + (long) j;
        if (t->rotulos[idx] == 0) {
            continue;
        }

        /* dc = -1 e +1 cobrem as ligacoes diagonais, dc = 0 a vertical.
         * Ligacoes horizontais nunca sao cortadas por faixas de linhas. */
        for (dc = -1; dc <= 1; dc++) {
            vc = j + dc;
            if (vc < 0 || vc >= colunas) {
                continue;
            }

            viz = (long) (b - 1) * (long) colunas + (long) vc;
            if (t->rotulos[viz] == 0) {
                continue;
            }

            pthread_mutex_lock(t->mutex);
            if (uf_unir(t->pai, t->rotulos[idx], t->rotulos[viz]) != 0) {
                t->unioes++;
            }
            pthread_mutex_unlock(t->mutex);
        }
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/* Orquestracao                                                        */
/* ------------------------------------------------------------------ */

static void libera_tarefas(Tarefa *tarefas, int n)
{
    int i;

    if (tarefas == NULL) {
        return;
    }
    for (i = 0; i < n; i++) {
        free(tarefas[i].pilha);
        free(tarefas[i].sementes);
    }
    free(tarefas);
}

/* Retorna 0 em caso de sucesso e preenche *res. */
static int conta_objetos_paralelo(const Matriz *m, int n_threads,
                                  Resultado *res, int verboso)
{
    pthread_t *threads = NULL;
    Tarefa *tarefas = NULL;
    pthread_mutex_t mutex;
    int *rotulos = NULL;
    int *pai = NULL;
    long total, celulas_faixa, i, s;
    int linhas, colunas, t, rc, base, resto, ini, mutex_criado;
    double t0, t1, t2, t3, t4;

    if (m == NULL || res == NULL) {
        return -1;
    }

    linhas = m->linhas;
    colunas = m->colunas;
    total = (long) linhas * (long) colunas;

    /* Os rotulos sao indices lineares +1 guardados em int. */
    if (total >= 2000000000L) {
        fprintf(stderr, "erro: matriz grande demais para esta implementacao\n");
        return -1;
    }

    if (n_threads > linhas) {
        if (verboso) {
            printf("aviso: %d threads para %d linhas; usando %d threads\n",
                   n_threads, linhas, linhas);
        }
        n_threads = linhas;
    }
    if (n_threads < 1) {
        n_threads = 1;
    }

    memset(res, 0, sizeof(Resultado));
    res->threads = n_threads;

    t0 = tempo_agora();

    rotulos = (int *) calloc((size_t) total, sizeof(int));
    pai = (int *) calloc((size_t) (total + 1), sizeof(int));
    threads = (pthread_t *) malloc((size_t) n_threads * sizeof(pthread_t));
    tarefas = (Tarefa *) calloc((size_t) n_threads, sizeof(Tarefa));

    if (rotulos == NULL || pai == NULL || threads == NULL || tarefas == NULL) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        free(rotulos);
        free(pai);
        free(threads);
        free(tarefas);
        return -1;
    }

    rc = pthread_mutex_init(&mutex, NULL);
    if (rc != 0) {
        fprintf(stderr, "erro: pthread_mutex_init falhou (%d)\n", rc);
        free(rotulos);
        free(pai);
        free(threads);
        free(tarefas);
        return -1;
    }
    mutex_criado = 1;

    /* Divisao das linhas: as 'resto' primeiras faixas ficam com uma
     * linha a mais, de modo que nenhuma faixa fique vazia. */
    base = linhas / n_threads;
    resto = linhas % n_threads;
    ini = 0;

    for (t = 0; t < n_threads; t++) {
        tarefas[t].id = t;
        tarefas[t].m = m;
        tarefas[t].linha_ini = ini;
        tarefas[t].linha_fim = ini + base + (t < resto ? 1 : 0);
        ini = tarefas[t].linha_fim;

        tarefas[t].rotulos = rotulos;
        tarefas[t].pai = pai;
        tarefas[t].mutex = &mutex;
        tarefas[t].unioes = 0;
        tarefas[t].erro = 0;
        tarefas[t].n_sementes = 0;
        tarefas[t].cap_sementes = CAP_SEMENTES_INICIAL;

        celulas_faixa = (long) (tarefas[t].linha_fim - tarefas[t].linha_ini)
                        * (long) colunas;
        tarefas[t].pilha = (long *) malloc((size_t) celulas_faixa
                                           * sizeof(long));
        tarefas[t].sementes = (long *) malloc((size_t) CAP_SEMENTES_INICIAL
                                              * sizeof(long));

        if (tarefas[t].pilha == NULL || tarefas[t].sementes == NULL) {
            fprintf(stderr, "erro: memoria insuficiente para a thread %d\n", t);
            libera_tarefas(tarefas, n_threads);
            free(rotulos);
            free(pai);
            free(threads);
            pthread_mutex_destroy(&mutex);
            return -1;
        }

        if (verboso) {
            printf("  faixa %2d -> linhas %d a %d (%d linhas)\n",
                   t, tarefas[t].linha_ini, tarefas[t].linha_fim - 1,
                   tarefas[t].linha_fim - tarefas[t].linha_ini);
        }
    }

    /* -------- Fase 1: rotulagem paralela -------- */
    t1 = tempo_agora();

    for (t = 0; t < n_threads; t++) {
        rc = pthread_create(&threads[t], NULL, fase_rotular, &tarefas[t]);
        if (rc != 0) {
            fprintf(stderr, "erro: pthread_create falhou na thread %d (%d)\n",
                    t, rc);
            /* espera as que ja foram criadas antes de abortar */
            for (i = 0; i < (long) t; i++) {
                pthread_join(threads[i], NULL);
            }
            libera_tarefas(tarefas, n_threads);
            free(rotulos);
            free(pai);
            free(threads);
            pthread_mutex_destroy(&mutex);
            return -1;
        }
    }

    for (t = 0; t < n_threads; t++) {
        rc = pthread_join(threads[t], NULL);
        if (rc != 0) {
            fprintf(stderr, "erro: pthread_join falhou na thread %d (%d)\n",
                    t, rc);
        }
    }

    for (t = 0; t < n_threads; t++) {
        if (tarefas[t].erro != 0) {
            fprintf(stderr, "erro: thread %d falhou ao alocar memoria\n", t);
            libera_tarefas(tarefas, n_threads);
            free(rotulos);
            free(pai);
            free(threads);
            pthread_mutex_destroy(&mutex);
            return -1;
        }
    }

    t2 = tempo_agora();

    /* -------- Fase 2: consolidacao paralela das fronteiras -------- */
    if (n_threads > 1) {
        for (t = 0; t < n_threads; t++) {
            rc = pthread_create(&threads[t], NULL, fase_fundir, &tarefas[t]);
            if (rc != 0) {
                fprintf(stderr, "erro: pthread_create (fusao) falhou (%d)\n",
                        rc);
                for (i = 0; i < (long) t; i++) {
                    pthread_join(threads[i], NULL);
                }
                libera_tarefas(tarefas, n_threads);
                free(rotulos);
                free(pai);
                free(threads);
                pthread_mutex_destroy(&mutex);
                return -1;
            }
        }
        for (t = 0; t < n_threads; t++) {
            rc = pthread_join(threads[t], NULL);
            if (rc != 0) {
                fprintf(stderr, "erro: pthread_join (fusao) falhou (%d)\n", rc);
            }
        }
    }

    t3 = tempo_agora();

    /* -------- Fase 3: contagem final (sequencial) --------
     * Um objeto para cada rotulo que continuou sendo raiz da sua classe.
     * O laco percorre apenas as sementes, nao a matriz inteira. */
    res->objetos = 0;
    for (t = 0; t < n_threads; t++) {
        res->componentes_locais += tarefas[t].n_sementes;
        res->unioes_fronteira += tarefas[t].unioes;

        for (s = 0; s < tarefas[t].n_sementes; s++) {
            int rotulo = (int) (tarefas[t].sementes[s] + 1);
            if (uf_raiz(pai, rotulo) == rotulo) {
                res->objetos++;
            }
        }
    }

    t4 = tempo_agora();

    res->t_rotulagem = t2 - t1;
    res->t_fusao = t3 - t2;
    res->t_contagem = t4 - t3;
    res->t_total = t4 - t0;

    libera_tarefas(tarefas, n_threads);
    free(rotulos);
    free(pai);
    free(threads);
    if (mutex_criado) {
        pthread_mutex_destroy(&mutex);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Interface de linha de comando                                       */
/* ------------------------------------------------------------------ */

static void uso(const char *prog)
{
    printf("Uso: %s [opcoes]\n", prog);
    printf("  (sem opcoes)   executa as 5 matrizes obrigatorias\n");
    printf("  -n N           numero de threads (padrao 4)\n");
    printf("  -m N           executa apenas a matriz obrigatoria N (1..5)\n");
    printf("  -a ARQUIVO     le a matriz de um arquivo texto\n");
    printf("  -g L C D S     gera matriz L x C, densidade D%% e semente S\n");
    printf("  -r REP         repete a medicao REP vezes (padrao 1)\n");
    printf("  -o ARQUIVO     grava a matriz usada em um arquivo texto\n");
    printf("  -p             imprime a matriz antes de contar\n");
    printf("  -v             mostra faixas, fusoes e tempo de cada fase\n");
    printf("  -h             mostra esta ajuda\n");
}

static int executa_obrigatorias(int n_threads, int imprimir, int verboso)
{
    int n, esperado, falhas;
    long sequencial;
    double t0, t1;
    Matriz *m;
    Resultado res;

    falhas = 0;

    printf("Versao paralela - conectividade 8 - %d thread(s)\n\n", n_threads);
    printf("Ex  Dimensoes   Esperado  Sequencial  Paralelo  Situacao  Tempo par. (s)\n");

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
        if (verboso) {
            printf("\nMatriz %d - divisao do trabalho:\n", n);
        }

        esperado = matriz_obrigatoria_esperado(n);

        t0 = tempo_agora();
        sequencial = conta_objetos_sequencial(m);
        t1 = tempo_agora();

        if (sequencial < 0 ||
            conta_objetos_paralelo(m, n_threads, &res, verboso) != 0) {
            matriz_liberar(m);
            return -1;
        }

        if (res.objetos != (long) esperado || sequencial != res.objetos) {
            falhas++;
        }

        if (verboso) {
            printf("  componentes locais: %ld | fusoes nas fronteiras: %ld\n",
                   res.componentes_locais, res.unioes_fronteira);
            printf("  tempo sequencial: %.6f s\n", t1 - t0);
        }

        printf("%2d  %3d x %-5d %8d  %10ld  %8ld  %-8s  %.6f\n",
               n, m->linhas, m->colunas, esperado, sequencial, res.objetos,
               (res.objetos == (long) esperado && sequencial == res.objetos)
                   ? "OK" : "FALHOU",
               res.t_total);

        matriz_liberar(m);
    }

    printf("\n");
    if (falhas == 0) {
        printf("Todos os 5 casos obrigatorios conferem, ");
        printf("e a versao paralela reproduz a sequencial.\n");
    } else {
        printf("%d caso(s) divergiram.\n", falhas);
    }

    return falhas;
}

static int executa_matriz(Matriz *m, int n_threads, int repeticoes,
                          int imprimir, int verboso, int esperado,
                          int comparar)
{
    double tempos[MAX_REPETICOES];
    Resultado res;
    long anterior, sequencial;
    double t0, t1;
    int i;

    if (imprimir) {
        matriz_imprimir(m);
        printf("\n");
    }

    anterior = -1;
    sequencial = -1;

    for (i = 0; i < repeticoes; i++) {
        if (conta_objetos_paralelo(m, n_threads, &res, (verboso && i == 0)) != 0) {
            return -1;
        }
        if (anterior >= 0 && res.objetos != anterior) {
            fprintf(stderr, "erro: resultado nao deterministico entre execucoes\n");
            return -1;
        }
        anterior = res.objetos;
        tempos[i] = res.t_total;
    }

    printf("Matriz .............: %d x %d (%ld celulas)\n",
           m->linhas, m->colunas, (long) m->linhas * (long) m->colunas);
    printf("Threads ............: %d\n", res.threads);
    printf("Objetos encontrados : %ld\n", res.objetos);

    if (esperado >= 0) {
        printf("Esperado ...........: %d (%s)\n", esperado,
               (res.objetos == (long) esperado) ? "OK" : "FALHOU");
    }

    if (comparar) {
        t0 = tempo_agora();
        sequencial = conta_objetos_sequencial(m);
        t1 = tempo_agora();
        printf("Referencia sequencial: %ld (%s) em %.6f s\n",
               sequencial,
               (sequencial == res.objetos) ? "confere" : "DIVERGENTE",
               t1 - t0);
    }

    printf("Componentes locais .: %ld\n", res.componentes_locais);
    printf("Fusoes nas fronteiras: %ld\n", res.unioes_fronteira);
    printf("Repeticoes .........: %d\n", repeticoes);
    printf("Tempo mediano ......: %.6f s\n", tempo_mediana(tempos, repeticoes));
    printf("Tempo minimo .......: %.6f s\n", tempo_minimo(tempos, repeticoes));
    printf("Ultima execucao - rotulagem: %.6f s | fusao: %.6f s | contagem: %.6f s\n",
           res.t_rotulagem, res.t_fusao, res.t_contagem);

    if (comparar && sequencial != res.objetos) {
        return 1;
    }
    if (esperado >= 0 && res.objetos != (long) esperado) {
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int modo = MODO_OBRIGATORIAS;
    int n_threads = 4;
    int repeticoes = 1;
    int imprimir = 0;
    int verboso = 0;
    int comparar = 0;
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
        } else if (strcmp(argv[i], "-v") == 0) {
            verboso = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            comparar = 1;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            n_threads = atoi(argv[++i]);
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

    if (n_threads < 1 || n_threads > MAX_THREADS) {
        fprintf(stderr, "erro: numero de threads deve ficar entre 1 e %d\n",
                MAX_THREADS);
        return 1;
    }
    if (repeticoes < 1 || repeticoes > MAX_REPETICOES) {
        fprintf(stderr, "erro: repeticoes deve ficar entre 1 e %d\n",
                MAX_REPETICOES);
        return 1;
    }

    if (modo == MODO_OBRIGATORIAS) {
        resultado = executa_obrigatorias(n_threads, imprimir, verboso);
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

    resultado = executa_matriz(m, n_threads, repeticoes, imprimir, verboso,
                               esperado, comparar);
    matriz_liberar(m);

    return (resultado == 0) ? 0 : 1;
}
