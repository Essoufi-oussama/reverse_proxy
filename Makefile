SRC =  main.cpp Server.cpp connection/new_connection.cpp connection/client_request.cpp parsing/parsing.cpp

INCLUDES = Server.hpp

NAME = reverse_proxy

OBJ = $(SRC:.cpp=.o)
 
CC = g++
FLAGS = -std=c++17

all		: $(NAME)

$(NAME)	:  $(OBJ) 
	$(CC) $(FLAGS) $(OBJ) -o $(NAME)

%.o: %.cpp $(INCLUDES)
	$(CC) $(FLAGS) -c $< -o $@

clean	:
	rm -rf $(OBJ)

fclean	: clean
	rm -rf $(NAME)

re	: fclean all

.PHONY: all clean fclean re