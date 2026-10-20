CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -MMD -MP -O2

SRC_DIR = src
BUILD_DIR = build
BUILD_ASAN_DIR = build_asan

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))
ASAN_OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_ASAN_DIR)/%.o, $(SRCS))

TARGET = RapidDB
TARGET_ASAN = RapidDB-asan

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

# AddressSanitizer + UndefinedBehaviorSanitizer Target
asan: $(TARGET_ASAN)

$(BUILD_ASAN_DIR):
	mkdir -p $(BUILD_ASAN_DIR)

$(BUILD_ASAN_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_ASAN_DIR)
	$(CXX) -std=c++17 -Wall -Wextra -pthread -fsanitize=address,undefined -g -O1 -MMD -MP -c $< -o $@

$(TARGET_ASAN): $(ASAN_OBJS)
	$(CXX) -std=c++17 -Wall -Wextra -pthread -fsanitize=address,undefined -g -O1 $(ASAN_OBJS) -o $(TARGET_ASAN)

test: all
	python3 tests/chaos_test.py
	python3 tests/memory_leak_test.py

clean:
	rm -rf $(BUILD_DIR) $(BUILD_ASAN_DIR) $(TARGET) $(TARGET_ASAN) dump.rapiddb

rebuild: clean all

run: all
	./$(TARGET)

.PHONY: all asan test clean rebuild run
