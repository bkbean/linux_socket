/* server_epoll.c : socket programming by epoll in Linux */
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define MAX_DATA_BUF_SIZE   256
#define MAX_EVENTS_NUM      32
#define PROTOCOL_TCP        0x1
#define PROTOCOL_UDP        0x2
#define PROTOCOL_ALL        (PROTOCOL_TCP | PROTOCOL_UDP)
#define PINFO(fmt, arg...)  fprintf(stdout,"+ "fmt,##arg);fflush(stdout)
#define ERROR(fmt, arg...)  fprintf(stderr,"ERROR: "fmt" ",##arg);perror("");fflush(stderr)
#define WARN(fmt, arg...)   fprintf(stderr,"WARNING: "fmt,##arg);fflush(stderr)
typedef struct msg_tag {
    uint32_t    type;
    char        data[32];
} MSG;
int exit_flag = 0;
int message_process(int client, MSG *message, const struct sockaddr *to, socklen_t tolen);
int setnonblocking(int sock);
void *server_controler(void *arg);
void print_host_info();

/* Usage: server [-l listen-port] [-p protocol] */
int main(int argc, char *argv[])
{
    int         opt,i,epollfd,nfds,listen_sock,udp_sock,conn_sock;
    struct epoll_event ev,events[MAX_EVENTS_NUM];
    int16_t     port = 7000;
    int32_t     protocol = PROTOCOL_ALL;
    struct sockaddr_in serveraddr,clientaddr;
    socklen_t   addrlen;
    char        buf[MAX_DATA_BUF_SIZE];
    ssize_t     n;
    pthread_t   server_controler_tid;

    while((opt = getopt(argc, argv, ":l:p:f:")) != -1)
    {
        switch(opt)
        {
            case 'l':
                port = atoi(optarg);
                break;
            case 'p':
                if(strcasecmp("tcp", optarg)==0)
                    protocol = PROTOCOL_TCP;
                else if(strcasecmp("udp", optarg)==0)
                    protocol = PROTOCOL_UDP;
                else
                    protocol = PROTOCOL_ALL;
                break;
            case 'f':   /* config file */
                break;
            case ':':
                ERROR("option needs a value\n");
                break;
            case '?':
                ERROR("unknown option: %c\n", optopt);
                break;
        }
    }

    /* Open an epoll file descriptor. */
    epollfd = epoll_create(MAX_EVENTS_NUM);
    if (epollfd == -1)
    {
        ERROR("epoll_create()");
        return 2;
    }

    /* TCP Socket */
    if(protocol & PROTOCOL_TCP)
    {
        listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        setnonblocking(listen_sock);
        n = 1;
        if (0 != setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,&n, sizeof(n)))
        {
            WARN("failed to set listen socket option to SO_REUSEADDR.\n");
        }
        memset(&serveraddr, 0x0, sizeof(serveraddr));
        serveraddr.sin_family      = AF_INET;
        serveraddr.sin_port        = htons(port);
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(-1 == bind(listen_sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)))
        {ERROR("bind()"); return 1;}
        listen(listen_sock, 5);

        /*
         * Add the target file descriptor listen_sock to the epoll descriptor epollfd
         * and associate the event ev with the internal file linked to listen_sock.
         */
        ev.data.fd = listen_sock;
        ev.events= EPOLLIN|EPOLLET;
        if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev))
        {ERROR("epoll_ctl(listen_sock)"); return 3;}
    }

    /* UDP Socket */
    if((protocol & PROTOCOL_UDP))
    {
        udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
        setnonblocking(udp_sock);
        n = 1;
        if (0 != setsockopt(udp_sock,SOL_SOCKET,SO_REUSEADDR,&n, sizeof(n)))
        {
            WARN("failed to set udp socket option to SO_REUSEADDR.\n");
        }
        memset(&serveraddr, 0x0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(port);
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

        if(-1 == bind(udp_sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)))
        {ERROR("bind()"); return 1;}
        ev.data.fd = udp_sock;
        ev.events= EPOLLIN|EPOLLET;
        if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, udp_sock, &ev))
        {ERROR("epoll_ctl(udp_sock)"); return 3;}
    }

    PINFO("Server is initialized!\n");
    print_host_info();
    PINFO("Server Addr: %s:%d\n", inet_ntoa(serveraddr.sin_addr), port);
    PINFO("Server Protocol: %s%s\n", (protocol&PROTOCOL_TCP)?"TCP ":"", (protocol&PROTOCOL_UDP)?"UDP":"");

    /* Create server controler thread. */
    pthread_create(&server_controler_tid, NULL, server_controler, NULL);

    for (;exit_flag==0;)
    {
        /* Wait for an I/O event on an epoll file descriptor. */
        nfds = epoll_wait(epollfd, events, MAX_EVENTS_NUM, 500);
        if (nfds == -1)
        {
            WARN("epoll_wait()");
            continue;
        }

        for(i=0; i<nfds; i++)
        {
            if((events[i].data.fd == listen_sock) && (protocol & PROTOCOL_TCP))
            {
                addrlen = sizeof(clientaddr);
                conn_sock = accept(listen_sock,(struct sockaddr *)&clientaddr, &addrlen);
                if(conn_sock == -1)
                {
                    ERROR("accept()");
                    return 5;
                }
                PINFO("connection from %s:%d(fd=%d)\n",
                        inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), conn_sock);
                setnonblocking(conn_sock);
                ev.data.fd = conn_sock;
                ev.events = EPOLLIN|EPOLLET;
                if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev))
                {ERROR("epoll_ctl(EPOLL_CTL_ADD, conn_sock)"); return 6;}
                memset(buf, 0x0, sizeof(buf));
                sprintf(buf, "%s", "SERVER# ");
                write(conn_sock, buf, strlen(buf));
                continue;
            }

            if((events[i].data.fd == udp_sock) && (protocol & PROTOCOL_UDP))
            {
                /* waiting for receive data */
                memset(buf, 0x0, sizeof(buf));
                addrlen = sizeof(clientaddr);
                n = recvfrom(udp_sock, buf, MAX_DATA_BUF_SIZE-1, 0, (struct sockaddr *)&clientaddr, &addrlen);
                PINFO("UDP %s:%d(fd=%d)->%s<-\n",
                        inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), udp_sock, buf);
                message_process(udp_sock, NULL, (struct sockaddr *)&clientaddr, addrlen);
                continue;
            }

            if(events[i].events & EPOLLIN)
            {
                conn_sock = events[i].data.fd;
                if (conn_sock < 0)
                {
                    continue;
                }

                addrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, addrlen);
                getpeername(conn_sock, (struct sockaddr *)&clientaddr, &addrlen);

                memset(buf, 0x0, sizeof(buf));
                n = read(conn_sock, buf, MAX_DATA_BUF_SIZE-1);
                if (n == 0)
                {
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, conn_sock, &ev);
                    close(conn_sock);
                    PINFO("connection close %s:%d(fd=%d)\n",
                            inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), conn_sock);
                    continue;
                }

                PINFO("TCP %s:%d(fd=%d)->%s<-\n",
                        inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), conn_sock, buf);
                for(;n>=MAX_DATA_BUF_SIZE-1;)
                {
                    memset(buf, 0x0, sizeof(buf));
                    n = read(conn_sock, buf, MAX_DATA_BUF_SIZE-1);
                    if(n > 0)
                    {
                        PINFO("%s", buf);
                    }
                }
                message_process(conn_sock, NULL, NULL, 0);
                continue;
            }
        }
    }

    /* wait for server controler thread termination */
    pthread_join(server_controler_tid, NULL);
    close(listen_sock);
    close(epollfd);
    return 0;
}

void *server_controler(void *arg)
{
    char    buf[MAX_DATA_BUF_SIZE];
    int     client_fd;
    char    *message;

    for(;;)
    {
        fgets(buf, MAX_DATA_BUF_SIZE,stdin);
        if(strcasecmp("exit\n",buf)==0)
        {
            exit_flag = 1;
            break;
        }
        else if(strncasecmp("send ",buf,5)==0)
        {
            client_fd = atoi(buf+5);
            if(client_fd > 2)
            {
                write(client_fd, "\n-> ", strlen("\n-> "));
                message = strstr(buf,"/");
                if(-1 == write(client_fd, message+1, strlen(message+1))){ERROR();}
                write(client_fd, "SERVER# ", strlen("SERVER# "));
            }
        }
        else
        {
            buf[strlen(buf)-1]='\0';
            WARN("unknown data : %s\n", buf);
        }
    }
    return (void*)0;
}

int message_process(int client, MSG *message, const struct sockaddr *to, socklen_t tolen)
{
    sendto(client, "SERVER# ", strlen("SERVER# "), 0, to, tolen);
    return 0;
}

void print_host_info()
{
    struct utsname  info;
    struct hostent  *host;
    if(uname(&info)){ERROR(" uname()");}
    PINFO("%s, %s, %s, %s, %s\n", info.sysname, info.nodename, info.release, info.version, info.machine);
    host = gethostbyname(info.nodename);
    if(host == NULL){ERROR(" gethostbyname()");}
    PINFO("official hostname: %s\n", host->h_name);
}

int setnonblocking(int sock)
{
    int opts = fcntl(sock, F_GETFL);
    if(opts < 0){ERROR("fcntl(sock, GETFL)"); return 1;}
    opts = fcntl(sock, F_SETFL, opts|O_NONBLOCK);
    if(opts < 0){ERROR("fcntl(sock,SETFL,opts)"); return 2;}
    return 0;
}
