#include "common.h"

static int udp_server(const char *host, const char *serv, socklen_t *addrlenp)
{
	int				sockfd, n;
	struct addrinfo	hints, *res, *ressave;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0)
		mylog_exit("udp_server error for %s, %s: %s",
				 host, serv, gai_strerror(n));
	ressave = res;

	do {
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0)
			continue;

		if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0)
			break;

		close(sockfd);
	} while ( (res = res->ai_next) != NULL);

	if (res == NULL)
		mylog_exit("udp_server error for %s, %s", host, serv);

	if (addrlenp)
		*addrlenp = res->ai_addrlen;

	freeaddrinfo(ressave);

	return(sockfd);
}

struct srvinfo {
    int sockfd;
    socklen_t addrlen;
};
pthread_t udpthread1, udpthread2, tcpthread;
pthread_cond_t data10cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t data10mux = PTHREAD_MUTEX_INITIALIZER;
int data10flag;
struct message_t data10msg;
struct hash_table udpbag;

static void *tcpsrc_start(void *arg)
{
    while (1) {
        pthread_mutex_lock(&data10mux);
        while (data10flag == 0)
            pthread_cond_wait(&data10cond, &data10mux);
        data10flag = 0;
        mylog_note("(thread tcpsrc) msg received, id: %llu, data: %llu",
                (unsigned long long)data10msg.id, (unsigned long long)data10msg.data);
        pthread_mutex_unlock(&data10mux);
    }
}

static void *server_start(void *arg)
{
    int sockfd, n, msglen;
    struct sockaddr *cliaddr;
    struct message_t rcvmsg;
    socklen_t addrlen, len;
    struct srvinfo *srv = (struct srvinfo*)arg;

    sockfd = srv->sockfd;
    addrlen = srv->addrlen;
    msglen = sizeof(rcvmsg);

    cliaddr=malloc(addrlen);
    while(1) {
        len = addrlen;
        if ((n = recvfrom(sockfd, &rcvmsg, msglen, 0, cliaddr, &len)) != msglen) {
            mylog_note("(thread %d) recvfrom %d bytes, expected %d: ", udpthread1==pthread_self()?1:2, n, msglen, strerror(errno));
            continue;
        }
        mylog_note("(thread %d) recvfrom %d bytes, id: %llu, data: %llu",
                udpthread1==pthread_self()?1:2, n, (unsigned long long)rcvmsg.id, (unsigned long long)rcvmsg.data);
        hashtbl_put(&udpbag, &rcvmsg);
        if (rcvmsg.data==10){
            pthread_mutex_lock(&data10mux);
            mylog_note("(thread %d) obtains data10 lock", udpthread1==pthread_self()?1:2);
            data10msg = rcvmsg;
            data10flag = 1;
            pthread_cond_signal(&data10cond);
            pthread_mutex_unlock(&data10mux);
            mylog_note("(thread %d) release data10 lock", udpthread1==pthread_self()?1:2);
        }
    }
}

static void printbag_int(int signo)
{
    hashtbl_print(&udpbag);
    exit(0);
}

int main(int argc, char **argv)
{
    struct srvinfo info;
    if (argc == 2)
        info.sockfd = udp_server(NULL, argv[1], &info.addrlen);
    else if (argc == 3)
        info.sockfd = udp_server(argv[1], argv[2], &info.addrlen);
    else
        mylog_exit("usage: %s [<host>] <service or port>", argv[0]);

    hashtbl_init(&udpbag, BAGSIZE);
    signal(SIGINT, printbag_int);
    
    pthread_create(&udpthread1, NULL, server_start, (void*)&info);
    pthread_create(&udpthread2, NULL, server_start, (void*)&info);
    pthread_create(&tcpthread, NULL, tcpsrc_start, (void*)&info);

    pthread_join(udpthread1, NULL);
    pthread_join(udpthread2, NULL);
    pthread_join(tcpthread, NULL);
}
