#include <iostream>

enum Piece {
    EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
};

enum Color {
    NONE, WHITE, BLACK
};

struct Square {
    Piece piece;
    Color color;
};

Square board[8][8];

// Unicode symbols for pieces (White: uppercase, Black: lowercase)
char getSymbol(Square sq) {
    if (sq.piece == EMPTY) return '.';

    switch (sq.piece) {
        case PAWN:   return sq.color == WHITE ? 'P' : 'p';
        case KNIGHT: return sq.color == WHITE ? 'N' : 'n';
        case BISHOP: return sq.color == WHITE ? 'B' : 'b';
        case ROOK:   return sq.color == WHITE ? 'R' : 'r';
        case QUEEN:  return sq.color == WHITE ? 'Q' : 'q';
        case KING:   return sq.color == WHITE ? 'K' : 'k';
        default:     return '?';
    }
}

extern "C" {

    void initBoard() {
        // Empty squares
        for (int r = 2; r < 6; ++r) {
            for (int c = 0; c < 8; ++c) {
                board[r][c] = { EMPTY, NONE };
            }
        }

        // Pawns
        for (int c = 0; c < 8; ++c) {
            board[1][c] = { PAWN, WHITE };
            board[6][c] = { PAWN, BLACK };
        }

        // Rooks
        board[0][0] = board[0][7] = { ROOK, WHITE };
        board[7][0] = board[7][7] = { ROOK, BLACK };

        // Knights
        board[0][1] = board[0][6] = { KNIGHT, WHITE };
        board[7][1] = board[7][6] = { KNIGHT, BLACK };

        // Bishops
        board[0][2] = board[0][5] = { BISHOP, WHITE };
        board[7][2] = board[7][5] = { BISHOP, BLACK };

        // Queens
        board[0][3] = { QUEEN, WHITE };
        board[7][3] = { QUEEN, BLACK };

        // Kings
        board[0][4] = { KING, WHITE };
        board[7][4] = { KING, BLACK };
    }

    void printBoard() {
        for (int r = 7; r >= 0; --r) {
            std::cout << r + 1 << " ";
            for (int c = 0; c < 8; ++c) {
                std::cout << getSymbol(board[r][c]) << " ";
            }
            std::cout << "\n";
        }
        std::cout << "  a b c d e f g h\n";
    }
}

int main() {
    initBoard();
    printBoard();
    return 0;
}

