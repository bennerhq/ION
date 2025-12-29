CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -MMD -MP
BUILD_DIR ?= build
SRC_DIR ?= src

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

all: $(BUILD_DIR)/ionc

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/ionc: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)
