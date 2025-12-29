CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
BUILD_DIR ?= build

all: $(BUILD_DIR)/ionc

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/main.o: src/main.cpp src/lexer.h src/parser.h src/codegen.h src/ast.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/main.o src/main.cpp

$(BUILD_DIR)/lexer.o: src/lexer.cpp src/lexer.h src/ast.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/lexer.o src/lexer.cpp

$(BUILD_DIR)/parser.o: src/parser.cpp src/parser.h src/ast.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/parser.o src/parser.cpp

$(BUILD_DIR)/codegen.o: src/codegen.cpp src/codegen.h src/ast.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/codegen.o src/codegen.cpp

$(BUILD_DIR)/ionc: $(BUILD_DIR)/main.o $(BUILD_DIR)/lexer.o $(BUILD_DIR)/parser.o $(BUILD_DIR)/codegen.o
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/ionc $(BUILD_DIR)/main.o $(BUILD_DIR)/lexer.o $(BUILD_DIR)/parser.o $(BUILD_DIR)/codegen.o

clean:
	rm -rf $(BUILD_DIR)
