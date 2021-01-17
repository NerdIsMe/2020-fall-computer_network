edit: client.out server.out

client.out: client.c
	gcc -pthread -o client.out client.c
server.out: my_server.c  threadpool.c 
	gcc my_server.c threadpool.c -lpthread -o server.out
clean:
	rm -rf client.out
	rm -rf server.out