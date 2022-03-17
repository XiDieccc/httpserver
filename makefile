all: server test httpd

server: socketServer.c
	gcc socketServer.c -o server

test: test.c
	gcc test.c -o test

httpd: httpdserver.c
	gcc httpdserver.c -lpthread -o httpd

clean: socketServer.c test.c httpdserver.c
	rm -fr server test httpd