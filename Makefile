webserver: webserver.c
	gcc -o webserver webserver.c http_message.c

clean:
	rm webserver