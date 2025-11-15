# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: webserv                                    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/11/15                              #+#    #+#              #
#    Updated: 2025/11/15                             ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME		= webserv

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98
RM			= rm -f

# ディレクトリ
SRC_DIR		= src
INC_DIR		= include
OBJ_DIR		= obj

# ソースファイル
SRCS		= $(SRC_DIR)/main.cpp \
			  $(SRC_DIR)/Config.cpp \
			  $(SRC_DIR)/ConfigParser.cpp \
			  $(SRC_DIR)/Listener.cpp \
			  $(SRC_DIR)/Server.cpp \
			  $(SRC_DIR)/Poller.cpp \
			  $(SRC_DIR)/ClientConnection.cpp \
			  $(SRC_DIR)/HttpRequest.cpp \
			  $(SRC_DIR)/HttpRequestParser.cpp

# オブジェクトファイル
OBJS		= $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# インクルードフラグ
INCLUDES	= -I$(INC_DIR)

# ルール
all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "✅ $(NAME) built successfully!"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	$(RM) -r $(OBJ_DIR)
	@echo "🧹 Object files cleaned"

fclean: clean
	$(RM) $(NAME)
	@echo "🧹 $(NAME) cleaned"

re: fclean all

.PHONY: all clean fclean re
