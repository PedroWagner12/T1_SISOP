#!/bin/sh
# executar-testes.sh - bateria de corretude.
# Sistemas Operacionais - PUCRS - 2026/II
#
# Roda:
#   1. as 5 matrizes obrigatorias na versao sequencial;
#   2. as 5 matrizes obrigatorias na versao paralela com varias
#      quantidades de threads;
#   3. os testes adicionais listados em tests/casos.txt;
#   4. um teste aleatorio comparando as duas versoes em 40 matrizes.
#
# Uso: ./tests/executar-testes.sh   (a partir da raiz do repositorio)

set -u

RAIZ=$(dirname "$0")/..
cd "$RAIZ" || exit 1

SEQ=./bin/conta-objetos-sequencial
PAR=./bin/conta-objetos-paralelo
SAIDA=results/corretude.txt

if [ ! -x "$SEQ" ] || [ ! -x "$PAR" ]; then
    echo "compile antes com 'make'"
    exit 1
fi

mkdir -p results

falhas=0

registra() {
    echo "$1" | tee -a "$SAIDA"
}

objetos_de() {
    # extrai a quantidade de objetos da saida dos programas
    grep "Objetos encontrados" | sed 's/[^0-9]*//'
}

: > "$SAIDA"
registra "Testes de corretude - $(date '+%Y-%m-%d %H:%M:%S')"
registra "Maquina: $(uname -s) $(uname -m)"
registra ""

registra "=== 1. Matrizes obrigatorias - versao sequencial ==="
$SEQ | tee -a "$SAIDA"
[ $? -ne 0 ] && falhas=$((falhas + 1))
registra ""

registra "=== 2. Matrizes obrigatorias - versao paralela ==="
for t in 1 2 3 4 8; do
    registra ""
    $PAR -n "$t" | tee -a "$SAIDA"
    if [ $? -ne 0 ]; then
        falhas=$((falhas + 1))
    fi
done
registra ""

registra "=== 3. Testes adicionais (arquivos em tests/) ==="
registra ""
registra "Arquivo                            Esperado  Seq  Par1  Par2  Par4  Par7  Situacao"
grep -v '^#' tests/casos.txt | while read -r arq esperado; do
    [ -z "$arq" ] && continue
    s=$($SEQ -a "tests/$arq" | objetos_de)
    p1=$($PAR -a "tests/$arq" -n 1 | objetos_de)
    p2=$($PAR -a "tests/$arq" -n 2 | objetos_de)
    p4=$($PAR -a "tests/$arq" -n 4 | objetos_de)
    p7=$($PAR -a "tests/$arq" -n 7 | objetos_de)
    sit="OK"
    if [ "$s" != "$esperado" ] || [ "$p1" != "$esperado" ] || \
       [ "$p2" != "$esperado" ] || [ "$p4" != "$esperado" ] || \
       [ "$p7" != "$esperado" ]; then
        sit="FALHOU"
    fi
    printf "%-34s %8s %4s %5s %5s %5s %5s  %s\n" \
        "$arq" "$esperado" "$s" "$p1" "$p2" "$p4" "$p7" "$sit" | tee -a "$SAIDA"
done
registra ""

registra "=== 4. Teste aleatorio: paralelo x sequencial (40 matrizes) ==="
divergencias=0
i=1
while [ "$i" -le 40 ]; do
    L=$(( (i * 37) % 120 + 3 ))
    C=$(( (i * 53) % 130 + 3 ))
    D=$(( (i * 7) % 70 + 5 ))
    T=$(( (i % 8) + 1 ))
    s=$($SEQ -g "$L" "$C" "$D" "$i" | objetos_de)
    p=$($PAR -g "$L" "$C" "$D" "$i" -n "$T" | objetos_de)
    if [ "$s" != "$p" ]; then
        registra "DIVERGENCIA: ${L}x${C} densidade ${D}% semente ${i} threads ${T}: seq=$s par=$p"
        divergencias=$((divergencias + 1))
    fi
    i=$((i + 1))
done

if [ "$divergencias" -eq 0 ]; then
    registra "40 matrizes aleatorias: a versao paralela reproduziu a sequencial em todos os casos."
else
    registra "$divergencias divergencia(s) encontradas."
    falhas=$((falhas + 1))
fi

registra ""
registra "Resultado gravado em $SAIDA"
exit 0
