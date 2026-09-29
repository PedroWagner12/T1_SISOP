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
| sequencial           | 0,198457          | 1,00       | —          |
| paralela, 1 thread   | 0,294674          | 0,67       | 67%        |
| paralela, 2 threads  | 0,165902          | 1,20       | 60%        |
| paralela, 4 threads  | 0,101805          | 1,95       | 49%        |
| paralela, 8 threads  | 0,077674          | 2,55       | 32%        |

Tempo por fase com 4 threads: rotulagem 0,0965 s, consolidação 0,00053 s, contagem final 0,0040 s.
Foram 758.112 componentes locais e 1.839 fusões nas fronteiras — a diferença de 1.839 entre as
componentes locais e o total de 756.273 objetos é exatamente o que a consolidação corrigiu.

## Carga 2 — matriz grande e densa

Matriz 4000 x 4000, densidade 45%, semente 7. Acima do limiar de percolação para conectividade 8, os
objetos se fundem: são apenas 117.108 objetos, mas cada flood fill percorre uma região muito maior.

| Versão               | Tempo mediano (s) | Aceleração | Eficiência |
|----------------------|-------------------|------------|------------|
| sequencial           | 0,408503          | 1,00       | —          |
| paralela, 1 thread   | 0,520713          | 0,78       | 78%        |
| paralela, 2 threads  | 0,277572          | 1,47       | 74%        |
| paralela, 4 threads  | 0,161644          | 2,53       | 63%        |
| paralela, 8 threads  | 0,110917          | 3,68       | 46%        |

Tempo por fase com 4 threads: rotulagem 0,1526 s, consolidação 0,00073 s, contagem final 0,0011 s.

Esta é a carga que melhor escala, e o motivo está nas fases: com menos objetos, a contagem final tem
menos sementes para percorrer e cai para 0,74% do tempo, contra 3,96% na carga esparsa. Menos trabalho
sequencial significa um teto de Amdahl mais alto.

## Carga 3 — matriz pequena

Matriz 200 x 200, densidade 30%, semente 1. O trabalho inteiro dura menos de um milissegundo.

| Versão               | Tempo mediano (s) | Aceleração |
|----------------------|-------------------|------------|
| sequencial           | 0,000590          | 1,00       |
| paralela, 1 thread   | 0,000892          | 0,66       |
| paralela, 2 threads  | 0,000673          | 0,88       |
| paralela, 4 threads  | 0,000670          | 0,88       |
| paralela, 8 threads  | 0,001025          | 0,58       |

## Leitura dos resultados

**O paralelismo funciona, e a escala é consistente.** Nas duas cargas grandes o tempo cai de forma
monotônica conforme as threads aumentam: 1,20 → 1,95 → 2,55 na esparsa e 1,47 → 2,53 → 3,68 na densa.
Nenhuma configuração piorou ao receber mais threads, o que indica que a sincronização não vira gargalo.

**A versão paralela com 1 thread é mais lenta que a sequencial, e isso não é custo de thread.** Ela
perde 0,67 e 0,78 nas duas cargas, com uma única thread criada. A diferença vem da estrutura de dados: a
versão sequencial guarda um byte por célula no vetor `visitado`, ou 16 MB; a paralela precisa de um
rótulo inteiro por célula (64 MB) e mais o vetor `pai` do union-find (64 MB), porque sem rótulo não há
como saber, na fronteira, a qual componente uma célula pertence. Percorrer 144 MB em vez de 16 MB custa
faltas de cache. É o preço de poder consolidar depois.

Por isso vale olhar também a aceleração interna, comparando a versão paralela consigo mesma. Tirando
esse custo estrutural da conta, o ganho de 1 para 8 threads é de **3,79x na carga esparsa e 4,69x na
densa** — é essa a medida de quanto a paralelização em si rendeu.

**A eficiência cai conforme as threads aumentam,** de 60% para 32% na esparsa e de 74% para 46% na
densa. Três razões se somam:

1. *Fração sequencial (lei de Amdahl).* A contagem final não paraleliza. Com 3,96% de fração sequencial
   na carga esparsa, o teto teórico é 3,57x com 4 threads e 6,26x com 8. Na carga densa, com 0,74%, o
   teto sobe para 3,91x e 7,61x — e de fato foi ela que chegou mais perto.
2. *Banda de memória.* A rotulagem é dominada por acesso a memória, não por cálculo. As threads
   percorrem 144 MB de vetores e disputam o mesmo barramento e o mesmo cache de último nível, então
   dobrar as threads não dobra a banda disponível.
3. *Núcleos lógicos não são núcleos físicos.* São 12 núcleos lógicos, mas metade deles são threads de
   hyper-threading, que compartilham as unidades de execução do núcleo físico. Para uma carga limitada
   por memória, como esta, a segunda thread lógica de um mesmo núcleo acrescenta pouco.

**Em matrizes pequenas, criar as threads custa mais do que o trabalho.** Na carga 3 o problema inteiro
leva 0,59 ms, e criar e destruir oito threads já consome uma fração relevante disso; o resultado é a
pior aceleração de todas as medições, 0,58. Isso confirma a observação do enunciado: matrizes pequenas
servem para validar correção, não para medir desempenho. Repare que, mesmo aqui, 2 e 4 threads ainda
melhoram em relação a 1 thread — é só a partir de 8 que o custo de criação vence o ganho.

**A consolidação não é o gargalo.** Ela custa 0,53 ms na carga esparsa e 0,73 ms na densa, o que dá
cerca de 0,5% do tempo total nas duas. Como ela é a única parte do programa com região crítica, isso
mostra que a estratégia de faixas com union-find paga muito pouco por sincronização: o custo dessa fase
é proporcional ao número de colunas vezes o número de fronteiras, e não ao número de células.

## Limitação conhecida: desequilíbrio de carga

As faixas têm o mesmo número de células, mas o custo do flood fill é proporcional à quantidade de
células com valor 1. Em uma matriz aleatória a densidade é uniforme e as faixas ficam equilibradas, que
é o caso de todas as medições acima. Em uma imagem real com os objetos concentrados na metade de cima, a
thread dessa faixa faria quase todo o trabalho e as outras ficariam ociosas.

A correção natural seria criar mais faixas do que threads e distribuí-las por uma fila dinâmica, o que o
enunciado permite explicitamente. O custo seria ter mais fronteiras para consolidar — mas, como a
consolidação custa 0,5% do tempo, há bastante margem para isso.

## Resumo

- A correção é independente do número de threads: as cinco matrizes obrigatórias, os sete testes de
  borda e 40 matrizes aleatórias deram o mesmo resultado com 1, 2, 3, 4, 7 e 8 threads.
- A aceleração chegou a 2,55x na carga esparsa e 3,68x na densa com 8 threads, em uma máquina de 12
  núcleos lógicos.
- Descontando o custo estrutural dos vetores, o ganho de 1 para 8 threads foi de 3,79x e 4,69x.
- A consolidação das fronteiras, única região crítica do programa, custa cerca de 0,5% do tempo.
- O paralelismo não compensa em matrizes pequenas: abaixo de um milissegundo de trabalho, criar threads
  custa mais do que elas economizam.
