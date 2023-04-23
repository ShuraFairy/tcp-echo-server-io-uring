#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

//#pragma once

#include <iostream>

#include <liburing.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAX_CONNECTIONS 4096
#define BACKLOG 512
#define MAX_MESSAGE_LEN 2048
#define IORING_FEAT_FAST_POLL (1U << 5)

// /**
//  * Каждое активное соединение в нашем приложение описывается структурой conn_info.
//  * fd - файловый дескриптор сокета.
//  * type - описывает состояние в котором находится сокет - ждет accept, read или write.
//  */
// typedef struct conn_info {
// 	int fd;
// 	unsigned type;
// } conn_info;
//
// enum {
// 	ACCEPT,
// 	READ,
// 	WRITE,
// };
//
// // // Буфер для соединений.
// conn_info conns[MAX_CONNECTIONS];
//
// // Для каждого возможного соединения инициализируем буфер для чтения/записи.
// char bufs[MAX_CONNECTIONS][MAX_MESSAGE_LEN];

class EchoServer final
{
public:
	// // Буфер для соединений.
	// conn_info conns[MAX_CONNECTIONS];
 //
	// // Для каждого возможного соединения инициализируем буфер для чтения/записи.
	// char bufs[MAX_CONNECTIONS][MAX_MESSAGE_LEN];

	EchoServer(int port);
	~EchoServer();
	void initEchoServer();
	void start();
private:
	void add_accept(struct io_uring* ring, int fd, struct sockaddr* client_addr, socklen_t* client_len);
	void add_socket_read(struct io_uring* ring, int fd, size_t size);
	void add_socket_write(struct io_uring* ring, int fd, size_t size);
	int sock_listen_fd;
	int portno_;
	struct io_uring ring {};
	struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

};

#endif
