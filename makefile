client.out: client.c
	gcc -pthread -o client.out client.c

clean:
	rm -rf client.out