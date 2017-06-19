#include "common.h"

static int tcp_connect(const char *host, const char *serv)
{
    int             sockfd, n;
    struct addrinfo hints, *res, *ressave;

    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0)
        mylog_exit("tcp_connect error for %s, %s: %s",
                 host, serv, gai_strerror(n));
    ressave = res;

    do {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0)
            continue;

        if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
            break;

        close(sockfd);
    } while ( (res = res->ai_next) != NULL);

    if (res == NULL)
        mylog_exit("tcp_connect error for %s, %s", host, serv);

    freeaddrinfo(ressave);

    return(sockfd);
}

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
struct msgqueue {
    struct message_t *queue;
    int size;
    int basesize;
    int idx;
    pthread_cond_t cond;
    pthread_mutex_t mux;
};
pthread_t udpthread1, udpthread2, tcpthread;
struct hash_table udpbag;
struct msgqueue udpqueue;
char *udphost, *tcphost, *udpservice, *tcpservice;

void msgqueue_init(struct msgqueue *q, int basesize)
{
    q->queue = malloc(sizeof(struct message_t)*basesize);
    q->size = basesize;
    q->basesize = basesize;
    q->idx = 0;
}
int msgqueue_empty(struct msgqueue *q)
{
    return q->idx == 0;
}
int msgqueue_push(struct msgqueue *q, struct message_t *msg)
{
    if(q->idx == q->size){
        void *newq;
        newq = realloc(q->queue, q->size*2*sizeof(*msg));
        if (newq) {
            q->queue = newq;
            q->size = q->size*2;
        } else {
            perror("realloc oom");
            mylog_note("discarding msg");
        }
        return 0;
    }
    memcpy(&q->queue[q->idx], msg, sizeof(*msg));
    q->idx++;
    return 1;
}
int msgqueue_pop(struct msgqueue *q, struct message_t *out)
{
    if(msgqueue_empty(q))
        return 0;
    q->idx--;
    if (q->idx > q->basesize && q->idx == q->size/4){
        q->queue = realloc(q->queue, q->size/2*sizeof(*out));
        q->size = q->size/2;
    }
    memcpy(out, &q->queue[q->idx], sizeof(*out));
    return 1;
}
int msgqueue_peek(struct msgqueue *q, struct message_t *out)
{
    if(msgqueue_empty(q))
        return 0;
    memcpy(out, &q->queue[q->idx-1], sizeof(*out));
    return 1;
}

int setnonblock(int fd)
{
    int val;

    if ((val = fcntl(fd, F_GETFL, 0)) == -1)
        return -1;

    if (fcntl(fd, F_SETFL, val | O_NONBLOCK) == -1)
        return -1;
    return 0;
}

static void *tcpsrc_start(void *arg)
{
    struct message_t msg;
    int sockfd;
    char buf[SNDBUFSIZE];
    int n;

    sockfd = tcp_connect(tcphost, tcpservice); 
    //setnonblock(sockfd); 

    while (1) {
        pthread_mutex_lock(&udpqueue.mux);
        while (msgqueue_empty(&udpqueue))
            pthread_cond_wait(&udpqueue.cond, &udpqueue.mux);
        if (!msgqueue_pop(&udpqueue, &msg)){
            mylog_note("this shouldn't happen");
            continue;
        }
        pthread_mutex_unlock(&udpqueue.mux);

        printf("(thread tcp) msg: "); msgprint(&msg); 

        msgserialize(&msg);
        if ((n = send(sockfd, &msg, sizeof(msg), 0)) != sizeof(msg)) {
            mylog_note("(thread tcp) send error: %s", strerror(errno));
            continue;
        }
        mylog_note("(thread tcp) snd msg %d bytes", n);

        if ((n = recvn (sockfd, buf, SNDBUFSIZE, 0)) != SNDBUFSIZE) {
            mylog_note("(thread tcp) recv error: %s", strerror(errno));
            continue;
        }
        mylog_note("(thread tcp) rcv response %d bytes", n);
    }
    close(sockfd);
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
        msgdeserialize(&rcvmsg);
        printf("(thread %d) recvfrom: ", udpthread1 == pthread_self() ? 1:2); msgprint(&rcvmsg);
        hashtbl_put(&udpbag, &rcvmsg);
        if (rcvmsg.data==10){
            pthread_mutex_lock(&udpqueue.mux);
            msgqueue_push(&udpqueue, &rcvmsg);
            pthread_cond_signal(&udpqueue.cond);
            pthread_mutex_unlock(&udpqueue.mux);
        }
    }
}

static void printbag_int(int signo)
{
    hashtbl_print(&udpbag);
    exit(0);
}
static void usage(char *prog)
{
    mylog_exit("usage: %s [-u <udphost>] [-t <tcphost>] <udp service or port> <tcp service or port>", prog);
}

int main(int argc, char **argv)
{
    struct srvinfo info;
    int opt;

    udphost=strdup("localhost");
    tcphost=strdup("localhost");

    while ((opt = getopt(argc, argv, "u:t:h")) != -1){
        switch (opt) {
        case 'u':
            free(udphost);
            udphost = strdup(optarg);
            break;
        case 't':
            free(udphost);
            tcphost = strdup(optarg);
            break;
        case '?':
        default:
            usage(argv[0]);
        }
    }
    if (argc-optind != 2)
        usage(argv[0]);

    udpservice = strdup(argv[optind++]);
    tcpservice = strdup(argv[optind]);
    mylog_note("udpsrv: %s:%s, tcpsrv: %s:%s", udphost,udpservice,tcphost,tcpservice);

    info.sockfd = udp_server(udphost, udpservice, &info.addrlen);
    hashtbl_init(&udpbag, BAGSIZE);
    msgqueue_init(&udpqueue, QUEUEBASE);
    signal(SIGINT, printbag_int);
    setendian();
    
    pthread_create(&udpthread1, NULL, server_start, (void*)&info);
    pthread_create(&udpthread2, NULL, server_start, (void*)&info);
    pthread_create(&tcpthread, NULL, tcpsrc_start, (void*)&info);

    pthread_join(udpthread1, NULL);
    pthread_join(udpthread2, NULL);
    pthread_join(tcpthread, NULL);
}
