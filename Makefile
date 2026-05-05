CC=clang++
ANTLR=antlr4
ANTLR_RUNTIME_INC=/usr/include/antlr4-runtime
ANTLR_RUNTIME_LIB=/usr/lib

CFLAGS=-std=c++17 -Iinclude -Isrc/generated -I$(ANTLR_RUNTIME_INC) $(shell llvm-config --cppflags)
LDFLAGS=$(shell llvm-config --ldflags --libs --system-libs) -L$(ANTLR_RUNTIME_LIB) -lantlr4-runtime

GEN_SRC=src/generated/LuvLexer.cpp src/generated/LuvParser.cpp src/generated/LuvBaseVisitor.cpp src/generated/LuvVisitor.cpp
SRC=src/main.cpp src/ast/AST.cpp src/CodeGen.cpp src/rss/RSSPipeline.cpp src/rss/MetaprogrammingEngine.cpp $(GEN_SRC)
OBJ=$(SRC:.cpp=.o)

BLV_SRC=src/blv/blv_main.cpp
BLV_OBJ=$(BLV_SRC:.cpp=.o)

LUV_CLI_SRC=src/cli/luv_cli.cpp
LUV_CLI_OBJ=$(LUV_CLI_SRC:.cpp=.o)

RUNTIME_SRC=src/runtime/luv_string.cpp
RUNTIME_OBJ=$(RUNTIME_SRC:.cpp=.o)

all: bin/luvc bin/blv bin/luv bin/luv_string.o

$(GEN_SRC): src/Luv.g4
	mkdir -p src/generated
	$(ANTLR) -Dlanguage=Cpp -visitor -o src/generated -Xexact-output-dir src/Luv.g4

$(OBJ): $(GEN_SRC)

bin/luvc: $(OBJ)
	mkdir -p bin
	$(CC) $(OBJ) -o bin/luvc $(LDFLAGS)

bin/blv: $(BLV_OBJ)
	mkdir -p bin
	$(CC) $(BLV_OBJ) -o bin/blv -ldl -std=c++17

bin/luv: $(LUV_CLI_OBJ)
	mkdir -p bin
	$(CC) $(LUV_CLI_OBJ) -o bin/luv -std=c++17

bin/luv_string.o: src/runtime/luv_string.cpp
	mkdir -p bin
	$(CC) -std=c++17 -c src/runtime/luv_string.cpp -o bin/luv_string.o

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

src/blv/%.o: src/blv/%.cpp
	$(CC) -std=c++17 -Iinclude -c $< -o $@

src/cli/%.o: src/cli/%.cpp
	$(CC) -std=c++17 -Iinclude -c $< -o $@

clean:
	rm -rf src/generated bin src/*.o src/ast/*.o src/blv/*.o src/cli/*.o src/runtime/*.o src/rss/*.o

