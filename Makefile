NAME = dune
SHARED = libdune.so
SHARED_WIN = libdune.dll

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

# Windows cross-compile (mingw-w64). The posix thread model is mandatory:
# the FFI's FFIAsyncAdapter relies on std::thread / std::condition_variable
# (PR 4b), which are stubs under the default win32 thread model.
CXX_WIN = x86_64-w64-mingw32-g++-posix
CXXFLAGS_WIN = -Wall -Wextra -Werror -g3 -std=c++17

OBJS_DIR = objs
OBJS_DIR_WIN = objs_win

CORE_OBJS    = $(patsubst $(SRCS_DIR)/%.cpp, $(OBJS_DIR)/%.o, $(CORE_SRCS))
CLI_ONLY_OBJS = $(patsubst $(SRCS_DIR)/%.cpp, $(OBJS_DIR)/%.o, $(CLI_ONLY_SRCS))
FFI_OBJS     = $(patsubst $(SRCS_DIR)/%.cpp, $(OBJS_DIR)/%.o, $(FFI_SRCS))

# Mirror objects for the Windows build under objs_win/ so the two trees
# don't collide. Only core + ffi sources go into libdune.dll (no CLI bits).
CORE_OBJS_WIN = $(patsubst $(SRCS_DIR)/%.cpp, $(OBJS_DIR_WIN)/%.o, $(CORE_SRCS))
FFI_OBJS_WIN  = $(patsubst $(SRCS_DIR)/%.cpp, $(OBJS_DIR_WIN)/%.o, $(FFI_SRCS))

# Tests
TESTS_DIR = tests
TEST_SRCS = $(wildcard $(TESTS_DIR)/*.cpp)
TEST_OBJS = $(patsubst $(TESTS_DIR)/%.cpp, $(OBJS_DIR)/tests/%.o, $(TEST_SRCS))
TEST_BIN = $(TESTS_DIR)/run

# FFI smoke tests (pure C, link libdune.so).
FFI_SMOKE_SRC = $(TESTS_DIR)/ffi_smoke.c
FFI_SMOKE_BIN = $(TESTS_DIR)/ffi_smoke
FFI_SMOKE_INTERACTIVE_SRC = $(TESTS_DIR)/ffi_smoke_interactive.c
FFI_SMOKE_INTERACTIVE_BIN = $(TESTS_DIR)/ffi_smoke_interactive

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

# Windows DLL: same source set as libdune.so, cross-compiled via mingw-w64.
# -static-libgcc / -static-libstdc++ avoids requiring libstdc++-6.dll etc to
# travel alongside the DLL on the host machine. -Wl,--export-all-symbols
# surfaces the C ABI on Windows (Linux exports everything by default).
# -Wl,--out-implib emits the import library so other Windows code can link
# against it.
$(SHARED_WIN): $(CORE_OBJS_WIN) $(FFI_OBJS_WIN)
	@echo "$(ORANGE)$(SHARED_WIN)$(NC) linking..."
	@$(CXX_WIN) $(CXXFLAGS_WIN) -shared -static-libgcc -static-libstdc++ \
		-Wl,--export-all-symbols \
		-Wl,--out-implib,$(SHARED_WIN).a \
		-o $@ $(CORE_OBJS_WIN) $(FFI_OBJS_WIN) $(INCLUDES)
	@echo "$(ORANGE)$(SHARED_WIN)$(NC) ready!"

windows-shared: $(SHARED_WIN)

$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJS_DIR_WIN)/%.o: $(SRCS_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(INCLUDES) -c $< -o $@

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

# Interactive FFI smoke (PR 4b): drives a full game via the v2 surface
# (create_interactive / step / get_pending_decision / submit_decision).
$(FFI_SMOKE_INTERACTIVE_BIN): $(FFI_SMOKE_INTERACTIVE_SRC) $(SHARED)
	@echo "$(BLUE)ffi_smoke_interactive$(NC) linking..."
	@$(CC) -Wall -Wextra -Werror -I $(INCLUDES_DIR) -o $@ $(FFI_SMOKE_INTERACTIVE_SRC) -L. -ldune
	@echo "$(BLUE)ffi_smoke_interactive$(NC) ready!"

ffi_smoke_interactive: $(FFI_SMOKE_INTERACTIVE_BIN)
	@LD_LIBRARY_PATH=. ./$(FFI_SMOKE_INTERACTIVE_BIN)

clean:
	@rm -rf $(OBJS_DIR) $(OBJS_DIR_WIN)
	@rm -f $(TEST_BIN) $(FFI_SMOKE_BIN) $(FFI_SMOKE_INTERACTIVE_BIN)
	@echo "$(RED)$(NAME)$(NC) OBJS cleaned!"

fclean: clean
	@rm -f $(NAME) $(SHARED) $(SHARED_WIN) $(SHARED_WIN).a
	@echo "$(RED)$(NAME)$(NC) cleaned!"

fcount:
	wc -l $(SRCS_DIR)/*.cpp $(SRCS_DIR)/**/*.cpp $(HEADERS)

re: fclean all

.PHONY: all shared windows-shared clean fclean re tests ffi_smoke ffi_smoke_interactive
