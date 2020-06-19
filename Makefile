# Set the default C++ compiler to g++
ifeq ($(origin CXX),default)
	CXX := g++
endif

SOURCE_DIR := src
BUILD_DIR := build

SOURCES := main.cpp

BIN := 4dtest
PREFIXED_BIN := $(addprefix $(BUILD_DIR)/,$(BIN))

OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.cpp=.o))

WARN_CXXFLAGS := \
	-Wall -Wextra -Wpedantic -Wcast-align -Wcast-qual -Wunused -Wdisabled-optimization -Wformat=2 -Winit-self \
	-Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wredundant-decls -Wshadow -Wundef -Wfloat-equal \
	-Wstack-protector -Wwrite-strings -Wrestrict -Wconversion -Wvla

DEBUG_CXXFLAGS := -O0 -g3 -fvar-tracking-assignments -fsanitize=undefined

RELEASE_CXXFLAGS := \
	-DNDEBUG -D_FORTIFY_SOURCE=2 -ffunction-sections -fdata-sections -Wl,--gc-sections,-O1 -flto -fuse-linker-plugin \
	-march=x86-64 -mtune=generic -O3 -g0

CXXFLAGS := $(WARN_CXXFLAGS)

ifdef RELEASE
	CXXFLAGS += $(RELEASE_CXXFLAGS)
	STRIP_CMD := strip --strip-unneeded
else
	CXXFLAGS += $(DEBUG_CXXFLAGS)
	STRIP_CMD := @:
endif

CXXFLAGS += \
	-m64 -std=c++17 -pipe -fvisibility=hidden -fPIE -pie -fstack-protector-strong -fno-plt \
	-Wl,--sort-common,--as-needed -z relro -z now -z defs

.PHONY: all clean
all: $(PREFIXED_BIN)

clean:
	rm -rf $(BUILD_DIR)

$(PREFIXED_BIN): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^
	$(STRIP_CMD) $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
