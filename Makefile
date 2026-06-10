CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
TARGET = mini_compiler

all: $(TARGET)

$(TARGET): src/main.cpp src/common.h src/lexer.h src/recursive_descent_parser.h src/predictive_parser.h src/symbol_table.h src/lr_parser.h
	$(CXX) $(CXXFLAGS) src/main.cpp -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) mini_compiler.exe
