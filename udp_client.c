#include "common.h"

static int udp_client(const char *host, const char *serv, struct sockaddr **saptr, socklen_t *lenp)
{
	int				sockfd, n;
	struct addrinfo	hints, *res, *ressave;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0)
		mylog_exit("udp_client error for %s, %s: %s",
				 host, serv, gai_strerror(n));
	ressave = res;

	do {
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd >= 0)
			break;
	} while ( (res = res->ai_next) != NULL);

	if (res == NULL)
		mylog_exit("udp_client error for %s, %s", host, serv);

	*saptr = malloc(res->ai_addrlen);
    if (!*saptr)
        myperror("udp_client malloc error");
	memcpy(*saptr, res->ai_addr, res->ai_addrlen);
	*lenp = res->ai_addrlen;

	freeaddrinfo(ressave);

	return(sockfd);
}

int main(int argc, char **argv)
{
    struct sockaddr *srvaddr;
    socklen_t addrlen;
    int sockfd, n, i;
    struct message_t sndmsg;
    int nrmsg, writelen;
    uint64_t msgdata=0;
    char buf[sizeof(struct message_t)];

    if (argc < 4) {
        mylog_exit("usage: %s <host> <service or port> <# of messages> [msgdata]", argv[0]);
    }
    if (argc==5)
        msgdata=atoi(argv[4]);
    nrmsg = atoi(argv[3]);
    srand(time(NULL));
    sockfd = udp_client(argv[1], argv[2], &srvaddr, &addrlen);
    setendian();
    
    writelen = sizeof(sndmsg);
    sndmsg.size = writelen;
    sndmsg.type = TWO;
    
    for(i=0; i < nrmsg; i++) {
        if (msgdata)
            sndmsg.data = msgdata;
        else
            sndmsg.data = rand() % 4 + 8;
        sndmsg.id = rand() % nrmsg;

        msgserialize(&sndmsg);
        if ((n = sendto(sockfd, &sndmsg, writelen, 0, srvaddr, addrlen)) != writelen) {
            mylog_note("dgram #%d: write returned %d bytes, expexted %d", i, n, writelen);
        } else {
            msgdeserialize(&sndmsg);
            printf("send msg| "); msgprint(&sndmsg);
        }
    }
}
