CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS = -Iinclude -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L

BUILD   = build
TARGET  = $(BUILD)/servidor
TEST_BIN = $(BUILD)/test_runner

LIB_SRC = src/http.c src/mime.c src/server.c
LIB_OBJ = $(LIB_SRC:src/%.c=$(BUILD)/%.o)

.PHONY: all test integration debug valgrind run clean

all: $(TARGET)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: src/%.c include/*.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TARGET): $(LIB_OBJ) $(BUILD)/main.o
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_BIN): $(LIB_OBJ) tests/test_main.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

# Sobe o servidor e testa requisições reais com curl
integration: $(TARGET)
	bash tests/integration.sh

# Compila com símbolos de depuração e sanitizers (AddressSanitizer + UBSan)
# (usa um diretório separado para não misturar objetos com o build normal)
debug:
	$(MAKE) BUILD=build/debug CFLAGS="-std=c11 -Wall -Wextra -Wpedantic -g -O0 -fsanitize=address,undefined" all test

valgrind: $(TEST_BIN)
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_BIN)

run: $(TARGET)
	./$(TARGET) -p 8080 -d www

clean:
	rm -rf $(BUILD)
