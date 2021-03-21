/* server_poll.c : socket programming in linux */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#define MAX_DATA_BUF_SIZE   256
#define MAX_EVENTS_NUM      32

int setnonblocking(int sock)
{
    int     opts;

    opts = fcntl(sock, F_GETFL);
    if(opts < 0)
    {
        perror("fcntl(sock, GETFL)");
        return 1;
    }
    opts = opts|O_NONBLOCK;
    if(fcntl(sock, F_SETFL, opts) < 0)
    {
        perror("fcntl(sock,SETFL,opts)");
        return 2;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    struct pollfd   fds[MAX_EVENTS_NUM];
    int             listen_sock, conn_sock;
    int             timeout,ret,i,length;
    nfds_t          nfds;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t       addr_len;
    char            buf[MAX_DATA_BUF_SIZE];

    if(argc != 2)
    {
        printf("Usage: %s listen-port\n", argv[0]);
        return 1;
    }

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    setnonblocking(listen_sock);
    memset(&server_addr, 0x0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(listen_sock, 5);

    fds[0].fd = listen_sock;
    fds[0].events = POLLIN|POLLPRI;
    nfds = 1;
    timeout = 10; // 10ms timeout

    memset(&client_addr, 0x0, sizeof(client_addr));
    for(;;)
    {
        for(i=0; i<nfds; i++)
        {
            fds[i].revents = 0;
        }

        ret = poll(fds, nfds, timeout);
        if (ret < 0)
        {
            // error.
            break;
        }
        else if (ret == 0)
        {
            // timed out and no file descriptors were ready.
            continue;
        }

        if((fds[0].revents & POLLIN) || (fds[0].revents & POLLPRI))
        {
            addr_len = sizeof(client_addr);
            conn_sock = accept(fds[0].fd, (struct sockaddr*)&client_addr, &addr_len);
            if (conn_sock  < 0 )
            {
                // error.
                continue;
            }
            fds[nfds].fd = conn_sock;
            fds[nfds].events = POLLIN|POLLPRI;
            fds[nfds].revents = 0;
            nfds ++;
            printf("+++ connetc from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            setnonblocking(conn_sock);
        }

        for(i=1; i<nfds; i++)
        {
            if((fds[i].revents & POLLIN) || (fds[i].revents & POLLPRI))
            {
                memset(buf, 0x0, sizeof(buf));
                ret = read(fds[i].fd, buf, MAX_DATA_BUF_SIZE-1);
                if (ret <= 0)
                {
                    close(fds[i].fd);
                    printf("+++ Close fd %d.\n", fds[i].fd);
                    nfds--;
                    continue;
                }

                length = ret;
                printf("+++ Receive Data From fd %d.->%s", fds[i].fd, buf);
                for(;ret>0;)
                {
                    memset(buf, 0x0, sizeof(buf));
                    ret = read(conn_sock, buf, MAX_DATA_BUF_SIZE-1);
                    length += ret;
                    printf("%s", buf);
                }
                printf("<- length=%lu\n", length);
            }
        }
    }

    close(listen_sock);
    return 0;
}
