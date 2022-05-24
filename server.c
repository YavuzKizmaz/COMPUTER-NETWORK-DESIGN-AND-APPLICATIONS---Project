#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <dirent.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

char *dir, *usrfile, *portC;
int port;

void PrintUsage();
void FTPShell(int newsockfd, struct sockaddr_in client_addr);
int CommandCheck(const char *command);
int User(const char *, char *);
void List(char *);
void Get(const char *, char *);
void Del(const char *recvbuff, char *sendbuff);

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

    int serverfd, newsockfd;
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
        newsockfd = accept(serverfd, (struct sockaddr *)&client_addr, (socklen_t *)&lenght);
        if (newsockfd < 0)
        {
            printf("[!]Socket could not accept the connection!\n");
            printf("[+]Wait for connections...\n");
            continue;
        }
        else
        {
            char *client_ip = inet_ntoa(client_addr.sin_addr);
            int client_port = ntohs(client_addr.sin_port);
            printf("[+]Server got connection from %s:%d\n", client_ip, client_port);

            if ((pid = fork()) == 0)
            {
                close(serverfd);
                FTPShell(newsockfd, client_addr);
                printf("[-]Client \"%s:%d\" is disconnected from the server.\n", client_ip, client_port);
                shutdown(newsockfd, SHUT_RDWR);
                close(newsockfd);
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

void FTPShell(int newsockfd, struct sockaddr_in client_addr)
{
    char *client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);
    int rcvn;
    char recv_buffer[4096] = {0};
    char send_buffer[4096] = {0};
    char welcome[36] = "Welcome to the Yavuz's file server\n";

    const char *help = "\nHELP (see this help menu)\n\
QUIT (disconnect from the server)\n\
USER <name> <passwd> (to login the file server, name and passwd is needed)\n\
LIST (list all files in the filesystem)\n\
DEL <filename> (delete the given filename)\n\
GET <filename> (print the contents of given filename)\n\
PUT <filename> (write to the given filename, EOF is \\r\\n.\\r\\n )\r\n";

    if (send(newsockfd, welcome, 36, 0) < 0)
    {
        printf("[!]Could not send welcome message to \"%s:%d\"!\n", client_ip, client_port);
        return;
    }

    do
    {
        memset(recv_buffer, 0, 4096);
        memset(send_buffer, 0, 4096);
        rcvn = recv(newsockfd, recv_buffer, 4096, 0);
        if (rcvn > 0)
        {
            printf("[+]Command from \"%s:%d\" =>\t%s", client_ip, client_port, recv_buffer);
            int chk = CommandCheck(recv_buffer);
            switch (chk)
            {
            case -1:
                send(newsockfd, "Unrecognized Command! Type HELP to see all commands\r\n", 54, 0);
                break;
            case 0:
                send(newsockfd, help, 339, 0);
                break;
            case 1:
                send(newsockfd, "Goodbye!\r\n", 10, 0);
                printf("[-]Connection closing from \"%s:%d\"\n", client_ip, client_port);
                return;
                break;
            case 2:
                // user function
                break;
            case 3:
                List(send_buffer);
                send(newsockfd, send_buffer, 4096, 0);
                break;
            case 4:
                // delete function
                break;
            case 5:
                Get(recv_buffer, send_buffer);
                send(newsockfd, send_buffer, 4096, 0);
                break;
            case 6:
                // put function
                break;
            default:
                send(newsockfd, "Unrecognized Command! Type HELP to see all commands\r\n", 54, 0);
                break;
            }
        }
        else if (rcvn < 0)
        {
            printf("[!]Receive failed from \"%s:%d\"!\n", client_ip, client_port);
            break;
        }
        else
        {
            printf("[-]Connection closing from \"%s:%d\"\n", client_ip, client_port);
        }

    } while (rcvn > 0);

    return;
}

int CommandCheck(const char *command)
{
    int return_val;

    if (strncmp("HELP\r", command, 5) == 0)
    {
        return_val = 0;
    }
    else if (strncmp("QUIT\r", command, 5) == 0)
    {
        return_val = 1;
    }
    else if (strncmp("USER ", command, 5) == 0)
    {
        return_val = 2;
    }
    else if (strncmp("LIST\r", command, 5) == 0)
    {
        return_val = 3;
    }
    else if (strncmp("DEL ", command, 4) == 0)
    {
        return_val = 4;
    }
    else if (strncmp("GET ", command, 4) == 0)
    {
        return_val = 5;
    }
    else if (strncmp("PUT ", command, 4) == 0)
    {
        return_val = 6;
    }
    else
    {
        return_val = -1;
    }

    return return_val;
}

void List(char *buff)
{
    memset(buff, 0, 4096);

    DIR *dp = opendir(dir);
    struct dirent *de;
    while ((de = readdir(dp)) != NULL)
    {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        char s[8] = {0};
        snprintf(s, 8, "%hu", de->d_reclen);
        strcat(buff, de->d_name);
        strcat(buff, "\t");
        strcat(buff, s);
        strcat(buff, "\n");
    }
    strcat(buff, ".\r\n");
    closedir(dp);
}

int User(const char *recvbuff, char *sendbuff)
{
}

void Get(const char *recvbuff, char *sendbuff)
{
    char *token = strtok(recvbuff, " ");
    token = strtok(NULL, " ");

    for (size_t i = 0; i < 100; i++)
    {
        if (token[i] == 13 || token[i] == 10)
        {
            token[i] = 0;
        }
    }

    char file[255];
    strcpy(file, dir);
    strcat(file, "/");
    strcat(file, token);

    FILE *fp = fopen(file, "r");
    if (fp == NULL)
    {
        memset(sendbuff, 0, 4096);
        strcat(sendbuff, "400 File ");
        strcat(sendbuff, token);
        strcat(sendbuff, " not found.\r\n.\r\n");
        return;
    }

    else
    {
        char buff[1024] = {0};
        memset(sendbuff, 0, 4096);
        strcat(sendbuff,"This is ");
        strcat(sendbuff,token);
        strcat(sendbuff," content.\n");
        while (fgets(buff, 1024, fp) != NULL)
        {
            strcat(sendbuff,buff);
        }
        strcat(sendbuff,"\r\n.\r\n\n");
    }
    fclose(fp);

    return;
}

void Del(const char *recvbuff, char *sendbuff)
{

}
