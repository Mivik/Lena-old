// Created by Mivik
// At 2019.10.28

#include "lena.h"

#include <cstdlib>
#include <string>
#include <typeinfo>
#include <unordered_map>

#include <pthread.h>
#include <dirent.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>

using namespace std;

// ========================Main Part========================

typedef int socket_t;
typedef unsigned int addr_t;

socket_t UDP_SOCKET,TCP_SOCKET;
sockaddr_in ServerAddr,ClientAddr,TCPAddr;
socklen_t socklen;
TPacket packet;
bool RE;

char FILE_BUFFER[FILEPART_SIZE];

unordered_map<TClientInfo,addr_t> clients;

inline void reprint() {
	printf("\033c");fflush(stdout);
	int i=0;
	for (auto iter : clients) {
		const TClientInfo &info = iter.first;
		++i;
		printf("%d: %s | %s | %s | %s\n",i,info.name,info.OS,info.workDir,inet_ntoa((in_addr){iter.second}));
	}
}

void halt(int signo) {
	RE = false;
}

BufferedSocket<1024> R;
int main(int argc, char **args) {
	if ((UDP_SOCKET=socket(AF_INET,SOCK_DGRAM,0))<0) {
		reportError("Failed to create UDP socket");
		return 1;
	}
	{int on=1;setsockopt(UDP_SOCKET,SOL_SOCKET,SO_REUSEADDR|SO_BROADCAST,&on,sizeof(on));}
	if ((TCP_SOCKET=socket(AF_INET,SOCK_STREAM,0))<0) {
		reportError("Failed to create TCP socket");
		return 1;
	}
	{int on=1;setsockopt(TCP_SOCKET,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));}

	socklen=sizeof(sockaddr_in);
	ServerAddr.sin_family=AF_INET;
	ServerAddr.sin_addr.s_addr=inet_addr(BROAD_ADDR);
	ServerAddr.sin_port=htons(PORT_SERVER);
	if (::bind(UDP_SOCKET,(sockaddr*)&ServerAddr,socklen)<0) {
		reportError("Failed to bind UDP socket");
		return 1;
	}

	TCPAddr.sin_family=AF_INET;
	TCPAddr.sin_port=htons(PORT_CLIENT);

	int ret;
	signal(SIGINT,halt);
	RE = 1;
	while (1) {
		ret=recvfrom(UDP_SOCKET,&packet,sizeof(TPacket),0,(sockaddr*)&ClientAddr,&socklen);
		addr_t cur = ClientAddr.sin_addr.s_addr;
		if (!RE) break;
		if (clients.find(packet.info)==clients.end()||clients[packet.info]!=cur) {
			clients[packet.info]=cur;
			reprint();
		}
	}
	printf("Input ID: ");
	scanf("%d",&ret);
	addr_t &ip=TCPAddr.sin_addr.s_addr;
	for (auto i : clients) {
		ip=i.second;
		if (!(--ret)) break;
	}
	printf("\033c");fflush(stdout);
	printf("Connecting to %s\n",inet_ntoa((in_addr){ip}));
	if (connect(TCP_SOCKET,(sockaddr*)&TCPAddr,socklen)<0) {
		reportError("Failed to connect");
		return 1;
	}
	puts("Connected successfully, fetching files...");
	strcpy(FILE_BUFFER,"GET\r\nAccept-Name: crazy\r\nAccept-Name: transporter\r\nAccept-Name: fish\r\nAccept-Extension: cpp\r\n\r\n");
	if (send(TCP_SOCKET,FILE_BUFFER,strlen(FILE_BUFFER),0)<0) {
		reportError("Failed to fetch");
		return 1;
	}
	R.socket=TCP_SOCKET;
	int c=R.getchar();
	while (c!=EOF) putchar(c),c=R.getchar();
	strcpy(FILE_BUFFER,"DONE\r\n\r\n");
	if (send(TCP_SOCKET,FILE_BUFFER,strlen(FILE_BUFFER),0)<0) {
		reportError("Failed to send DONE");
		return 1;
	}

	R.close();
	return 0;
}