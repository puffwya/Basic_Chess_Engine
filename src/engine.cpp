#include "engine.h"
#include "main.h"
#include <vector>
#include <cstdlib>

extern int pendingPromotionSquare; 

// Simple piece values
const int pieceValues[13] = {
    0,   // Empty
    100, // White Pawn
    100, // Black Pawn
    320, // White Knight
    320, // Black Knight
    330, // White Bishop
    330, // Black Bishop
    500, // White Rook
    500, // Black Rook
    900, // White Queen
    900, // Black Queen
    20000, // White King
    20000  // Black King
};

int evaluateBoard() {
    int score = 0;
    for (int i = 0; i < 64; ++i) {
        int piece = board[i];
        if (piece == 0) continue;
        bool isWhite = (piece % 2 == 1);
        int value = pieceValues[piece];
        score += isWhite ? value : -value;
    }
    return score;
}

int minimax(int depth, int alpha, int beta, bool maximizingPlayer) {
    if (depth == 0) {
        return evaluateBoard();
    }
    
    int bestScore = maximizingPlayer ? -1000000 : 1000000;
    bool moveFound = false;
    
    for (int from = 0; from < 64; ++from) {
        if (board[from] == 0 || (board[from] % 2 == 1) != maximizingPlayer) continue;
    
        for (int to = 0; to < 64; ++to) {
            if (!isValidMove(from, to)) continue;
            if (wouldKingBeInCheckAfterMove(from, to)) continue;
    
            moveFound = true;  // At least one move found
    
            // Save board state
            uint8_t backupTo = board[to];
            board[to] = board[from];
            board[from] = 0;

            int score = minimax(depth - 1, alpha, beta, !maximizingPlayer);
    
            // Undo move
            board[from] = board[to];
            board[to] = backupTo;
        
            if (maximizingPlayer) {
                bestScore = std::max(bestScore, score);
                alpha = std::max(alpha, score);
            } else {
                bestScore = std::min(bestScore, score);
                beta = std::min(beta, score);
            }
        
            if (beta <= alpha) break;
        }
    }

    if (!moveFound) {
        // No legal moves: check if in check
        bool inCheck = isInCheck(maximizingPlayer);
        if (inCheck) {
            // Checkmate: bad for maximizing player
            return maximizingPlayer ? -1000000 : 1000000;
        } else {
            // Stalemate: draw
            return 0;
        }
    }

    return bestScore;
}

int findBestMove(bool white) {
    int bestScore = white ? -1000000 : 1000000;
    int bestMove = -1;

    // Store the first legal move as fallback
    int fallbackMove = -1;

    for (int from = 0; from < 64; ++from) {
        if (board[from] == 0 || (board[from] % 2 == 1) != white) continue;

        for (int to = 0; to < 64; ++to) {
            if (!isValidMove(from, to)) continue;
            if (wouldKingBeInCheckAfterMove(from, to)) continue;

            // Save first legal move in case all scores are bad
            if (fallbackMove == -1) fallbackMove = from * 64 + to;

            // Save board state
            uint8_t backupTo = board[to];
            board[to] = board[from];
            board[from] = 0;

            int score = minimax(2, -1000000, 1000000, !white);

            // Undo move
            board[from] = board[to];
            board[to] = backupTo;

            if ((white && score > bestScore) || (!white && score < bestScore)) {
                bestScore = score;
                bestMove = from * 64 + to;
            }
        }
    }

    // If no good move was found, fall back to a legal one
    if (bestMove == -1) bestMove = fallbackMove;

    return bestMove;
}

extern "C" {

    bool makeAIMove() {
        pendingPromotionSquare = -1;  // Clear any leftover promotion state

        int move = findBestMove(false); // false = black
        if (move == -1) return false;

        int from = move / 64;
        int to = move % 64;

        return makeMove(from, to);  // Calls the real makeMove from main.cpp
    }

}
