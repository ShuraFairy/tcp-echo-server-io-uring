#include <cstdlib>
#include <cstring>
#include <cassert>

// Linux
#include <error.h>
//#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#include "echo-server.h"

/**
 * Каждое активное соединение в нашем приложение описывается структурой conn_info.
 * fd - файловый дескриптор сокета.
 * type - описывает состояние в котором находится сокет - ждет accept, read или write.
 */
typedef struct conn_info {
	int fd;
	unsigned type;
} conn_info;

enum {
	ACCEPT,
	READ,
	WRITE,
};

// // Буфер для соединений.
conn_info conns[MAX_CONNECTIONS];

// Для каждого возможного соединения инициализируем буфер для чтения/записи.
char bufs[MAX_CONNECTIONS][MAX_MESSAGE_LEN];


EchoServer::EchoServer(int port) : sock_listen_fd{ -1 }, portno_{ port }
{
}

EchoServer::~EchoServer()
{
	io_uring_queue_exit(&ring);
}

void EchoServer::initEchoServer()
{
    // struct sockaddr_in serv_addr;
    // struct sockaddr_in client_addr;
    // socklen_t client_len = sizeof(client_addr);

    sock_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    const int val = 1;
    setsockopt(sock_listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno_);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    assert(bind(sock_listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) >= 0);
    assert(listen(sock_listen_fd, BACKLOG) >= 0);

    /**
     * Создаем инстанс io_uring, не используем никаких кастомных опций.
     * Емкость очередей SQ и CQ указываем как 4096 вхождений.
     */
    struct io_uring_params params;
    //struct io_uring ring;
    memset(&params, 0, sizeof(params));

    assert(io_uring_queue_init_params(4096, &ring, &params) >= 0);

    /**
     * Проверяем наличие фичи IORING_FEAT_FAST_POLL.
     * Для нас это наиболее "перформящая" фича в данном приложении,
     * фактически это встроенный в io_uring движок для поллинга I/O.
     */
    if (!(params.features & IORING_FEAT_FAST_POLL)) {
        printf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...\n");
        exit(0);
    }

    /**
     * Добавляем в SQ первую операцию - слушаем сокет сервера для приема входящих соединений.
     */
    add_accept(&ring, sock_listen_fd, (struct sockaddr*)&client_addr, &client_len);
}

void EchoServer::start()
{
    /*
     * event loop
     */
	while (true) {
		struct io_uring_cqe* cqe;
		int ret;

		/**
		 * Сабмитим все SQE которые были добавлены на предыдущей итерации.
		 */
		io_uring_submit(&ring);

		/**
		 * Ждем когда в CQ буфере появится хотя бы одно CQE.
		 */
        ret = io_uring_wait_cqe(&ring, &cqe);
        assert(ret == 0);

        /**
         * Положим все "готовые" CQE в буфер cqes.
         */
        struct io_uring_cqe* cqes[BACKLOG];
        int cqe_count = io_uring_peek_batch_cqe(&ring, cqes, sizeof(cqes) / sizeof(cqes[0]));

        for (int i = 0; i < cqe_count; ++i) {
            cqe = cqes[i];

            /**
             * В поле user_data мы заранее положили указатель структуру
             * в которой находится служебная информация по сокету.
             */
            struct conn_info* user_data = (struct conn_info*)io_uring_cqe_get_data(cqe);

            /**
             * Используя тип идентифицируем операцию к которой относится CQE (accept/recv/send).
             */
            unsigned type = user_data->type;
            if (type == ACCEPT) {
                int sock_conn_fd = cqe->res;

                /**
                * Если появилось новое соединение: добавляем в SQ операцию recv - читаем из клиентского сокета,
                * продолжаем слушать серверный сокет.
                */
                add_socket_read(&ring, sock_conn_fd, MAX_MESSAGE_LEN);
                add_accept(&ring, sock_listen_fd, (struct sockaddr*)&client_addr, &client_len);
            }
            else if (type == READ) {
                int bytes_read = cqe->res;

                /**
                 * В случае чтения из клиентского сокета:
                 * если прочитали 0 байт - закрываем сокет
                 * если чтение успешно: добавляем в SQ операцию send - пересылаем прочитанные данные обратно, на клиент.
                 */
                if (bytes_read <= 0) {
                    shutdown(user_data->fd, SHUT_RDWR);
                }
                else {
                    add_socket_write(&ring, user_data->fd, bytes_read);
                }
            }
            else if (type == WRITE) {
                /**
                * Запись в клиентский сокет окончена: добавляем в SQ операцию recv - читаем из клиентского сокета.
                */
                add_socket_read(&ring, user_data->fd, MAX_MESSAGE_LEN);
            }

            io_uring_cqe_seen(&ring, cqe);
        }
    }
}

void EchoServer::add_accept(io_uring* ring, int fd, sockaddr* client_addr, socklen_t* client_len)
{
    // Получаем указатель на первый доступный SQE.
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    // Хелпер io_uring_prep_accept помещает в SQE операцию ACCEPT.
    io_uring_prep_accept(sqe, fd, client_addr, client_len, 0);

    // Устанавливаем состояние серверного сокета в ACCEPT.
    conn_info* conn_i = &conns[fd];
    conn_i->fd = fd;
    conn_i->type = ACCEPT;

    // Устанавливаем в поле user_data указатель на socketInfo соответствующий серверному сокету.
    io_uring_sqe_set_data(sqe, conn_i);
}

void EchoServer::add_socket_read(io_uring* ring, int fd, size_t size)
{
    // Получаем указатель на первый доступный SQE.
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    // Хелпер io_uring_prep_recv помещает в SQE операцию RECV, чтение производится в буфер соответствующий клиентскому сокету.
    io_uring_prep_recv(sqe, fd, &bufs[fd], size, 0);

    // Устанавливаем состояние клиентского сокета в READ.
    conn_info* conn_i = &conns[fd];
    conn_i->fd = fd;
    conn_i->type = READ;

    // Устанавливаем в поле user_data указатель на socketInfo соответствующий клиентскому сокету.
    io_uring_sqe_set_data(sqe, conn_i);
}

void EchoServer::add_socket_write(io_uring* ring, int fd, size_t size)
{
    // Получаем указатель на первый доступный SQE.
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    // Хелпер io_uring_prep_send помещает в SQE операцию SEND, запись производится из буфера соответствующего клиентскому сокету.
    io_uring_prep_send(sqe, fd, &bufs[fd], size, 0);

    // Устанавливаем состояние клиентского сокета в WRITE.
    conn_info* conn_i = &conns[fd];
    conn_i->fd = fd;
    conn_i->type = WRITE;

    // Устанавливаем в поле user_data указатель на socketInfo соответсвующий клиентскому сокету.
    io_uring_sqe_set_data(sqe, conn_i);
}
