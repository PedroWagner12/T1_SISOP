# Makefile - Contagem paralela de objetos em uma matriz binaria
# Sistemas Operacionais - PUCRS - 2026/II
#
# Alvos principais:
#   make            compila os dois programas em bin/
#   make teste      roda as matrizes obrigatorias e os testes adicionais
#   make bench      roda a analise de desempenho e grava em results/
#   make clean      remove binarios e objetos

CC      = cc
CFLAGS  = -std=c89 -Wall -Wextra -pedantic -O2
LDFLAGS = -pthread

SRC  = src
BIN  = bin
OBJ  = build

COMUNS_SRC = $(SRC)/matriz.c $(SRC)/sequencial.c $(SRC)/tempo.c
COMUNS_OBJ = $(OBJ)/matriz.o $(OBJ)/sequencial.o $(OBJ)/tempo.o

SEQ = $(BIN)/conta-objetos-sequencial
PAR = $(BIN)/conta-objetos-paralelo

all: $(SEQ) $(PAR)

$(OBJ):
	mkdir -p $(OBJ)

$(BIN):
	mkdir -p $(BIN)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(CFLAGS) -pthread -c $< -o $@

$(SEQ): $(OBJ)/conta-objetos-sequencial.o $(COMUNS_OBJ) | $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(PAR): $(OBJ)/conta-objetos-paralelo.o $(COMUNS_OBJ) | $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# dependencias de cabecalho
$(OBJ)/matriz.o: $(SRC)/matriz.c $(SRC)/matriz.h
$(OBJ)/sequencial.o: $(SRC)/sequencial.c $(SRC)/sequencial.h $(SRC)/matriz.h
$(OBJ)/tempo.o: $(SRC)/tempo.c $(SRC)/tempo.h
$(OBJ)/conta-objetos-sequencial.o: $(SRC)/conta-objetos-sequencial.c \
	$(SRC)/matriz.h $(SRC)/sequencial.h $(SRC)/tempo.h
$(OBJ)/conta-objetos-paralelo.o: $(SRC)/conta-objetos-paralelo.c \
	$(SRC)/matriz.h $(SRC)/sequencial.h $(SRC)/tempo.h

teste: all
	./tests/executar-testes.sh

bench: all
	./tests/medir-desempenho.sh

# Verifica ausencia de condicoes de corrida com o ThreadSanitizer.
# Exige gcc ou clang com suporte a -fsanitize=thread.
tsan: | $(BIN)
	$(CC) -std=c89 -Wall -Wextra -pedantic -g -fsanitize=thread -pthread \
		$(SRC)/conta-objetos-paralelo.c $(COMUNS_SRC) -o $(BIN)/par-tsan
	$(BIN)/par-tsan -g 500 400 45 3 -n 8 -c
	$(BIN)/par-tsan -g 300 300 30 9 -n 4 -c

clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all teste bench tsan clean
