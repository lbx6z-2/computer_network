#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <cstdlib>
#include <cerrno>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <dirent.h>

#define dataLen 1024

void cmd_pwd(int sock);
void cmd_dir(int sock);
void cmd_cd(int sock, char* dirName);
void cmd_help(int sock);
void cmd_get(int sock, char* fileName);
void cmd_put(int sock, char* fileName);


int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("Usage: ./client <address> <port>\n");
        return 0;
    }

    int sock, sockmsg;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    sockmsg = socket(AF_INET, SOCK_STREAM, 0);    
    if (sock < 0 || sockmsg < 0) {
        perror("opening stream socket");    
        exit(1);
    }

    struct sockaddr_in server, servermsg;
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &server.sin_addr);
    servermsg.sin_family = AF_INET;
    servermsg.sin_port = htons(atoi(argv[2]) + 1);
    inet_pton(AF_INET, argv[1], &servermsg.sin_addr);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0 ||
        connect(sockmsg, (struct sockaddr*)&servermsg, sizeof(servermsg)) < 0) {
        perror("connect server failed...");
        exit(1);
    }

    char help[300];
    read(sock, help, sizeof(help));
    printf("%s\n", help);

    char client_cmd[10], cmd_arg[20];
    while(1) {
        memset(client_cmd, 0, sizeof(client_cmd));
        memset(cmd_arg, 0, sizeof(cmd_arg));
        printf("command: ");
        scanf("%s", client_cmd);
        if (strcmp(client_cmd, "quit") == 0) {
            write(sockmsg, client_cmd, sizeof(client_cmd));
            close(sockmsg);
            close(sock);
            printf("connection closed\n\n");
            exit(0);
        }
        else if (strcmp(client_cmd, "?") == 0) {
            write(sockmsg, client_cmd, sizeof(client_cmd));
            cmd_help(sock);
        }
        else if (strcmp(client_cmd, "pwd") == 0) {
            write(sockmsg, client_cmd, sizeof(client_cmd));
            cmd_pwd(sock);
        }
        else if (strcmp(client_cmd, "dir") == 0) {
            write(sockmsg, client_cmd, sizeof(client_cmd));
            cmd_dir(sock);
        }
        else if (strcmp(client_cmd, "cd") == 0) {
            write(sockmsg, client_cmd, sizeof(client_cmd));
            scanf("%s", cmd_arg);
            write(sockmsg, cmd_arg, sizeof(cmd_arg));
            cmd_cd(sock, cmd_arg);
        }
        else if (strcmp(client_cmd, "get") == 0) {
            write(sockmsg, client_cmd, sizeof(client_cmd));
            scanf("%s", cmd_arg);
            write(sockmsg, cmd_arg, sizeof(cmd_arg));
            cmd_get(sock, cmd_arg);
        }
        else if (strcmp(client_cmd, "put") == 0) {
            write(sockmsg, client_cmd, sizeof(client_cmd));
            scanf("%s", cmd_arg);
            write(sockmsg, cmd_arg, sizeof(cmd_arg));
            cmd_put(sock, cmd_arg);
        }
        else
            printf("bad command!\n");
    }

    return 0;
}


void cmd_pwd(int sock) {
    char dirName[30];
    memset(dirName, 0, sizeof(dirName));
    read(sock, dirName, 30);
    printf("%s\n", dirName);
}

void cmd_dir(int sock) {
    int num = 0;
    read(sock, &num, sizeof(num));
    printf("%d files\n", num);    
    while (num > 0) {
        char fileInfo[50];
        memset(fileInfo, 0, 50);
        read(sock, fileInfo, 50);
        printf("%s\n", fileInfo);
        num --;
    }    
}

void cmd_cd(int sock, char* dirName) {
    char dirPath[200];
    memset(dirPath, 0, sizeof(dirPath));
    read(sock, dirPath, 200);
    printf("transferred dir: %s\n", dirPath);
}

void cmd_help(int sock){
    char help[300];
    memset(help, 0, sizeof(help));
    read(sock, help, 300);
    printf("%s\n", help);
}

void cmd_get(int sock, char* fileName) { 
    // 获取文件在当前目录下的path
    char localFilePath[200];
    memset(localFilePath, 0, sizeof(localFilePath));
    getcwd(localFilePath, sizeof(localFilePath));
    strcat(localFilePath, "/");
    strcat(localFilePath, fileName);
    printf("localFilePath: %s\n", localFilePath);
    
    // 打开文件路径
    int fd = open(localFilePath, O_RDWR|O_CREAT, S_IREAD|S_IWRITE);
    // 写入文件
    if (fd != -1) {
        long fileSize = 0;
        read(sock, &fileSize, sizeof(fileSize));
        printf("fileSize: %ld\n", fileSize);
        char buf[dataLen];
        memset(buf, 0, dataLen);
        while (fileSize > dataLen) {
            read(sock, buf, dataLen);
            write(fd, buf, dataLen);
            fileSize = fileSize - dataLen;
        }
        if (fileSize > 0) {
            read(sock, buf, fileSize);
            write(fd, buf, fileSize);       // 往fd中写buf
        }
        close(fd);
        printf("download complete!\n");
    }
    else
        printf("open file %s failed\n", localFilePath);
}


void cmd_put(int sock, char* fileName) {
    int fd;
    long fileSize;
    int numRead;
    char filePath[200];
    struct stat fileStat;
    
    memset(filePath, 0, sizeof(filePath));
    getcwd(filePath, sizeof(filePath));

    strcat(filePath, "/");
    strcat(filePath, fileName);
    fd = open(filePath, O_RDONLY, S_IREAD);
    if (fd != -1) {
        fstat(fd, &fileStat);
        fileSize = (long)fileStat.st_size;
        write(sock, &fileSize, sizeof(long));
        char buf[dataLen];
        memset(buf, 0, dataLen);
        while (fileSize > dataLen) {
            read(fd, buf, dataLen);
            write(sock, buf, dataLen);
            fileSize = fileSize - dataLen;
        }
        printf("fileSize: %ld\n", fileSize);
        read(fd, buf, fileSize);
        write(sock, buf, fileSize);
        close(fd);
        printf("upload completed\n");
    }
    else
        printf("open file %s failed\n", filePath);    
}

