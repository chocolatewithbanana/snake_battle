FLAGS=-Wall -Wextra -pedantic

all: client server

client: client.c
	gcc -o client client.c $(FLAGS) -std=c11 -g

server: server.c
	gcc -o server server.c $(FLAGS) -std=c11 -g

clean: server client
	rm server client
