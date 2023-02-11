NAME=main

$(NAME): $(NAME).c
	gcc -o $(NAME) $(NAME).c -lSDL2 -lSDL2_image -lSDL2_ttf -g -Wall -Wextra -std=c11

clean: $(NAME)
	rm $(NAME)
