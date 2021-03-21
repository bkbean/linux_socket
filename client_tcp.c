/* client_tcp.c : socket programming in linux */
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#define MAX_DATA_BUF_SIZE   256
int     exit_flag = 0;
int     sockfd;
void *client_controler(void *arg);

// program parameter: client server_ip server_port
int main(int argc, char *argv[])
{
    struct sockaddr_in  serv_addr;
    short       server_port = 0;
    char        buffer[MAX_DATA_BUF_SIZE];
    int         ret;
    pthread_t   client_controler_tid;

    if(argc < 2)
    {
        fprintf(stderr, "Please enter the server's hostname!\n");
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
    {
        perror("socket()");
        return 2;
    }

    server_port = atoi(argv[2]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(server_port);
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    if(ret == -1)
    {
        perror("connect()");
        return 3;
    }

    // create client controler thread
    ret = pthread_create(&client_controler_tid, NULL, client_controler, NULL);
    if(ret != 0)
    {
        perror("+++ Fail to create client controler thread.\n");
    }
    else
    {
        printf("+++ create client controler thread.\n");
    }

    while(exit_flag == 0)
    {
        memset(buffer, 0, sizeof(buffer));
        ret = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if(ret <= 0)       // server close
        {
            printf("+++ Server Close.\n");
            exit_flag = 1;
            break;
        }
        else if(ret > 0)    // receive data
        {
            printf("+ Server Data (Length:%d)->%s\n", ret, buffer);
        }
    }

    /* wait for client controler thread termination */
    pthread_join(client_controler_tid, NULL);

    close(sockfd);
    return 0;
}

void *client_controler(void *arg)
{
    int         i;
    char        command;
    char        buffer[MAX_DATA_BUF_SIZE];

    printf("+ Please select the control operation type:\n");
    printf(" S: input s+message to sent message\n");
    printf(" Q: quit service\n");

    do
    {
        scanf("%c", &command);
        switch (toupper(command))
        {
            case 'S':
                scanf("%256s", buffer);
                send(sockfd, buffer, strlen(buffer), 0);
                break;

            case 'Q':
                exit_flag = 1;
                break;

            default:
                break;
        }
    } while (exit_flag == 0);

    return (void*)0;
}
