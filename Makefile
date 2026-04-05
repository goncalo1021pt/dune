NAME = dune

SRCS = $(wildcard $(SRCS_DIR)/*.cpp $(SRCS_DIR)/**/*.cpp)
SRCS_DIR = srcs

INCLUDES = -I $(INCLUDES_DIR)
INCLUDES_DIR = includes/headers
HEADERS = $(wildcard $(INCLUDES_DIR)/*.hpp $(INCLUDES_DIR)/*.h $(INCLUDES_DIR)/**/*.hpp $(INCLUDES_DIR)/**/*.h)

CXX = g++
CXXFLAGS = -Wall -Wextra -Werror

OBJS_DIR = objs
OBJS = $(patsubst $(SRCS_DIR)/%.cpp, $(OBJS_DIR)/%.o, $(SRCS))

# Color codes
GREEN = \033[0;32m
RED = \033[0;31m
BLUE = \033[0;34m 
ORANGE = \033[0;33m
NC = \033[0m 

all: $(NAME)

$(NAME): $(OBJS)
	@echo "$(GREEN)$(NAME)$(NC)compiling..."
	@$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS) $(INCLUDES)
	@echo "$(GREEN)$(NAME)$(NC)ready!"

$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean: 
	@rm -rf $(OBJS_DIR)
	@echo "$(RED)$(NAME)$(NC)OBJS cleaned!"

fclean: clean
	@rm -f $(NAME)
	@echo "$(RED)$(NAME)$(NC)cleaned!"

re: fclean all

.PHONY: all clean fclean re