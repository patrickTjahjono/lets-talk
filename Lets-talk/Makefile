all: lets-talk.c
	gcc -g -Wall -o lets-talk lets-talk.c list.c -pthread -lreadline

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./lets-talk 6000 localhost 6001

cleans:
	$(RM) lets-talk
