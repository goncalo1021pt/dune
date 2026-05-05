NAME = dune
SHARED = libdune.so

SRCS_DIR = srcs

# All sources under srcs/.
ALL_SRCS = $(wildcard $(SRCS_DIR)/*.cpp $(SRCS_DIR)/**/*.cpp)

# CLI-only sources: ship in `dune`, do NOT ship in libdune.so. These hold
# the signal handler and GameDebugger singleton, both of which assume a
# single live process-wide game.
CLI_ONLY_SRCS = $(SRCS_DIR)/main.cpp $(SRCS_DIR)/sighandler.cpp $(SRCS_DIR)/debug.cpp

# FFI sources: ship in libdune.so, do NOT ship in `dune` (the C ABI symbols
# would otherwise pollute the executable's symbol table).
FFI_SRCS = $(wildcard $(SRCS_DIR)/ffi/*.cpp)

# Core engine: shared between `dune` and libdune.so + the test runner.
CORE_SRCS = $(filter-out $(CLI_ONLY_SRCS) $(FFI_SRCS), $(ALL_SRCS))

INCLUDES_DIR = includes/headers
THIRD_PARTY_DIR = includes/third_party
INCLUDES = -I $(INCLUDES_DIR) -I $(THIRD_PARTY_DIR)
HEADERS = $(wildcard $(INCLUDES_DIR)/*.hpp $(INCLUDES_DIR)/*.h $(INCLUDES_DIR)/**/*.hpp $(INCLUDES_DIR)/**/*.h)

CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -g3 -std=c++17 -fPIC

OBJS_DIR = objs

CORE_OBJS    = $(patsubst $(SRCS_DIR)/%.cpp, $(OBJS_DIR)/%.o, $(CORE_SRCS))
CLI_ONLY_OBJS = $(patsubst $(SRCS_DIR)/%.cpp, $(OBJS_DIR)/%.o, $(CLI_ONLY_SRCS))
FFI_OBJS     = $(patsubst $(SRCS_DIR)/%.cpp, $(OBJS_DIR)/%.o, $(FFI_SRCS))

# Tests
TESTS_DIR = tests
TEST_SRCS = $(wildcard $(TESTS_DIR)/*.cpp)
TEST_OBJS = $(patsubst $(TESTS_DIR)/%.cpp, $(OBJS_DIR)/tests/%.o, $(TEST_SRCS))
TEST_BIN = $(TESTS_DIR)/run

# FFI smoke test (pure C, links libdune.so).
FFI_SMOKE_SRC = $(TESTS_DIR)/ffi_smoke.c
FFI_SMOKE_BIN = $(TESTS_DIR)/ffi_smoke

# Tests reuse the same object files as the engine — no need to recompile.
# The C ABI surface (srcs/ffi/dune_c_api.cpp) is excluded because its
# extern "C" symbols don't make sense in a doctest binary; the snapshot
# builder is included so SnapshotBuilder/EventSerialization can be tested.
TEST_FFI_OBJS = $(filter-out $(OBJS_DIR)/ffi/dune_c_api.o, $(FFI_OBJS))
ENGINE_OBJS = $(CORE_OBJS) $(TEST_FFI_OBJS)

# Color codes
GREEN = \033[0;32m
RED = \033[0;31m
BLUE = \033[0;34m
ORANGE = \033[0;33m
NC = \033[0m

all: $(NAME)

$(NAME): $(CORE_OBJS) $(CLI_ONLY_OBJS)
	@echo "$(GREEN)$(NAME)$(NC) compiling..."
	@$(CXX) $(CXXFLAGS) -o $(NAME) $(CORE_OBJS) $(CLI_ONLY_OBJS) $(INCLUDES)
	@echo "$(GREEN)$(NAME)$(NC) ready!"

# Shared library (PR 4a). Built from core + ffi objects only — main.cpp and
# the signal handler are deliberately absent. -fPIC is already in CXXFLAGS
# so all object files are PIC-clean.
$(SHARED): $(CORE_OBJS) $(FFI_OBJS)
	@echo "$(ORANGE)$(SHARED)$(NC) linking..."
	@$(CXX) $(CXXFLAGS) -shared -Wl,-soname,$(SHARED) -o $@ $(CORE_OBJS) $(FFI_OBJS) $(INCLUDES)
	@echo "$(ORANGE)$(SHARED)$(NC) ready!"

shared: $(SHARED)

$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJS_DIR)/tests/%.o: $(TESTS_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_BIN): $(ENGINE_OBJS) $(TEST_OBJS)
	@echo "$(BLUE)tests$(NC) linking..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDES)
	@echo "$(BLUE)tests$(NC) ready!"

tests: $(TEST_BIN)
	@./$(TEST_BIN)

# FFI smoke test: pure C, links libdune.so. Runs from the build dir with
# LD_LIBRARY_PATH=. so it finds the .so without an install step.
$(FFI_SMOKE_BIN): $(FFI_SMOKE_SRC) $(SHARED)
	@echo "$(BLUE)ffi_smoke$(NC) linking..."
	@$(CC) -Wall -Wextra -Werror -I $(INCLUDES_DIR) -o $@ $(FFI_SMOKE_SRC) -L. -ldune
	@echo "$(BLUE)ffi_smoke$(NC) ready!"

ffi_smoke: $(FFI_SMOKE_BIN)
	@LD_LIBRARY_PATH=. ./$(FFI_SMOKE_BIN)

clean:
	@rm -rf $(OBJS_DIR)
	@rm -f $(TEST_BIN) $(FFI_SMOKE_BIN)
	@echo "$(RED)$(NAME)$(NC) OBJS cleaned!"

fclean: clean
	@rm -f $(NAME) $(SHARED)
	@echo "$(RED)$(NAME)$(NC) cleaned!"

fcount:
	wc -l $(SRCS_DIR)/*.cpp $(SRCS_DIR)/**/*.cpp $(HEADERS)

re: fclean all

.PHONY: all shared clean fclean re tests ffi_smoke
