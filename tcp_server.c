#include "common.h"

int tcp_listen(const char *host, const char *serv, socklen_t *addrlenp)
{
    int             listenfd, n;
    const int       on = 1;
    struct addrinfo hints, *res, *ressave;

    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0)
        mylog_exit("tcp_listen error for %s, %s: %s",
                 host, serv, gai_strerror(n));
    ressave = res;

    do {
        listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (listenfd < 0)
            continue;

        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
            mylog_exit("tcp_listen: setsockopt(%d) error", listenfd);
        if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
            break;

        if (close(listenfd) == -1)
            mylog_exit("tcp_listen: close(%d) error", listenfd);
    } while ( (res = res->ai_next) != NULL);

    if (res == NULL)
        mylog_exit("tcp_listen error for %s, %s", host, serv);

    if (listen(listenfd, 1024))
        mylog_exit("tcp_listen: listen(%d) error", listenfd);

    if (addrlenp)
        *addrlenp = res->ai_addrlen;

    freeaddrinfo(ressave);

    return(listenfd);
}

void pattern(char *ptr, int len)
{
    char c; 
    c = 0;
    while(len-- > 0)  {  
        while(isprint((c & 0x7F)) == 0) 
            c++;
        *ptr++ = (c++ & 0x7F);
    }  
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t socklen, clilen;
    struct sockaddr_storage cliaddr;
    struct message_t msg;
    int n;
    char sndbuf[SNDBUFSIZE];

    if (argc == 2)
        listenfd = tcp_listen("localhost", argv[1], &socklen);
    else if (argc == 3)
        listenfd = tcp_listen(argv[1], argv[2], &socklen);
    else
        mylog_exit("usage: %s [<host>] <service or port>", argv[0]);

    pattern(sndbuf, SNDBUFSIZE);
    while (1) {
        clilen = sizeof(cliaddr);
        if ((connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen)) < 0) {
            if (errno == EINTR)
                continue;
            else
                mylog_note("accept: %s", strerror(errno));
        }

        while (1) {
            if (recvn(connfd, &msg, sizeof(msg)) != sizeof(msg)) {
                mylog_note("error/eof on client socket, continue...");
                close(connfd);
                break;
            }
            msgdeserialize(&msg);
            printf("recv msg: "); msgprint(&msg);
            //sleep(1);
            if (sendn(connfd, sndbuf, SNDBUFSIZE) != SNDBUFSIZE) {
                mylog_note("error sending msg, continue...");
                close(connfd);
                break;
            }
            printf("send response: %d bytes\n", SNDBUFSIZE);
        }
    }
}
