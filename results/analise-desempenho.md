# Análise de desempenho

Todos os números deste arquivo saíram de `tests/medir-desempenho.sh`, e a saída bruta está em
`results/desempenho.txt`. Cada configuração foi executada sete vezes; o valor representativo é a
**mediana** das sete execuções, que é menos sensível a uma execução isolada atrapalhada por outro
processo do sistema do que a média. Os tempos são de relógio de parede
(`clock_gettime(CLOCK_MONOTONIC)`) e incluem as alocações de cada versão.

A aceleração é `S = T_sequencial / T_paralelo` e a eficiência é `S / número de threads`.

## Ambiente da medição

| Item             | Valor                                           |
|------------------|-------------------------------------------------|
| Sistema          | Linux x86_64 (WSL2 sobre Windows 11)            |
| Núcleos lógicos  | 12                                              |
| Compilador       | gcc 15.2.0                                      |
| Flags            | `-std=c89 -Wall -Wextra -pedantic -O2 -pthread` |
| Repetições       | 7 (mediana)                                     |

## Carga 1 — matriz grande e esparsa

Matriz 4000 x 4000, densidade 30%, semente 42. São 16 milhões de células e 756.273 objetos, ou seja,
muitos objetos pequenos e, portanto, muitas sementes de flood fill.

| Versão               | Tempo mediano (s) | Aceleração | Eficiência |
|----------------------|-------------------|------------|------------|
| sequencial           | 0,278486          | 1,00       | —          |
| paralela, 1 thread   | 0,440975          | 0,63       | 63%        |
| paralela, 2 threads  | 0,255961          | 1,09       | 54%        |
| paralela, 4 threads  | 0,179507          | 1,55       | 39%        |
| paralela, 8 threads  | 0,118013          | 2,36       | 29%        |

Tempo por fase com 4 threads: rotulagem 0,1420 s, consolidação 0,00102 s, contagem final 0,0046 s.
Foram 758.112 componentes locais e 1.839 fusões nas fronteiras — a diferença de 1.839 entre as
componentes locais e o total de 756.273 objetos é exatamente o que a consolidação corrigiu.

## Carga 2 — matriz grande e densa

Matriz 4000 x 4000, densidade 45%, semente 7. Acima do limiar de percolação para conectividade 8, os
objetos se fundem: são apenas 117.108 objetos, mas cada flood fill percorre uma região muito maior.

| Versão               | Tempo mediano (s) | Aceleração | Eficiência |
|----------------------|-------------------|------------|------------|
| sequencial           | 0,575346          | 1,00       | —          |
| paralela, 1 thread   | 0,746867          | 0,77       | 77%        |
| paralela, 2 threads  | 0,404775          | 1,42       | 71%        |
| paralela, 4 threads  | 0,257124          | 2,24       | 56%        |
| paralela, 8 threads  | 0,168954          | 3,41       | 43%        |

Tempo por fase com 4 threads: rotulagem 0,2677 s, consolidação 0,00183 s, contagem final 0,0015 s.

Esta é a carga que melhor escala, e o motivo está nas fases: com menos objetos, a contagem final tem
menos sementes para percorrer e cai para 0,57% do tempo, contra 3,14% na carga esparsa. Menos trabalho
sequencial significa um teto de Amdahl mais alto.

## Carga 3 — matriz pequena

Matriz 200 x 200, densidade 30%, semente 1. O trabalho inteiro dura menos de um milissegundo.

| Versão               | Tempo mediano (s) | Aceleração |
|----------------------|-------------------|------------|
| sequencial           | 0,000850          | 1,00       |
| paralela, 1 thread   | 0,001505          | 0,56       |
| paralela, 2 threads  | 0,002251          | 0,38       |
| paralela, 4 threads  | 0,001935          | 0,44       |
| paralela, 8 threads  | 0,002857          | 0,30       |

## Leitura dos resultados

**O paralelismo funciona, e a escala é consistente.** Nas duas cargas grandes o tempo cai de forma
monotônica conforme as threads aumentam: 1,09 → 1,55 → 2,36 na esparsa e 1,42 → 2,24 → 3,41 na densa.
Nenhuma configuração piorou ao receber mais threads, o que indica que a sincronização não vira gargalo.

**A versão paralela com 1 thread é mais lenta que a sequencial, e isso não é custo de thread.** Ela
perde 0,63 e 0,77 nas duas cargas, com uma única thread criada. A diferença vem da estrutura de dados: a
versão sequencial guarda um byte por célula no vetor `visitado`, ou 16 MB; a paralela precisa de um
rótulo inteiro por célula (64 MB) e mais o vetor `pai` do union-find (64 MB), porque sem rótulo não há
como saber, na fronteira, a qual componente uma célula pertence. Percorrer 144 MB em vez de 16 MB custa
faltas de cache. É o preço de poder consolidar depois.

Por isso vale olhar também a aceleração interna, comparando a versão paralela consigo mesma. Tirando
esse custo estrutural da conta, o ganho de 1 para 8 threads é de **3,74x na carga esparsa e 4,42x na
densa** — é essa a medida de quanto a paralelização em si rendeu.

**A eficiência cai conforme as threads aumentam,** de 54% para 29% na esparsa e de 71% para 43% na
densa. Três razões se somam:

1. *Fração sequencial (lei de Amdahl).* A contagem final não paraleliza. Com 3,14% de fração sequencial
   na carga esparsa, o teto teórico é 3,66x com 4 threads e 6,56x com 8. Na carga densa, com 0,57%, o
   teto sobe para 3,93x e 7,69x — e de fato foi ela que chegou mais perto.
2. *Banda de memória.* A rotulagem é dominada por acesso a memória, não por cálculo. As threads
   percorrem 144 MB de vetores e disputam o mesmo barramento e o mesmo cache de último nível, então
   dobrar as threads não dobra a banda disponível.
3. *Núcleos lógicos não são núcleos físicos.* São 12 núcleos lógicos, mas metade deles são threads de
   hyper-threading, que compartilham as unidades de execução do núcleo físico. Para uma carga limitada
   por memória, como esta, a segunda thread lógica de um mesmo núcleo acrescenta pouco.

**Em matrizes pequenas, criar as threads custa mais do que o trabalho.** Na carga 3 o problema inteiro
leva 0,85 ms, e criar e destruir as threads consome mais do que isso; o resultado é a pior aceleração de
todas as medições, 0,30. Isso confirma a observação do enunciado: matrizes pequenas servem para validar
correção, não para medir desempenho. Repare que aqui nenhuma configuração paralela melhora: já com 2
threads o tempo é pior do que com 1, porque o trabalho útil de cada thread é pequeno demais para pagar o
próprio custo de criação.

**A consolidação não é o gargalo.** Ela custa 1,02 ms na carga esparsa e 1,83 ms na densa, o que dá
cerca de 0,7% do tempo total nas duas. Como ela é a única parte do programa com região crítica, isso
mostra que a estratégia de faixas com union-find paga muito pouco por sincronização: o custo dessa fase
é proporcional ao número de colunas vezes o número de fronteiras, e não ao número de células.

## Limitação conhecida: desequilíbrio de carga

As faixas têm o mesmo número de células, mas o custo do flood fill é proporcional à quantidade de
células com valor 1. Em uma matriz aleatória a densidade é uniforme e as faixas ficam equilibradas, que
é o caso de todas as medições acima. Em uma imagem real com os objetos concentrados na metade de cima, a
thread dessa faixa faria quase todo o trabalho e as outras ficariam ociosas.

A correção natural seria criar mais faixas do que threads e distribuí-las por uma fila dinâmica, o que o
enunciado permite explicitamente. O custo seria ter mais fronteiras para consolidar — mas, como a
consolidação custa 0,7% do tempo, há bastante margem para isso.

## Resumo

- A correção é independente do número de threads: as cinco matrizes obrigatórias, os sete testes de
  borda e 40 matrizes aleatórias deram o mesmo resultado com 1, 2, 3, 4, 7 e 8 threads.
- A aceleração chegou a 2,36x na carga esparsa e 3,41x na densa com 8 threads, em uma máquina de 12
  núcleos lógicos.
- Descontando o custo estrutural dos vetores, o ganho de 1 para 8 threads foi de 3,74x e 4,42x.
- A consolidação das fronteiras, única região crítica do programa, custa cerca de 0,7% do tempo.
- O paralelismo não compensa em matrizes pequenas: abaixo de um milissegundo de trabalho, criar threads
  custa mais do que elas economizam.
