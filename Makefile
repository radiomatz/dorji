all: dorji

dorji: dorji.c
	gcc -g -o dorji dorji.c
