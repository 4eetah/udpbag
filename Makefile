SRCS := udp_server.c udp_client.c
HDRS := common.h
OBJS := $(SRCS:.c=.o)
CFLAGS :=
LDFLAGS := -pthread

all: udpcli udpsrv tcpsrv

udpsrv: udp_server.c hashtbl.c endian.c tcprw.c common.h
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

udpcli: udp_client.c endian.c common.h
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tcpsrv: tcp_server.c endian.c tcprw.c common.h
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) udpsrv udpcli tcpsrv *.o
