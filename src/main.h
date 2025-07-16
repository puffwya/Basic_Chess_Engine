#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

// Tell the compiler this is C-style linkage when included from C++ files
#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t board[64];

bool isValidMove(int from, int to);
bool wouldKingBeInCheckAfterMove(int from, int to);
bool makeMove(int from, int to);
bool isInCheck(bool white);
int evaluateBoard();

#ifdef __cplusplus
}
#endif

#endif // MAIN_H
