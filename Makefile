NAME = dune

SRCS_DIR = srcs
SRCS = $(wildcard $(SRCS_DIR)/*.cpp $(SRCS_DIR)/**/*.cpp)

INCLUDES_DIR = includes/headers
THIRD_PARTY_DIR = includes/third_party
INCLUDES = -I $(INCLUDES_DIR) -I $(THIRD_PARTY_DIR)
HEADERS = $(wildcard $(INCLUDES_DIR)/*.hpp $(INCLUDES_DIR)/*.h $(INCLUDES_DIR)/**/*.hpp $(INCLUDES_DIR)/**/*.h)

CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -g3 -std=c++17 -fPIC

OBJS_DIR = objs
OBJS = $(patsubst $(SRCS_DIR)/%.cpp, $(OBJS_DIR)/%.o, $(SRCS))

# Tests
TESTS_DIR = tests
TEST_SRCS = $(wildcard $(TESTS_DIR)/*.cpp)
TEST_OBJS = $(patsubst $(TESTS_DIR)/%.cpp, $(OBJS_DIR)/tests/%.o, $(TEST_SRCS))
TEST_BIN = $(TESTS_DIR)/run
# Engine objects without main.o (tests provide their own main via doctest)
ENGINE_OBJS = $(filter-out $(OBJS_DIR)/main.o, $(OBJS))

# Color codes
GREEN = \033[0;32m
RED = \033[0;31m
BLUE = \033[0;34m
ORANGE = \033[0;33m
NC = \033[0m

all: $(NAME)

$(NAME): $(OBJS)
	@echo "$(GREEN)$(NAME)$(NC) compiling..."
	@$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS) $(INCLUDES)
	@echo "$(GREEN)$(NAME)$(NC) ready!"

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

clean:
	@rm -rf $(OBJS_DIR)
	@rm -f $(TEST_BIN)
	@echo "$(RED)$(NAME)$(NC) OBJS cleaned!"

fclean: clean
	@rm -f $(NAME)
	@echo "$(RED)$(NAME)$(NC) cleaned!"

fcount:
	wc -l $(SRCS) $(HEADERS)


re: fclean all

.PHONY: all clean fclean re tests
