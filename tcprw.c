#include "common.h"

ssize_t recvn(int sockfd, void *ptr, size_t n)
{
    int nrecv = 0;
    int nleft = n;

    while (nleft > 0) {
        nrecv = recv(sockfd, ptr, nleft, 0);
        if (nrecv < 0) {
            if (errno == EINTR)
                continue;
            else
                return -1;
        } else if (nrecv == 0)
            break;

        nleft -= nrecv;
        ptr += nrecv;
    }
    return n-nleft;
}
ssize_t sendn(int sockfd, void *ptr, size_t n)
{
    int nsend = 0;
    int nleft = n;

    while (nleft > 0) {
        nsend = send(sockfd, ptr, nleft, 0);
        if (nsend < 0) {
            if(errno == EINTR)
                continue;
            else
                return -1;
        }
        nleft -= nsend;
        ptr += nsend;
    }
    return n;
}

