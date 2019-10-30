// Created by Mivik
// At 2019.10.28

#include "lena.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <set>

#include <pthread.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>

using namespace std;

// ========================Main Part========================

typedef int socket_t;

socket_t UDP_SOCKET,TCP_SOCKET;
sockaddr_in ServerAddr,ClientAddr,TCPAddr;
socklen_t socklen;
TPacket packet;

char FILE_BUFFER[FILEPART_SIZE];
void *TaskUDPRecv(void *args) {
	return 0;
}
pthread_t _TaskUDPRecv;

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
	ClientAddr=ServerAddr;
	ServerAddr.sin_port=htons(PORT_SERVER);
	ClientAddr.sin_port=htons(PORT_CLIENT);
	if (::bind(UDP_SOCKET,(sockaddr*)&ClientAddr,socklen)<0) {
		reportError("Failed to bind UDP socket");
		return 1;
	}
	pthread_create(&_TaskUDPRecv,NULL,TaskUDPRecv,0);

	TCPAddr.sin_family=AF_INET;
	TCPAddr.sin_port=htons(PORT_CLIENT);

	int ret;
	while (1) {
		ret=recvfrom(UDP_SOCKET,&packet,sizeof(TPacket),0,(sockaddr*)&ClientAddr,&socklen);
		printf("%s | %s | %s\n",packet.info.name,packet.info.workDir,packet.info.OS);
	}
	return 0;
}