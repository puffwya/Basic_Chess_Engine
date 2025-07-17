EMCC = emcc
SRC = src/main.cpp src/engine.cpp
OUT_DIR = docs
OUT_JS = $(OUT_DIR)/index.js

EXPORTED_FUNCS = "['_initBoard', '_getBoard', '_makeMove', '_getPendingPromotionSquare', '_promotePawn', '_currentTurn', '_isInCheck', '_isCheckmate', '_isStalemate', '_isInsufficientMaterial', '_makeAIMove', '_setCurrentTurn']"
EXPORTED_RUNTIME = "['ccall', 'cwrap', 'HEAPU8']"

$(OUT_JS): $(SRC)
	@echo "🔧 Compiling $(SRC) → $(OUT_JS)..."
	$(EMCC) $(SRC) -s WASM=1 -o $(OUT_JS) \
		-s EXPORTED_FUNCTIONS=$(EXPORTED_FUNCS) \
		-s EXPORTED_RUNTIME_METHODS=$(EXPORTED_RUNTIME)

build: $(OUT_JS)
	@echo "✅ Build finished and saved to $(OUT_DIR)"

clean:
	rm -f $(OUT_DIR)/index.js $(OUT_DIR)/index.wasm
	@echo "🧹 Cleaned build artifacts."

