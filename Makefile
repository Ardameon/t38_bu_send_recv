.PHONY: all

all: 
	gcc -Wall -O0 -g t38_transmitter.c msg_proc.c -o fax_transmitter 
