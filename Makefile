NAME=main
EXEC=snake_game

$(EXEC): $(NAME).c
	gcc -o $(EXEC) $(NAME).c -lSDL2 -lSDL2_image -lSDL2_ttf -g -Wall -Wextra -pedantic -std=c11

clean: $(EXEC)
	rm $(EXEC)
