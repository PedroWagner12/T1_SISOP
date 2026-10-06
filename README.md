# Contagem paralela de objetos em uma matriz binária

Trabalho prático da disciplina de Sistemas Operacionais (2026/II) — Escola Politécnica, PUCRS.
Professor Filipo Novo Mór.

O programa conta quantos objetos existem em uma imagem binária representada por uma matriz de 0 e 1,
usando conectividade 8 (células vizinhas por aresta ou por canto pertencem ao mesmo objeto).
Há duas implementações funcionalmente equivalentes: uma sequencial, usada como referência de correção,
e uma paralela com Pthreads (POSIX Threads).

## Autoria

- Pedro Augusto Wagner — matrícula 23107295
- Diogo Giacoboni Gallo — matrícula 24102192

Repositório: https://github.com/PedroWagner12/T1_SISOP

Vídeo da apresentação: https://youtu.be/McfPHyLeQQY

## Compilação

Requer um compilador C com suporte a ANSI C (C89/C90) e à biblioteca Pthreads, em Linux ou macOS.
Não há nenhuma dependência de Windows.

```
make
```

Os dois executáveis ficam em `bin/`. A compilação usa exatamente as flags sugeridas no enunciado:

```
cc -std=c89 -Wall -Wextra -pedantic -O2 -pthread
```

e termina sem erros e sem avisos.

Outros alvos:

| Alvo          | O que faz                                                            |
|---------------|----------------------------------------------------------------------|
| `make`        | compila os dois programas                                            |
| `make teste`  | roda a bateria de corretude e grava `results/corretude.txt`          |
| `make bench`  | roda a análise de desempenho e grava `results/desempenho.txt`        |
| `make tsan`   | recompila a versão paralela com ThreadSanitizer e procura corridas   |
| `make clean`  | remove `bin/` e `build/`                                             |

## Execução

Sem argumentos, os dois programas executam as cinco matrizes obrigatórias do enunciado:

```
./bin/conta-objetos-sequencial
./bin/conta-objetos-paralelo -n 4
```

Opções aceitas pelos dois programas:

```
-m N           executa apenas a matriz obrigatória N (1 a 5)
-a ARQUIVO     lê a matriz de um arquivo texto
-g L C D S     gera uma matriz L x C com densidade D% e semente S
-r REP         repete a medição REP vezes e reporta mediana e mínimo
-o ARQUIVO     grava a matriz usada em um arquivo texto
-p             imprime a matriz antes de contar
-h             ajuda
```

A versão paralela aceita ainda:

```
-n N           quantidade de threads (padrão 4)
-v             mostra a divisão em faixas, as fusões de fronteira e o tempo de cada fase
-c             conta também pela versão sequencial e compara os dois resultados
```

Exemplos:

```
./bin/conta-objetos-paralelo -m 3 -n 2 -v          # mostra a consolidação do exemplo 3
./bin/conta-objetos-paralelo -a tests/exemplo5.txt -n 4
./bin/conta-objetos-paralelo -g 4000 4000 30 42 -n 8 -r 7 -c
```

O gerador de matrizes é um LCG escrito no próprio código, então a mesma semente produz exatamente a
mesma matriz em qualquer máquina e em qualquer compilador. Isso é o que permite comparar a versão
sequencial e a paralela sobre dados idênticos sem precisar guardar arquivos gigantes no repositório.

## Arquitetura

```
src/matriz.c      representação da matriz, as 5 matrizes obrigatórias, gerador e leitura de arquivo
src/sequencial.c  contagem de referência (flood fill iterativo)
src/tempo.c       relógio monotônico e estatísticas das repetições
src/conta-objetos-sequencial.c   programa da versão sequencial
src/conta-objetos-paralelo.c     programa da versão paralela (Pthreads + union-find)
tests/            matrizes de teste e scripts de execução
results/          saídas gravadas dos testes e das medições
slides/           apresentação em PDF
```

### Versão sequencial

Percorre a matriz em ordem de varredura. Quando encontra uma célula com valor 1 ainda não visitada,
incrementa o contador de objetos e faz um preenchimento por inundação a partir dela, marcando todas as
células alcançáveis pelos 8 vizinhos.

O flood fill é iterativo, com pilha explícita, e não recursivo. Em uma matriz de 4000 x 4000 com um
objeto grande, a versão recursiva ultrapassaria a pilha do processo e o programa morreria com falha de
segmentação. A pilha explícita é dimensionada para o número de células da matriz, que é o pior caso,
porque cada célula é marcada antes de ser empilhada e portanto entra na pilha no máximo uma vez.

Uma estrutura `visitado` separada da matriz de entrada garante que a matriz original não seja destruída
e deixa clara a diferença entre célula visitada e não visitada.

### Versão paralela

A decomposição é por faixas de linhas: a matriz é dividida em T faixas contíguas, uma por thread. As
faixas que sobram do resto da divisão recebem uma linha a mais, de modo que nenhuma thread fique sem
trabalho. Se forem pedidas mais threads do que linhas, o programa reduz T para o número de linhas.

O processamento tem três fases.

**Fase 1 — rotulagem local (paralela).** Cada thread roda o mesmo flood fill da versão sequencial, mas
restrito às suas linhas: qualquer vizinho fora de `[linha_ini, linha_fim)` é ignorado. Cada thread
escreve apenas nas posições do vetor `rotulos` que pertencem às suas linhas, então não existe dado
compartilhado sendo escrito por duas threads e essa fase não precisa de nenhuma sincronização.

A identificação das componentes locais é o ponto que dispensa região crítica: o rótulo de uma
componente é o índice linear da sua célula semente somado de 1. Como cada célula pertence a uma única
faixa, dois rótulos gerados por threads diferentes nunca colidem, e não é preciso um contador global
protegido por mutex.

**Fase 2 — consolidação das fronteiras (paralela, com mutex).** Uma componente que atravessa a divisão
entre duas faixas foi contada duas vezes, uma em cada lado. Somar as contagens locais daria o resultado
errado; é preciso reconhecer que os dois rótulos são o mesmo objeto.

Isso é feito com uma estrutura union-find (conjuntos disjuntos) compartilhada, com compressão de
caminho. Cada thread trata apenas a fronteira entre a sua faixa e a faixa de cima, de forma que nenhuma
fronteira é processada duas vezes e não há trabalho duplicado. Para cada célula com valor 1 na primeira
linha da faixa, são verificados os três vizinhos da última linha da faixa anterior: diagonal esquerda,
vertical e diagonal direita. Se o vizinho também vale 1, os dois rótulos são unidos.

Esses três vizinhos cobrem todas as ligações que a divisão por linhas pode cortar. Ligações
horizontais nunca são cortadas por faixas de linhas, porque as duas células de uma ligação horizontal
estão sempre na mesma linha e, portanto, na mesma faixa. Ligações verticais e as duas diagonais são
exatamente os casos tratados.

O encontro de quatro blocos, citado no enunciado, é uma situação específica da decomposição em blocos
2D. Com faixas de linhas não existe fronteira vertical e portanto não existe ponto onde quatro regiões
se encontram: aquele caso degenera nas ligações diagonais da fronteira horizontal, que são justamente
os deslocamentos -1 e +1 verificados aqui. O arquivo `tests/adicional-diagonal.txt`, uma diagonal que
atravessa todas as faixas ligada apenas por cantos, e `tests/adicional-listras-verticais.txt`, com oito
objetos que atravessam todas as faixas, cobrem esse comportamento.

A união propriamente dita é a região crítica: o vetor `pai` do union-find é compartilhado e as threads
podem alterá-lo ao mesmo tempo. Por isso cada par `busca + união` acontece entre
`pthread_mutex_lock` e `pthread_mutex_unlock`. O custo é baixo porque o trabalho dessa fase é
proporcional ao número de colunas vezes o número de fronteiras, e não ao número de células. Nas
medições ela fica em torno de 0,5 ms em uma matriz de 16 milhões de células, ou seja, meio por cento
do tempo total.

A união sempre mantém como raiz o menor rótulo da classe. Isso torna o resultado independente da ordem
em que as threads executam as fusões: a mesma entrada produz sempre a mesma resposta, mesmo que o
escalonador intercale as threads de forma diferente a cada execução.

**Fase 3 — contagem final (sequencial).** Cada thread guardou a lista das suas células semente. O total
de objetos é a quantidade de rótulos que continuaram sendo raiz da própria classe depois das fusões.
O laço percorre apenas as sementes, não a matriz inteira, e leva alguns milissegundos mesmo na matriz
maior. Essa fase ficou sequencial de propósito: paralelizá-la exigiria um contador compartilhado, cujo
custo de sincronização seria maior do que o trabalho economizado.

Não é usada nenhuma barreira (`pthread_barrier`), porque ela não existe no macOS. A separação entre as
fases é feita criando e juntando as threads duas vezes, com `pthread_create` e `pthread_join`, o que é
portável entre Linux e macOS.

### Por que Pthreads e não processos

O enunciado permite processos POSIX, Pthreads ou uma combinação. A escolha foi Pthreads por dois
motivos concretos.

O primeiro é o custo: no próprio material da disciplina, a medição do professor mostra `fork()` cerca
de quatro vezes mais caro que `pthread_create()` por criação, porque criar um processo envolve montar
uma nova tabela de páginas. Como a fase paralela deste problema dura poucas centenas de milissegundos,
o custo de criação pesa.

O segundo é a natureza dos dados: a matriz de entrada e o vetor de rótulos são grandes e precisam ser
vistos por todos os trabalhadores. Com threads eles simplesmente já são compartilhados. Com processos
seria preciso memória compartilhada (`shm_open` mais `mmap`) ou copiar os resultados parciais por
pipes, o que acrescenta código sem acrescentar paralelismo.

### Verificação de retornos e liberação de recursos

Os retornos de `pthread_create`, `pthread_join` e `pthread_mutex_init` são verificados. Se a criação de
uma thread falhar no meio do laço, o programa espera as threads já criadas antes de liberar a memória,
para não deixar threads escrevendo em vetores já liberados. Toda memória alocada é liberada no fim, e o
mutex é destruído com `pthread_mutex_destroy`. Falhas de `malloc`, `calloc` e `realloc` são tratadas com
mensagem de erro e retorno diferente de zero.

## Matrizes de teste e resultados

As cinco matrizes obrigatórias estão embutidas em `src/matriz.c`, no mesmo formato de inicializador em C
que o Editor de tabelas C produz, e também em arquivo texto em `tests/exemplo1.txt` a
`tests/exemplo5.txt`, para quem quiser carregá-las com a opção `-a`.

### Registro dos resultados (seção 7.1 do enunciado)

| Ex. | Dimensões | Esperado | Sequencial | Paralelo |
|-----|-----------|----------|------------|----------|
| 1   | 5 x 5     | 3        | 3          | 3        |
| 2   | 6 x 8     | 4        | 4          | 4        |
| 3   | 8 x 8     | 5        | 5          | 5        |
| 4   | 9 x 12    | 6        | 6          | 6        |
| 5   | 12 x 12   | 7        | 7          | 7        |

A coluna paralela foi conferida com 1, 2, 3, 4 e 8 threads, e o resultado é o mesmo em todas as
configurações. A saída completa está em `results/corretude.txt`.

Um exemplo de consolidação, com o exemplo 3 dividido em duas faixas (linhas 0 a 3 e linhas 4 a 7):

```
$ ./bin/conta-objetos-paralelo -m 3 -n 2 -v
  faixa  0 -> linhas 0 a 3 (4 linhas)
  faixa  1 -> linhas 4 a 7 (4 linhas)
Objetos encontrados : 5
Componentes locais .: 6
Fusoes nas fronteiras: 1
```

São seis componentes locais porque o objeto central ocupa as linhas 3 e 4 e foi rotulado duas vezes, uma
em cada faixa. A fusão na fronteira reconhece que são o mesmo objeto e o total volta a ser 5.

### Testes adicionais

Além das matrizes obrigatórias, `tests/` traz casos de borda com resposta conhecida:

| Arquivo                             | Dimensões | Objetos | O que exercita                                   |
|-------------------------------------|-----------|---------|--------------------------------------------------|
| `adicional-vazia.txt`               | 10 x 10   | 0       | matriz sem nenhum objeto                         |
| `adicional-celula-unica.txt`        | 1 x 1     | 1       | menor matriz possível                            |
| `adicional-cheia.txt`               | 12 x 12   | 1       | um objeto que ocupa todas as faixas              |
| `adicional-diagonal.txt`            | 16 x 16   | 1       | objeto ligado só por cantos, cruzando as faixas  |
| `adicional-xadrez.txt`              | 8 x 8     | 1       | conectividade 8 pura (só diagonais)              |
| `adicional-listras-horizontais.txt` | 16 x 16   | 8       | objetos que nunca cruzam fronteiras              |
| `adicional-listras-verticais.txt`   | 16 x 16   | 8       | oito objetos que cruzam todas as fronteiras      |

O script `tests/executar-testes.sh` ainda compara as duas versões em 40 matrizes aleatórias, variando
dimensões, densidade e número de threads de 1 a 8. Nenhuma divergência foi encontrada.

Para procurar condições de corrida, a versão paralela também foi compilada com o ThreadSanitizer
(`make tsan`) e executada com 4 e 8 threads. Nenhum aviso foi emitido.

## Análise de desempenho

A carga principal é uma matriz de 4000 x 4000 (16 milhões de células) gerada com semente fixa. Cada
configuração é executada sete vezes e o valor representativo é a mediana das sete execuções, e não a
média: a mediana descarta o efeito de uma execução isolada atrapalhada por outro processo do sistema.
Os tempos são de relógio de parede, medidos com `clock_gettime(CLOCK_MONOTONIC)`. `clock()` não serviria
aqui, porque ele mede tempo de CPU e somaria o tempo de todas as threads.

O tempo medido inclui as alocações de cada versão, que fazem parte do custo real de cada abordagem.

Os números completos, com três cargas diferentes e o detalhamento por fase, estão em
`results/desempenho.txt`, e a discussão está em `results/analise-desempenho.md`. Para reproduzir na
máquina do grupo basta rodar `make bench`, que regrava esse arquivo.

## Ferramentas e referências

- Enunciado da disciplina e material de aula do Prof. Filipo Novo Mór (filipomor.com), em especial as
  medições de custo de `fork()` e `pthread_create()` citadas acima.
- SILBERSCHATZ, A.; GALVIN, P. B.; GAGNE, G. *Operating System Concepts*, 10ª ed. Wiley — capítulos de
  processos, threads e sincronização.
- Páginas de manual POSIX de `pthread_create`, `pthread_join` e `pthread_mutex_lock`.
- Estrutura union-find com compressão de caminho: algoritmo clássico de conjuntos disjuntos, usado aqui
  para unificar rótulos equivalentes nas fronteiras.
- ThreadSanitizer (GCC/Clang) para a verificação de condições de corrida.

Nenhuma biblioteca externa é usada. Todo o código é ANSI C com APIs POSIX.
