/* client_udp.c : socket programming in linux */
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#define MAX_DATA_BUF_SIZE   256

int main(int argc, char *argv[])
{
    int         sockfd;
    struct sockaddr_in serv_addr;
    short       server_port = 0;
    char        buffer[MAX_DATA_BUF_SIZE];
    int         n;

    /* check args */
    if(argc <= 2)
    {
        printf("+++ Error: not enough parameters!\n");
        return 1;
    }

    /* init servaddr */
    memset(&serv_addr, 0, sizeof(serv_addr));
    server_port = atoi(argv[2]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    /* connect to server */
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("connect error");
        return 2;;
    }

    memset(buffer, 0, sizeof(buffer));
    while(fgets(buffer, sizeof(buffer), stdin) != NULL)
    {
        /* read a line and send to server */
        write(sockfd, buffer, strlen(buffer));
        /* receive data from server */
        n = read(sockfd, buffer, sizeof(buffer) -1);
        if(n == -1)
        {
            perror("read error");
            return 3;;
        }
        buffer[n] = 0; /* terminate string */
        fputs(buffer, stdout);
        memset(buffer, 0, sizeof(buffer));
    }

    return 0;
}

