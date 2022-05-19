#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <dirent.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

char *dir, *usrfile, *portC;
int port;

void PrintUsage();
void FTPShell(int clientfd, struct sockaddr_in client_addr);

int main(int argc, char **argv)
{
    int option = 0;
    if (argc != 7)
    {
        PrintUsage();
    }
    else
    {
        while ((option = getopt(argc, argv, "d:p:u:")) != -1)
        {
            switch (option)
            {
            case 'd':
                dir = optarg;
                break;
            case 'p':
                portC = optarg;
                break;
            case 'u':
                usrfile = optarg;
                break;
            default:
                PrintUsage();
                break;
            }
        }
    }

    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        printf("Could not open the directory!");
        exit(-1);
    }
    closedir(dp);

    port = atoi(portC);
    if (port == 0)
    {
        printf("Please enter a valid port number!");
        exit(-1);
    }

    FILE *fp = fopen(usrfile, "r");
    if (fp == NULL)
    {
        printf("Password file could not be found!");
        exit(-1);
    }
    fclose(fp);

    int serverfd, clientfd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0)
    {
        printf("[!]Socket creating failed!");
        exit(-1);
    }
    else
    {
        printf("[+]Socket created successfully\n");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    const int opt = 1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int bindR = bind(serverfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bindR < 0)
    {
        printf("[!]Socket binding failed!");
        exit(-1);
    }
    else
    {
        printf("[+]Socket binded successfully!\n");
    }

    int listenR = listen(serverfd, SOMAXCONN);
    if (listenR < 0)
    {
        printf("[!]Listening failed!");
        exit(-1);
    }
    else
    {
        printf("[+]Concurrent socket server is now listening on Port %d\n", port);
        printf("[+]Wait for connections...\n");
    }

    pid_t pid;
    while (1)
    {
        int lenght = sizeof client_addr;
        clientfd = accept(serverfd, (struct sockaddr *)&client_addr, (socklen_t *)&lenght);
        if (clientfd < 0)
        {
            printf("[!]Socket could not accept the connection!\n");
            printf("[+]Wait for connections...\n");
            continue;
        }
        else
        {
            char *client_ip = inet_ntoa(client_addr.sin_addr);
            int client_port = ntohs(client_addr.sin_port);
            printf("[+]Server got connection from %s\n", client_ip);

            if ((pid = fork()) == 0)
            {
                close(serverfd);
                FTPShell(clientfd,client_addr);
                printf("[-]Client \"%s:%d\" is disconnected from the server.\n", client_ip, client_port);
                close(clientfd);
                exit(0);
            }
        }
    }

    close(serverfd);

    return 0;
}

void PrintUsage()
{
    printf("Help menu:\n-d <Directory of the file system>\n-p <PORT>\n-u <Password File>");
    exit(-1);
}

