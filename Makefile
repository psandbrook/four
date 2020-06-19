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

GLAD_SOURCE := depends/glad/src/glad.c
GLAD_OBJECT := $(BUILD_DIR)/glad.o

WARN_FLAGS := \
	-Wall -Wextra -Wpedantic -Wcast-align -Wcast-qual -Wunused -Wdisabled-optimization -Wformat=2 -Winit-self \
	-Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wredundant-decls -Wshadow -Wundef -Wfloat-equal \
	-Wstack-protector -Wwrite-strings -Wrestrict -Wconversion -Wvla -Wuninitialized -Wctor-dtor-privacy -Wnoexcept \
	-Woverloaded-virtual -Wsign-promo -Wstrict-null-sentinel -Wuseless-cast -Wzero-as-null-pointer-constant

DEBUG_FLAGS := -O0 -g3 -fvar-tracking-assignments -fsanitize=undefined

RELEASE_FLAGS := \
	-DNDEBUG -D_FORTIFY_SOURCE=2 -ffunction-sections -fdata-sections -Wl,--gc-sections,-O1 -flto -fuse-linker-plugin \
	-march=x86-64 -mtune=generic -O3 -g0

DEP_FLAGS_IN := $(shell pkg-config --cflags --libs sdl2)
DEP_FLAGS := $(DEP_FLAGS_IN:-I%=-isystem %) -ldl -isystem depends/glad/include -isystem depends/Handmade-Math

FLAGS := $(WARN_FLAGS) $(DEP_FLAGS)

ifdef RELEASE
	FLAGS += $(RELEASE_FLAGS)
	STRIP_CMD := strip --strip-unneeded
else
	FLAGS += $(DEBUG_FLAGS)
	STRIP_CMD := @:
endif

FLAGS += \
	-m64 -std=c++17 -pipe -fvisibility=hidden -fPIE -pie -fstack-protector-strong -fno-plt \
	-Wl,--sort-common,--as-needed -z relro -z now -z defs

.PHONY: all clean
all: $(PREFIXED_BIN)

clean:
	rm -rf $(BUILD_DIR)

$(PREFIXED_BIN): $(OBJECTS) $(GLAD_OBJECT)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(FLAGS) -o $@ $^
	$(STRIP_CMD) $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(FLAGS) -c -o $@ $<

$(GLAD_OBJECT): $(GLAD_SOURCE)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(FLAGS) -w -c -o $@ $<
