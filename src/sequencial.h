/* sequencial.h - contagem de objetos com conectividade 8, versao de
 * referencia (um unico fluxo de controle).
 * Sistemas Operacionais - PUCRS - 2026/II
 */

#ifndef SEQUENCIAL_H
#define SEQUENCIAL_H

#include "matriz.h"

/* Conta os objetos (componentes conexas com conectividade 8) da matriz.
 * Usa preenchimento por inundacao iterativo, com pilha explicita, para
 * evitar recursao profunda em matrizes grandes.
 * Retorna a quantidade de objetos, ou -1 se faltar memoria. */
long conta_objetos_sequencial(const Matriz *m);

#endif /* SEQUENCIAL_H */
