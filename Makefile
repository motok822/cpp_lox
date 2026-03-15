CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I.
TARGET   := clox
SRCS     := src/main.cpp include/value.cpp include/compiler.cpp
OBJS     := $(SRCS:.cpp=.o)

TEST_TARGET := test_runner
TEST_SRC    := tests/main.cpp

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_TARGET): $(TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

test: $(TARGET) $(TEST_TARGET)
	./$(TEST_TARGET) ./$(TARGET) tests

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_TARGET)

.PHONY: all test clean
