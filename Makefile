SRC =  main.cpp Server.cpp connection/new_connection.cpp connection/client_request.cpp

INCLUDES = Server.hpp

NAME = reverse_proxy

OBJ = $(SRC:.cpp=.o)
 
CC = g++

all		: $(NAME)

$(NAME)	:  $(OBJ) 
	$(CC) $(FLAGS) $(OBJ) -o $(NAME)

%.o: %.cpp $(INCLUDES)
	$(CC) -c $< -o $@

clean	:
	rm -rf $(OBJ)

fclean	: clean
	rm -rf $(NAME)

re	: fclean all

.PHONY: all clean fclean re