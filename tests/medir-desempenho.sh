#!/bin/sh
# medir-desempenho.sh - analise de desempenho (secao 8 do enunciado).
# Sistemas Operacionais - PUCRS - 2026/II
#
# Para cada carga de trabalho executa a versao sequencial e a versao
# paralela com 1, 2, 4 e 8 threads, repetindo cada medicao REP vezes.
# O valor representativo e a MEDIANA das repeticoes (menos sensivel a
# uma execucao isolada atrapalhada por outro processo do sistema).
#
# Uso: ./tests/medir-desempenho.sh   (a partir da raiz do repositorio)

set -u

RAIZ=$(dirname "$0")/..
cd "$RAIZ" || exit 1

SEQ=./bin/conta-objetos-sequencial
PAR=./bin/conta-objetos-paralelo
SAIDA=results/desempenho.txt
REP=7

if [ ! -x "$SEQ" ] || [ ! -x "$PAR" ]; then
    echo "compile antes com 'make'"
    exit 1
fi

mkdir -p results

tempo_de() {
    grep "Tempo mediano" | sed 's/.*: *//; s/ s$//'
}

objetos_de() {
    grep "Objetos encontrados" | sed 's/[^0-9]*//'
}

nucleos() {
    if command -v nproc > /dev/null 2>&1; then
        nproc
    elif command -v sysctl > /dev/null 2>&1; then
        sysctl -n hw.ncpu
    else
        echo "desconhecido"
    fi
}

: > "$SAIDA"
{
    echo "Analise de desempenho - $(date '+%Y-%m-%d %H:%M:%S')"
    echo "Sistema ..........: $(uname -s) $(uname -m)"
    echo "Nucleos logicos ..: $(nucleos)"
    echo "Compilador .......: $(cc --version 2>/dev/null | head -1)"
    echo "Repeticoes .......: $REP (valor representativo = mediana)"
    echo "Aceleracao .......: S = T_sequencial / T_paralelo"
    echo ""
} | tee -a "$SAIDA"

mede_carga() {
    rotulo=$1
    linhas=$2
    colunas=$3
    densidade=$4
    semente=$5

    saida_seq=$($SEQ -g "$linhas" "$colunas" "$densidade" "$semente" -r $REP)
    ts=$(echo "$saida_seq" | tempo_de)
    obj=$(echo "$saida_seq" | objetos_de)

    {
        echo "--- Carga: $rotulo ---"
        echo "Matriz: ${linhas} x ${colunas} | densidade ${densidade}% | semente ${semente}"
        echo "Objetos contados: $obj (identico em todas as configuracoes)"
        echo ""
        printf "%-22s %14s %12s %14s\n" "Versao" "Tempo med.(s)" "Aceleracao" "Eficiencia"
        printf "%-22s %14s %12s %14s\n" "sequencial" "$ts" "1.00" "-"
    } | tee -a "$SAIDA"

    for t in 1 2 4 8; do
        saida_par=$($PAR -g "$linhas" "$colunas" "$densidade" "$semente" -n "$t" -r $REP)
        tp=$(echo "$saida_par" | tempo_de)
        op=$(echo "$saida_par" | objetos_de)

        if [ "$op" != "$obj" ]; then
            echo "ERRO: paralelo com $t threads contou $op objetos" | tee -a "$SAIDA"
        fi

        echo "$ts $tp $t" | awk '{
            s = ($2 > 0) ? $1 / $2 : 0;
            printf "%-22s %14.6f %12.2f %13.0f%%\n", "paralelo (" $3 " threads)", $2, s, 100*s/$3;
        }' | tee -a "$SAIDA"
    done

    # detalhamento das fases com 4 threads
    {
        echo ""
        echo "Detalhe das fases com 4 threads:"
        $PAR -g "$linhas" "$colunas" "$densidade" "$semente" -n 4 -r 1 | \
            grep -E "Componentes locais|Fusoes|Ultima execucao"
        echo ""
    } | tee -a "$SAIDA"
}

mede_carga "grande esparsa (muitos objetos pequenos)" 4000 4000 30 42
mede_carga "grande densa (poucos objetos gigantes)"   4000 4000 45 7
mede_carga "pequena (custo de criacao domina)"         200  200 30 1

echo "Resultado gravado em $SAIDA" | tee -a "$SAIDA"
