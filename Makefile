all: complie_server compile_client

complie_server: Request.h Response.h server.cpp
	g++ -g -o server server.cpp -lpthread

compile_client: client.cpp Request.h Response.h
	g++ -g -o client client.cpp