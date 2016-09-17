#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>

const int maxn = 1024;
bool ispasv = false;
int controlFd, dataFd;
char *host, *port = (char*)"21";
char buf[maxn];
struct sockaddr_in server;
struct addrinfo hint;	// 过滤地址的模板
struct addrinfo *res;

void init();
void connServerCon();
void connServerDat();
void recvInfo();
void sendInfo();
void login();
void pasv();
void cwd(char *dirname);
void size(char *filename);
void retr(char *filename);
void stor(char *filename);
void quit();
void ll();
void help();
void getFileName(char *filename);

int main(int argc, char **argv) 
{
	if(argc < 2) {
		printf("Input format wrong. Right format: \n%s Ip|Domain [port]\n", argv[0]);
		exit(1);
	}
	host = argv[1];
	if(argc >= 3) port = argv[2];
	init();
	connServerCon();
	login();	
	while(1) { // 处理输入的命令
		char comm[maxn], dirname[maxn], filename[maxn];
		printf("ftp> ");
		scanf("%s", comm);
		if(!strcmp(comm, "pasv")) {
			pasv();	
			connServerDat();
		}
		else if(!strcmp(comm, "cd")) {
			scanf("%s", dirname);
			cwd(dirname);
		}
		else if(!strcmp(comm, "size")) {
			scanf("%s", filename);
			size(filename);
		}
		else if(!strcmp(comm, "get")) {
			scanf("%s", filename);
			if(!ispasv) {
				printf("Need pasv mode to translate data\n");
				continue;
			}
			retr(filename);
		}
		else if(!strcmp(comm, "put")) {
			scanf("%s", filename);
			if(!ispasv) {
				printf("Need pasv mode to translate data\n");
				continue;
			}
			stor(filename);
		}
		else if(!strcmp(comm, "quit")) {
			quit();
			break;
		}
		else if(!strcmp(comm, "ll")) {
			if(!ispasv) {
				printf("Need pasv mode to translate data\n");
				continue;
			}
			ll();
		}
		else if(!strcmp(comm, "help")) {
			help();
		}
		else {
			printf("Command not found\n");
		}
	}
	return 0;
}

void init() {
	memset(&hint, 0, sizeof(hint));
	hint.ai_family = AF_INET;
	hint.ai_flags = AI_PASSIVE;  			   // 套接字地址用于接口绑定监听
	hint.ai_socktype = SOCK_STREAM;
	if(getaddrinfo(host, port, &hint, &res)) { // 出错返回非0错误码
		printf("getaddrinfo wrong!\n");	
		exit(1);
	}
	memcpy(&server, (struct sockaddr_in *)res->ai_addr, sizeof(server));
	controlFd = socket(AF_INET, SOCK_STREAM, 0);
	dataFd = socket(AF_INET, SOCK_STREAM, 0);
}

void connServerCon() {
	char str[16];
	printf("Server: %s(%s)\n", host, inet_ntop(AF_INET, &server.sin_addr, str, 16));
	printf("Port: %s\n", port);
	printf("Connect to server...\n");
	if(connect(controlFd, (struct sockaddr *)&server, sizeof(server)) == -1) {
		printf("Connect wrong!\n");
		exit(1);
	}
	recvInfo();
}

void connServerDat() {
	int tport = 0, tport2 = 0;
	for(int i = 0, cnt = 0; buf[i] != ')'; ++i) {
		if(buf[i] == ',') {cnt++; continue;}
		if(cnt == 4) tport = tport * 10 + buf[i] - '0';
		if(cnt == 5 && buf[i]) tport2 = tport2 * 10 + buf[i] -'0';
	}
	tport = tport * 256 + tport2;
	char dport[maxn];
	sprintf(dport, "%d", tport);
	getaddrinfo("127.0.0.1", dport, 0, &res);
	memcpy(&server, (struct sockaddr_in *)res->ai_addr, sizeof(server));
	if(connect(dataFd, (struct sockaddr *)&server, sizeof(server)) == -1) {
		printf("Data connect wrong!");
	}
}

void recvInfo() {
	memset(buf, 0, sizeof(buf));
	recv(controlFd, buf, maxn, 0);
	printf("--> %s", buf);
}

void sendInfo() {
	send(controlFd, buf, strlen(buf), 0);
}

void login() {
	char name[maxn], passwd[maxn];	
	printf("Username: ");
	scanf("%s", name);
	sprintf(buf, "USER %s\r\n", name);
	sendInfo();
	recvInfo();
	printf("Password: ");
	scanf("%s", passwd);
	sprintf(buf, "PASS %s\r\n", passwd);
	sendInfo();
	recvInfo();
	printf("PS: Input help you will know how to use this program\n");
}

void pasv() {
	sprintf(buf, "PASV\r\n");
	sendInfo();
	recvInfo();
	ispasv = true;
}

void cwd(char *dirname) {
	sprintf(buf, "CWD %s\r\n", dirname);
	sendInfo();
	recvInfo();
}

void size(char *filename) {
	sprintf(buf, "SIZE %s\r\n", filename);
	sendInfo();
	recvInfo();
}

void retr(char *filename) {
	sprintf(buf, "RETR %s\r\n", filename);
	sendInfo();
	recvInfo();
	getFileName(filename);
	printf("%s\n", filename);
	int newFd = open(filename, O_WRONLY | O_CREAT | O_TRUNC);
	if(newFd == -1) {
		printf("Failure on creating %s\n", filename);
	}
	while(1) {
		memset(buf, 0, sizeof(buf));
		if(!recv(dataFd, buf, maxn, 0)) break;
		write(newFd, buf, strlen(buf));
	}
	close(newFd);
	recvInfo();
}

void stor(char *filename) { // 550 permission denied.
	sprintf(buf, "STOR %s\r\n", filename);
	sendInfo();
	recvInfo();
	getFileName(filename);
	int fd = open(filename, O_RDONLY);
	if(fd == -1) {
		printf("File %s don't exit!\n", filename);
	}
	while(1) {
		memset(buf, 0, sizeof(buf));
		if(!read(fd, buf, sizeof(buf))) break;
		send(dataFd, buf, strlen(buf), 0);
	}
	close(fd);
	recvInfo();
}

void quit() {
	sprintf(buf, "QUIT\r\n");
	sendInfo();
	recvInfo();
}

void ll() {
	sprintf(buf, "LIST\r\n");
	sendInfo();
	recvInfo();
	while(1) {
		memset(buf, 0, sizeof(buf));
		if(!recv(dataFd, buf, maxn, 0)) break;
		printf("%s\n", buf);
	}
}

void getFileName(char *filename) {
	char tmp[maxn];
	strcpy(tmp, filename);
	getcwd(filename, maxn);
	strcat(filename, "/");
	strcat(filename, tmp);
}

void help() {
	printf("Command list:\n");
	printf("pasv\t\tPassive mode\n");
	printf("ll\t\tList current dirctory contents\n");
	printf("quit\t\tQuit ftp\n");
	printf("cd dirname\tChange directory to dirname\n");
	printf("get filename\tDownload file\n");
	printf("put filename\tUpload file\n");
	printf("size filename\tGet file size\n");
	printf("PS: You need input \"pasv\" when using \"get\" or \"put\" or \"ll\" command\n");
}
