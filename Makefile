all:
	gcc main.c -o main -lpthread
	nasm -f bin code.asm -o code
clean:
	rm main code
