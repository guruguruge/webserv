NAME = webserv
CXX = c++
FLAGS = -Wall -Werror -Wextra -std=c++98 -pedantic
INCLUDES = -I inc
RM = rm -f
SRCDIR = src
SRC = \
	$(SRCDIR)/Config.cpp \
	$(SRCDIR)/HttpRequest.cpp \
	$(SRCDIR)/HttpResponse.cpp \
	main.cpp

OBJDIR = obj
OBJ = $(addprefix $(OBJDIR)/, $(SRC:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJ)
		$(CXX) $(FLAGS) $(OBJ) -o $(NAME)

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(FLAGS) $(INCLUDES) -c $< -o $@

clean:
		$(RM) -r $(OBJDIR)

fclean: clean
		$(RM) $(NAME)

re: fclean all

fmt:
	@echo "Formatting code..."
	@find . -type f \( -name "*.cpp" -o -name "*.hpp" \) -not -path "./.*" -exec clang-format -i {} +
	@echo "Done."

.PHONY: all clean fclean re fmt
