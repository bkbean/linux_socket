/* server_select.c : socket programming in linux */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#define MAX_DATA_BUF_SIZE   256         /* 最大数据缓存大小 */
#define MAX_CONN_NUM        256         /* 最大客户连接数 */
#define MAX_LISTEN_BACKLOG  5           /* 最大同时连接请求数 */
int     exit_flag = 0;
int     *client_fd, client_count = 0;   /* accepted connection fd */
void *server_controler(void *arg);

// program parameter :
// server 服务器监听端口号
int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr;     /* 本机地址信息 */
    struct sockaddr_in client_addr;     /* 客户端地址信息 */
    int             sockfd;             /* sock_fd：监听socket */
    short           listen_port = 0;    /* 服务器监听端口号 */
    int             sin_size;
    char            command;
    int             nfds, client_newfd; // accepted connection fd
    fd_set          readfds;
    struct timeval  timeout;
    char            buffer[MAX_DATA_BUF_SIZE];
    int             i,ret;
    pthread_t       server_controler_tid;

    if (argc <= 2)
    {
        printf("+++ Error: not enough parameters!\n");
        return 1;
    }

    listen_port = atoi(argv[1]);
    client_fd = malloc(sizeof(int) * MAX_CONN_NUM);
    memset(client_fd, -1, sizeof(int) * MAX_CONN_NUM);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
    {
        perror("socket创建出错！\n");
        return 2;
    }

    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(listen_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
    if (ret == -1)
    {
        perror("bind出错！\n");
        return 3;
    }

    ret = listen(sockfd, MAX_LISTEN_BACKLOG);
    if ( ret == -1)
    {
        perror("listen出错！\n");
        return 4;
    }

    // Show Server Infomation
    printf("+++ Server Infomation +++\n");
    printf("+ IP Address:Listen Port        : %s:%d\n", inet_ntoa(server_addr.sin_addr), listen_port);
    printf("+ Server Socket File Descriptor : %d\n", sockfd);
    printf("+++\n");
    nfds = sockfd + 1;

    // create server controler thread
    ret = pthread_create(&server_controler_tid, NULL, server_controler, NULL);
    if(ret != 0)
    {
        perror("+++ Fail to create server controler thread.\n");
    }
    else
    {
        printf("+++ create server controler thread.\n");
    }

    while(exit_flag == 0)
    {
        // initialize file descriptor set
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        // add active connection to fd set
        for(i = 0; i < MAX_CONN_NUM; i++)
        {
            if(client_fd[i] != -1)
            {
                FD_SET(client_fd[i], &readfds);
            }
        }

        // timeout setting
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        ret = select(nfds, &readfds, NULL, NULL, &timeout);
        if(ret < 0)
        {
            perror("+++ Select Error!\n");
            break;
        }
        else if(ret == 0)
        {
            continue;
        }

        // check every fd in the set
        for(i = 0; i < MAX_CONN_NUM; i++)
        {
            if((client_fd[i] != -1) && FD_ISSET(client_fd[i], &readfds))
            {
                memset(buffer, 0 ,sizeof(buffer));
                ret = recv(client_fd[i], buffer, sizeof(buffer) - 1, 0);
                if(ret <= 0)       // client close
                {
                    printf("+++ Client(id:%d,fd:%d) Close.\n", i, client_fd[i]);
                    close(client_fd[i]);
                    client_fd[i] = -1;
                    client_count --;
                }
                else if(ret > 0)    // receive data
                {
                    printf("+++ Receive Data From Client(id:%d,fd:%d) Length:%d\n", i, client_fd[i], ret);
                    printf("%s\n", buffer);
                    sprintf(buffer, "%s", "OK");
                    send(client_fd[i], buffer, strlen(buffer), 0);
                }
            }
        }

        // check whether a new connection comes
        if(FD_ISSET(sockfd, &readfds))
        {
            sin_size = sizeof(struct sockaddr_in);
            client_newfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);

            if (client_newfd == -1)
            {
                perror("accept出错");
                continue;
            }

            if(client_count >= MAX_CONN_NUM)
            {
                printf("+++ Max Connection. Please Wait!\n");
                send(client_newfd, "Bye!", 4, 0);
                close(client_newfd);
                continue;
            }

            // add to fd queue
            sprintf(buffer, "%s", "Hello, you are connected!");
            ret = send(client_newfd, buffer, strlen(buffer), 0);
            if (ret == -1)
            {
                printf("+++ Send To Client(fd:%d) Error.\n", client_newfd);
                close(client_newfd);
                continue;
            }

            for(i=0; i<=client_count; i++)
            {
                if(client_fd[i] == -1)
                {
                    client_fd[i] = client_newfd;
                    printf("+++ Receive New Connection : Client(id:%d,fd:%d) %s:%d\n",
                        i, client_fd[i],
                        inet_ntoa(client_addr.sin_addr),
                        ntohs(client_addr.sin_port));
                    break;
                }
            }

            if(client_newfd + 1 > nfds)
            {
                nfds = client_newfd + 1;
            }
            client_count++;
        }
    }

    /* wait for server controler thread termination */
    pthread_join(server_controler_tid, NULL);

    // close all connections
    for(i = 0; i < MAX_CONN_NUM; i++)
    {
        if(client_fd[i] != -1)
        {
            client_fd[i];
        }
    }
    close(sockfd);

    return 0;
}

void *server_controler(void *arg)
{
    int         i;
    char        command;
    char        buffer[MAX_DATA_BUF_SIZE];

    printf("+ Please select the control operation type:\n");
    printf(" P: print available client fd\n");
    printf(" Q: quit service\n");

    do
    {
        scanf("%c", &command);
        switch (toupper(command))
        {
            case 'P':
                if(client_count <= 0)
                {
                    printf("No Available Client!\n");
                    continue;
                }

                printf("+++ Available Client File Descriptor (id:fd) +++\n");
                for(i = 0; i < MAX_CONN_NUM; i++)
                {
                    if(client_fd[i] != -1)
                    {
                        printf(" (%d:%d) ", i, client_fd[i]);
                    }
                }
                printf("\n+++\n");
                break;

            case 'Q':
                for(i = 0; i < MAX_CONN_NUM; i++)
                {
                    if(client_fd[i] != -1)
                    {
                        sprintf(buffer, "%s", "Server will close. Bye!");
                        send(client_fd[i], buffer, strlen(buffer), 0);
                    }
                }
                exit_flag = 1;
                break;

            default:
                break;
        }
    } while (exit_flag == 0);

    return (void*)0;
}
