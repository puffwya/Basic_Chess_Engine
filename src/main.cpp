#include <emscripten.h>

extern "C" {

// Use plain enums for WASM compatibility
enum Piece {
    EMPTY = 0,
    PAWN = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK = 4,
    QUEEN = 5,
    KING = 6
};

enum Color {
    NONE = 0,
    WHITE = 1,
    BLACK = 2
};

// Each square stores piece + color
typedef struct {
    int piece;
    int color;
} Square;

// 8x8 board (global for now)
Square board[8][8];

// Set up standard chess position
void initBoard() {
    // Clear board
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            board[row][col] = { EMPTY, NONE };

    // Place pawns
    for (int col = 0; col < 8; ++col) {
        board[1][col] = { PAWN, WHITE };
        board[6][col] = { PAWN, BLACK };
    }

    // Place major pieces (white bottom, black top)
    Piece layout[8] = { ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK };

    for (int col = 0; col < 8; ++col) {
        board[0][col] = { layout[col], WHITE };
        board[7][col] = { layout[col], BLACK };
    }
}

// Expose board as a flat pointer for JS to read
const Square* getBoard() {
    return &board[0][0];
}

}
