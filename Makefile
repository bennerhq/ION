CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
BUILD_DIR ?= build

all: $(BUILD_DIR)/ionc

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/main.o: src/main.cpp src/lexer.h src/parser.h src/codegen.h src/ast.h src/common.h src/module_loader.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/main.o src/main.cpp

$(BUILD_DIR)/lexer.o: src/lexer.cpp src/lexer.h src/ast.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/lexer.o src/lexer.cpp

$(BUILD_DIR)/parser.o: src/parser.cpp src/parser.h src/ast.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/parser.o src/parser.cpp

$(BUILD_DIR)/codegen.o: src/codegen.cpp src/codegen.h src/codegen_types.h src/runtime_emitter.h src/struct_layout.h src/type_resolver.h src/ast.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/codegen.o src/codegen.cpp

$(BUILD_DIR)/module_loader.o: src/module_loader.cpp src/module_loader.h src/lexer.h src/parser.h src/ast.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/module_loader.o src/module_loader.cpp

$(BUILD_DIR)/type_resolver.o: src/type_resolver.cpp src/type_resolver.h src/codegen_types.h src/ast.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/type_resolver.o src/type_resolver.cpp

$(BUILD_DIR)/struct_layout.o: src/struct_layout.cpp src/struct_layout.h src/type_resolver.h src/codegen_types.h src/ast.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/struct_layout.o src/struct_layout.cpp

$(BUILD_DIR)/runtime_emitter.o: src/runtime_emitter.cpp src/runtime_emitter.h src/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(BUILD_DIR)/runtime_emitter.o src/runtime_emitter.cpp

$(BUILD_DIR)/ionc: $(BUILD_DIR)/main.o $(BUILD_DIR)/lexer.o $(BUILD_DIR)/parser.o $(BUILD_DIR)/codegen.o $(BUILD_DIR)/module_loader.o $(BUILD_DIR)/type_resolver.o $(BUILD_DIR)/struct_layout.o $(BUILD_DIR)/runtime_emitter.o
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/ionc $(BUILD_DIR)/main.o $(BUILD_DIR)/lexer.o $(BUILD_DIR)/parser.o $(BUILD_DIR)/codegen.o $(BUILD_DIR)/module_loader.o $(BUILD_DIR)/type_resolver.o $(BUILD_DIR)/struct_layout.o $(BUILD_DIR)/runtime_emitter.o

clean:
	rm -rf $(BUILD_DIR)
