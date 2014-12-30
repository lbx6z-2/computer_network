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
char help[] = "get get a file from server\nput upload a file to server\npwd display the current directory of server\ndir display the files in the current directory of server\ncd change the directory of server\n? display the whole command which equals 'help'\nquit    return\n";

char currentDirPath[200];
char currentDirName[30];

void cmd_pwd(int sock);
void cmd_dir(int sock);
void cmd_cd(int sock, char* dirName);
void cmd_help(int sock);
void cmd_get(int sock, char* filename);
void cmd_put(int sock, char* filename);


int main(int argc, char* argv[])
{
    int sock;
    int sockmsg;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    sockmsg = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt1 = 1, opt2 = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt1, sizeof(opt1));
    setsockopt(sockmsg, SOL_SOCKET, SO_REUSEADDR, &opt2, sizeof(opt2));
    
    if (sock < 0 || sockmsg < 0) {
        perror("..");
        exit(1);
    }

    // 套接字要绑定到地址上
    struct sockaddr_in server;
    struct sockaddr_in server_msg;

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(40000);

    memset(&server_msg, 0, sizeof(server_msg));
    server_msg.sin_family = AF_INET;
    server_msg.sin_addr.s_addr = htonl(INADDR_ANY);
    server_msg.sin_port = htons(40001);

    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0 ||
        bind(sockmsg, (struct sockaddr*)&server_msg, sizeof(server_msg)) < 0) { 
        perror("binding stream socket: ");
        exit(1);
    }

    printf("socket port: #%d #%d\n", ntohs(server.sin_port), ntohs(server_msg.sin_port));

    // 初始化当前工作路径
    getcwd(currentDirPath, sizeof(currentDirPath));

    // 最大连接数：2
    listen(sock, 2);
    listen(sockmsg, 2);

    int datasock, msgsock;
    pid_t child;    // 子进程号
    char client_cmd[10], cmd_arg[20];   // 命令，命令参数
    while(1) {
        datasock = accept(sock, (struct sockaddr*)0, (socklen_t*)0);
        msgsock = accept(sockmsg, (struct sockaddr*)0, (socklen_t*)0);
        if (datasock == -1 || msgsock == -1) {
            perror("accept:");
        }
        else {
            if ((child = fork()) == -1) {
                printf("fork error\n");
            }
            if (child == 0) {
                printf("new client...\n");
                write(datasock, help, strlen(help) + 1);
                printf("end help...\n");
                while(1) {
                    memset(client_cmd, 0, sizeof(client_cmd));
                    int rval = read(msgsock, client_cmd, sizeof(client_cmd));   // 接收到的
                    if (rval < 0)
                        perror("read command failed...\n");
                    else if (rval == 0) {
                        printf("connection closed...\n");
                        close(datasock);
                        close(msgsock);
                    }
                    else {
                        if (strcmp(client_cmd, "pwd") == 0) {
                            printf("command pwd\n");
                            cmd_pwd(datasock);
                            printf("done\n\n");
                            continue;
                        }
                        else if (strcmp(client_cmd, "dir") == 0) {
                            printf("command dir\n");
                            cmd_dir(datasock);
                            printf("done\n\n");
                            continue;
                        }
                        else if (strcmp(client_cmd, "cd") == 0) {
                            printf("command cd\n");
                            memset(cmd_arg, 0, sizeof(cmd_arg));
                            read(msgsock, cmd_arg, sizeof(cmd_arg));
                            cmd_cd(datasock, cmd_arg);
                            printf("done\n\n");
                            continue;
                        }
                        else if (strcmp(client_cmd, "get") == 0) {
                            printf("command get\n");
                            memset(cmd_arg, 0, sizeof(cmd_arg));
                            read(msgsock, cmd_arg, sizeof(cmd_arg));
                            cmd_get(datasock, cmd_arg);
                            printf("done\n\n");
                            continue;
                        }
                        else if (strcmp(client_cmd, "put") == 0) {
                            printf("command put\n");
                            memset(cmd_arg, 0, sizeof(cmd_arg));
                            read(msgsock, cmd_arg, sizeof(cmd_arg));
                            cmd_put(datasock, cmd_arg);
                            printf("done\n\n");
                            continue;
                        }
                        else if (strcmp(client_cmd, "?") == 0) {
                            printf("command ?\n");
                            cmd_help(datasock);
                            printf("done\n\n");
                            continue;
                        }
                        else if (strcmp(client_cmd, "quit") == 0) {
                            printf("quit\n");
                            printf("connection closed.\n");
                            close(datasock);
                            close(msgsock);
                            exit(1);
                        }
                        else {
                            printf("bad request!\n");
                            continue;
                        }
                    }
                }
            }
        }
    }
    return 0;
}


void getDirName(char* name, char* path)
{
    if (path == NULL) {
        printf("path is null...\n");
    }
    int len = strlen(path);
    int pos = -1;   // '/'的位置
    for (int i = len - 1; i >= 0; i --) {
        if (path[i] == '/') {
            pos = i;
            break;
        }
    }
    for (int i = pos + 1; i <= len; i ++)
        name[i - pos - 1] = path[i];
}

// 获取当前的工作目录
void cmd_pwd(int sock){
    getDirName(currentDirName, currentDirPath);
    write(sock, currentDirName, strlen(currentDirName) + 1);
}

void cmd_dir(int sock){
    DIR* pdir;
    struct dirent* pent;
    // 返回文件数
    pdir = opendir(currentDirPath);
    int num = 0;
    while (pent = readdir(pdir)) {
        num ++;
    }
    write(sock, &num, sizeof(num));
    closedir(pdir);

    // 返回文件名
    if (num <= 0) return;
    else {
        pdir = opendir(currentDirPath);
        while (pent = readdir(pdir)) {
            // 获取文件路径            
            char* fileName = pent -> d_name;
            char filePath[100];
            memset(filePath, 0, sizeof(filePath));
            strcpy(filePath, currentDirPath);
            strcat(filePath, "/");
            strcat(filePath, fileName);
            
            // 得到文件状态值
            int fd = open(filePath, O_RDONLY, S_IREAD);
            struct stat fileStat;
            fstat(fd, &fileStat);

            // 标记文件or目录，写回
            char fileInfo[50];
            memset(fileInfo, 0, sizeof(fileInfo));
            if (S_ISDIR(fileStat.st_mode)) {
                strcpy(fileInfo, "DIR\t");
                strcat(fileInfo, fileName);
            }
            else {
                strcpy(fileInfo, "FILE\t");
                strcat(fileInfo, fileName);
            }
            write(sock, fileInfo, sizeof(fileInfo));
        }
        closedir(pdir);
    }
}

void cmd_cd(int sock, char* dirName){
    DIR* pdir = opendir(currentDirPath);
    bool flag = 0;
    struct dirent* pent;
    while (pent = readdir(pdir)) {
        if (strcmp(pent->d_name, dirName) == 0) {
            if (strcmp(dirName, "..") == 0) {
                int len = strlen(currentDirPath);
                int pos = len;
                for (int i = len - 1; i >= 0; i --) {
                    if (currentDirPath[i] == '/') {
                        pos = i;
                        break;
                    }
                }
                if (pos < len)
                    currentDirPath[pos] = '\0';
            }
            else {
                strcat(currentDirPath, "/");
                strcat(currentDirPath, dirName);
            }
            getDirName(currentDirName, currentDirPath);
            flag = 1;
            break;
        }
    }
    if (flag) {
        write(sock, currentDirPath, strlen(currentDirPath) + 1);
        printf("%s\n", currentDirPath);
    }
    else {
        char tmp[] = "not exists...\0";
        write(sock, tmp, strlen(tmp) + 1);
        printf("%s\n", tmp);
    }
    closedir(pdir);
}

void cmd_help(int sock){
    write(sock, help, strlen(help) + 1);
}

void cmd_get(int sock, char* filename) {
    char filePath[200];
    memset(filePath, 0, sizeof(filePath));
    strcpy(filePath, currentDirPath);
    strcat(filePath, "/");
    strcat(filePath, filename);

    int fd = open(filePath, O_RDONLY, S_IREAD);
    if (fd != -1) {
        struct stat fileStat;
        fstat(fd, &fileStat);
        long fileSize = (long)fileStat.st_size;
        printf("fileSize: %ld\n", fileSize);
        write(sock, &fileSize, sizeof(long));
        char buf[dataLen];
        memset(buf, 0, dataLen);
        while (fileSize > dataLen) {
            read(fd, buf, dataLen);
            write(sock, buf, dataLen);
            fileSize = fileSize - dataLen;
        }
        if (fileSize > 0) {
            read(fd, buf, fileSize);
            write(sock, buf, fileSize);
        }
        close(fd);
        printf("transfer completed\n");
    }
    else {
        printf("open file failed..\n");
    }
}


void cmd_put(int sock, char* fileName) {
    int fd;
    long fileSize;
    
    char filePath[200], buf[dataLen];
    strcpy(filePath, currentDirPath);
    strcat(filePath, "/");
    strcat(filePath, fileName);
    
    fd = open(filePath, O_RDWR|O_CREAT, S_IREAD|S_IWRITE);
    if (fd != -1) {
        memset(buf, 0, dataLen);
        read(sock, &fileSize, sizeof(long));
        while (fileSize > dataLen) {
            read(sock, buf, dataLen);
            write(fd, buf, dataLen);
            fileSize = fileSize - dataLen;
        }
        read(sock, buf, fileSize);
        write(fd, buf, fileSize);
        close(fd);
        printf("transfer completed\n");
    }
    else
        printf("open file %s failed\n", filePath);
}













































