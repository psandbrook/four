# Set the default C compiler to gcc
ifeq ($(origin CC),default)
	CC := gcc
endif

# Set the default C++ compiler to g++
ifeq ($(origin CXX),default)
	CXX := g++
endif

# TODO: Detect changes in header files

SOURCE_DIR := src
SOURCE_SUBDIRS := four
BUILD_DIR := build
BUILD_SUBDIRS := $(addprefix $(BUILD_DIR)/,$(SOURCE_SUBDIRS))

SOURCES := main.cpp four/mesh.cpp four/generate.cpp

BIN := 4dtest
PREFIXED_BIN := $(addprefix $(BUILD_DIR)/,$(BIN))

OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.cpp=.o))

GLAD_SOURCE := depends/glad/src/glad.c
GLAD_OBJECT := $(BUILD_DIR)/glad.o

TRIANGLE_SOURCE := depends/triangle/triangle.c
TRIANGLE_OBJECT := $(BUILD_DIR)/triangle.o

WARN_FLAGS := \
	-Wall -Wextra -Wcast-align -Wcast-qual -Wunused -Wdisabled-optimization -Wformat=2 -Winit-self \
	-Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wredundant-decls -Wshadow -Wundef -Wfloat-equal \
	-Wstack-protector -Wwrite-strings -Wrestrict -Wconversion -Wvla -Wuninitialized -Wctor-dtor-privacy -Wnoexcept \
	-Woverloaded-virtual -Wsign-promo -Wstrict-null-sentinel -Wuseless-cast

DEBUG_FLAGS := -O0 -g3 -fvar-tracking -fvar-tracking-assignments -fsanitize=undefined

RELEASE_FLAGS := \
	-DNDEBUG -D_FORTIFY_SOURCE=2 -ffunction-sections -fdata-sections -Wl,--gc-sections,-O1 -flto -fuse-linker-plugin \
	-march=x86-64 -mtune=generic -O3 -g0

OPTDEBUG_FLAGS := \
	-march=x86-64 -mtune=generic -Og -g3 -fvar-tracking -fvar-tracking-assignments

DEP_FLAGS_IN := $(shell pkg-config --cflags --libs sdl2)
DEP_FLAGS := \
	$(DEP_FLAGS_IN:-I%=-isystem %) -isystem depends/glad/include -isystem depends/Handmade-Math \
	-isystem depends/libigl/include -isystem depends/eigen -isystem depends/triangle -ldl -lm -DLINUX \
	-DTRILIBRARY -DANSI_DECLARATORS

DEP_C_FLAGS := \
	-DNDEBUG -D_FORTIFY_SOURCE=2 -ffunction-sections -fdata-sections -Wl,--gc-sections,-O1 \
	-march=x86-64 -mtune=generic -O3 -g0 -m64 -std=gnu11 -pipe -fvisibility=hidden -fPIE -pie \
	-fstack-protector-strong -fno-plt -Wl,--sort-common,--as-needed -z relro -z now -z defs

FLAGS := $(WARN_FLAGS) $(DEP_FLAGS) -I$(SOURCE_DIR)
C_FLAGS := $(DEP_FLAGS) $(DEP_C_FLAGS)

ifdef RELEASE
	FLAGS += $(RELEASE_FLAGS)
	STRIP_CMD := strip --strip-unneeded
else ifdef OPTDEBUG
	FLAGS += $(OPTDEBUG_FLAGS)
	STRIP_CMD := @:
else
	FLAGS += $(DEBUG_FLAGS)
	STRIP_CMD := @:
endif

FLAGS += \
	-m64 -std=gnu++17 -pipe -fvisibility=hidden -fPIE -pie -fstack-protector-strong -fno-plt \
	-Wl,--sort-common,--as-needed -z relro -z now -z defs -fno-strict-aliasing

.PHONY: all clean
all: $(PREFIXED_BIN)

clean:
	rm -rf $(BUILD_DIR)

$(PREFIXED_BIN): $(OBJECTS) $(GLAD_OBJECT) $(TRIANGLE_OBJECT)
	$(CXX) $(FLAGS) -o $@ $^
	$(STRIP_CMD) $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR) $(BUILD_SUBDIRS)
	$(CXX) $(FLAGS) -c -o $@ $<

$(GLAD_OBJECT): $(GLAD_SOURCE)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(FLAGS) -w -c -o $@ $<

$(TRIANGLE_OBJECT): $(TRIANGLE_SOURCE)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(C_FLAGS) -w -c -o $@ $<
