#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

struct SessionMessage
{
    int32_t number;
    int32_t length;
} __attribute__ ((__packed__));

struct PayloadMessage
{
    int32_t length;
    char data[0];
};

static void printf_address(int fd, struct sockaddr *in_addr, socklen_t in_len, const char *msg = "")
{
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    if (getnameinfo(in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf, NI_NUMERICHOST | NI_NUMERICSERV) == 0)
    {
        fprintf(stdout, "[%8d]%s:  (host=%s, port=%s)\n", fd, msg, hbuf, sbuf);
    }
}

static int create_and_bind(const char *port, const char *address=NULL)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
    int sfd;

    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* All interfaces */

    s = getaddrinfo(address, port, &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;
        int enable = 1;
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&enable, sizeof(int)) < 0)
        {
            fprintf(stderr, "error setsockopt SO_REUSEADDR\n");
        }

        s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0)
        {
            /* We managed to bind successfully! */
            printf_address(sfd, rp->ai_addr, rp->ai_addrlen, "Listen on");
            break;
        }
        close(sfd);
    }

    if (rp == NULL)
    {
        fprintf(stderr, "Could not bind\n");
        sfd = -1;
    }

    freeaddrinfo(result);

    return sfd;
}

static int create_and_connect(const char *address, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
    int sfd;

    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* All interfaces */

    s = getaddrinfo(address, port, &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        s = connect(sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0)
        {
            /* We managed to bind successfully! */
            printf_address(sfd, rp->ai_addr, rp->ai_addrlen, "Connect to");
            int flags = 1;
            if (setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flags, sizeof(int)) < 0)
            {
                fprintf(stderr, "setsockopt TCP_NODELAY error, errno %d\n", errno);
            }
            break;
        }
        close(sfd);
    }

    if (rp == NULL)
    {
        fprintf(stderr, "Could not bind\n");
        sfd = -1;
    }

    freeaddrinfo(result);

    return sfd;
}

int send_all(int sfd, const uint8_t* buff, uint32_t len)
{
    uint32_t s = 0;
    while(s < len)
    {
        int n = send(sfd, buff + s, len - s, 0);
        if(n > 0) {
            s += n;
            continue;
        } else if(n == -1) {
            if(errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("send all");
                return -1;
            }
        }
    }
    return 0;
}

int recv_all(int sfd, uint8_t* buff, uint32_t len)
{
    uint32_t r = 0;
    while(r < len)
    {
        int n = recv(sfd, buff + r, len - r, 0);
        if(n > 0) {
            r += n;
            continue;
        } else if(n == 0) {
            fprintf(stderr, "peer close the connection.\n");
            return -1;
        } else if(n == -1) {
            if(errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("recv all");
                return -2;
            }
        }
    }
    return 0;
}

double now()
{
    struct timeval tv = { 0, 0 };
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// server
void Receive(const char* port)
{
    int listenfd = create_and_bind(port);
    assert(listen(listenfd, 1000) == 0);
    while(true)
    {
        struct sockaddr_in si;
        socklen_t l = sizeof si;
        int connfd = accept(listenfd, (struct sockaddr*)&si, &l);
        assert(connfd != -1);
        printf_address(connfd, (struct sockaddr*)&si, l, "Accept");

        SessionMessage sm = {0 , 0};
        if(0 != recv_all(connfd, (uint8_t *)&sm, sizeof sm))
        {
            close(connfd);
            continue;
        }

        sm.length = ntohl(sm.length);
        sm.number = ntohl(sm.number);
        printf("length = %d, number = %d\n", sm.length, sm.number);
        double total_mb = 1.0 * sm.length * sm.number  / 1024 / 1024;
        printf("Total: %f MiB.\n", total_mb);

        PayloadMessage *pm;
        int32_t total = sizeof(int32_t) + sm.length;
        pm = (PayloadMessage*)malloc(total);

        double start = now();
        while(sm.number > 0)
        {
            if(recv_all(connfd, (uint8_t*)&pm->length, sizeof pm->length) != 0)
                break;

            pm->length = ntohl(pm->length);
            if(pm->length != sm.length) break;
            if(recv_all(connfd, (uint8_t*)pm->data, pm->length) != 0)
                break;

            int ack = htonl(pm->length);
            if(send_all(connfd, (uint8_t*)&ack, sizeof ack) != 0)
                break;

            sm.number --;
        }
        double total_time = now() - start;
        printf("Speed: %f MiB/s\nTotal time=%f seconds\n", total_mb / total_time, total_time);

        free(pm);
        close(connfd);
    }
}

// client
void Transmit(const char* host, const char* port, const int32_t length, const int32_t number)
{
    int connfd = create_and_connect(host, port);
    assert(connfd != -1);

    double total_mb = 1.0 * length * number  / 1024 / 1024;
    printf("Total: %f MiB.\n", total_mb);

    SessionMessage sm;
    sm.length = htonl(length);
    sm.number = htonl(number);

    if(0 != send_all(connfd,(uint8_t*)&sm, sizeof sm))
    {
        close(connfd);
        return;
    }

    PayloadMessage *pm;
    int32_t total = sizeof(int32_t) + length;
    pm = (PayloadMessage*)malloc(total);
    pm->length = htonl(length);
    memset(pm->data, 0x0, length);
    for(int i = 0; i < length; i++) {
        pm->data[i] = "1234567890"[i%10];
    }

    double start = now();
    int n = number;
    while(n > 0)
    {
        if(0 != send_all(connfd, (uint8_t*)pm, total))
        {
            break;
        }
        int ack = 0;
        assert(recv_all(connfd, (uint8_t*)&ack, sizeof ack) == 0);
        ack = ntohl(ack);
        assert(ack == length);
        n--;
    }
    double total_time = now() - start;
    printf("Speed: %f MiB/s\nTotal time=%f seconds\n", total_mb / total_time, total_time);

    free(pm);
    close(connfd);
}

int main(int argc, char* argv[])
{
    if(argc == 1) {
        printf("Usage: %s -r or -t\n", argv[0]);
        return 0;
    }

    bool receive = true;
    int32_t number = 65535, length = 4096;
    char* host, *port = const_cast<char*>("5001");

    int opt;
    while ((opt = getopt(argc, argv, "p:rt:n:l:")) != -1) {
        switch (opt) {
            case 'p':
                port = optarg;
                break;
            case 'r':
                receive = true;
                break;
            case 't':
                receive = false;
                host = optarg;
                break;
            case 'n':
                number = atoi(optarg);
                break;
            case 'l':
                length = atoi(optarg);
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s -r or -t [-l length] [-n number]\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if(receive) Receive(port);
    else Transmit(host, port, length, number);
    return 0;
}
