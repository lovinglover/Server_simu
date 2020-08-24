#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>
#include "thread_pool.h"

using std::cout;
using std::endl;
using std::string;

#define SERVER_PORT 12345
#define MAX_OPEN 5000

extern int errno;

struct para{
    int fd;
    char *s_buf;
    int len;
};

inline void p_err(const string s){
    cout << s << " at " << __FILE__ << ":" << __LINE__ << endl;
    cout << strerror(errno) << endl;
    exit(-1);
}

int Accept(int fd, struct sockaddr *addr, socklen_t *addrlen){
    int ret;

again:
    if((ret = accept(fd, addr, addrlen)) < 0){
        if((errno == ECONNABORTED) || (errno == EINTR)) goto again;
        else p_err("accept error");
    }
    return ret;
}

int Bind(int fd, struct sockaddr *addr, socklen_t addrlen){
    int ret;
    if((ret = bind(fd, addr, addrlen)) < 0) p_err("bind error");

    return ret;
}

int Connect(int fd, struct sockaddr *addr, socklen_t addrlen){

    int ret;
    if((ret = connect(fd, addr, addrlen)) < 0) p_err("connect error");

    return ret;
}

int Listen(int fd, int backlog){
    int ret;

    if((ret = listen(fd, backlog)) < 0) p_err("listen error");
    return ret;
}

int Socket(int domain, int type, int protocol){
    int ret;

    if((ret == socket(domain, type, protocol)) < 0) p_err("socket error");
    return ret;
}

void *process(void *arg){
    para *pa = (para *)arg;
    char *buf = pa->s_buf;
    int res = pa->len;
    int socketfd = pa->fd;
    cout << "reveive msg:";
    write(STDOUT_FILENO, buf, res);
    for(int i = 0; i < res; i++) buf[i] = toupper(buf[i]);

    write(socketfd, buf, res);
}

int main()
{
    Thread_pool pool(3, MAX_NUMBER_OF_THREAD);
    int listenfd, connfd, socketfd;
    int conn_num = 0;
    size_t ready_num, efd;
    char buf[1024], str[INET_ADDRSTRLEN];
    socklen_t client_len;

    struct sockaddr_in client_addr, server_addr;
    struct epoll_event tep, ep[MAX_OPEN];

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    Bind(listenfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    Listen(listenfd, 20);

    efd = epoll_create(MAX_OPEN);
    if(-1 == efd) p_err("epoll_create error");

    tep.events = EPOLLIN;
    tep.data.fd = listenfd;
    if(-1 == epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &tep)) p_err("EPOLL_CTL_ADD error");

    while(true){
        if(-1 == (ready_num = epoll_wait(efd, ep, MAX_OPEN, -1))) p_err("epoll_wait error");

        for(int i = 0; i < ready_num; i++){
            if(!(ep[i].events & EPOLLIN)) continue;

            if(listenfd == ep[i].data.fd){
                client_len = sizeof(client_addr);
                connfd = Accept(listenfd, (struct sockaddr*)&client_addr, &client_len);
                cout << "new client join " << inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)) << ":" << ntohs(client_addr.sin_port) << endl;
                cout << "already have " << ++conn_num << " clients" << endl;

                tep.events = EPOLLIN;
                tep.data.fd = connfd;
                if(-1 == epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &tep)) p_err("EPOLL_CTL_ADD error");
            }
            else{
                socketfd = ep[i].data.fd;
                int res = read(socketfd, buf, 1024);

                if(0 == res){
                    if(-1 == epoll_ctl(efd, EPOLL_CTL_DEL, socketfd, NULL)) p_err("EPOLL_CTL_DEL error");
                    close(socketfd);
                    cout << "client " << socketfd << " closed connection" << endl;
                    cout << "have " << --conn_num << " clients" << endl;
                }
                else if(res < 0){
                    cout << "read < 0 error at " << __FILE__ ":" << __LINE__ << endl;
                    if(-1 == epoll_ctl(efd, EPOLL_CTL_DEL, socketfd, NULL)) p_err("EPOLL_CTL_DEL error");
                    close(socketfd);
                    cout << "client " << socketfd << " closed connection" << endl;
                    cout << "have " << --conn_num << " clients" << endl;
                }
                else{
                    para *pa = new para();
                    pa->fd = connfd;
                    pa->s_buf = buf;
                    pa->len = res;
                    pool.add(process, pa);
//                    cout << "reveive msg:";
//                    write(STDOUT_FILENO, buf, res);
//                    for(int i = 0; i < res; i++) buf[i] = toupper(buf[i]);

//                    write(socketfd, buf, res);
                }                
            }
        }
    }
    close(listenfd);
    close(efd);

    return 0;
}
