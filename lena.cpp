// Created by Mivik
// At 2019.10.28
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <termio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

using namespace std;

// ========================Type Defs========================

typedef int TInteger;
typedef bool TBoolean;
typedef char TClientID[32];
typedef char TName[64];
typedef char TString[256];
typedef unsigned int TCardinal;

struct TClientInfo {
	TInteger version;
	TClientID clientID;
	TName name;
	TString workDir;
	TBoolean autoLaunch;
	TInteger status;
	TString OS;
	TCardinal permissions;
	TBoolean useComputerName;
};

struct TPacket {
	enum Type : TCardinal {
		BROADCAST=0U,ONLINE=1U,UPDATE=2U,OFFLINE=3U,NAME_CONFLICT=4U,SHOW_TEST_MESSAGE=6U
	}
	Type type;
	TClientInfo info;
};

struct TTCPPacket {
	TCardinal operation;
	TInteger fileCount;
	TString fileName;
	TInteger fileSize;
};

// ========================Main Part========================

const char *BROAD_ADDR="255.255.255.255";
const int BROAD_PORT=19260;

int s;
void reportError(const char *msg) { fprintf(stderr,"%s: %s(ErrorCode:%d)\n",msg,strerror(errno),errno); }

int main() {
	int TIME_LIVE = 256;

	if ((s=socket(AF_INET,SOCK_DGRAM,0))<0) {
		reportError("Failed to create socket");
		return 1;
	}

	struct sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	socklen_t len=sizeof(addr);
	addr.sin_family=AF_INET;
	addr.sin_port=htons(BROAD_PORT);
	addr.sin_addr.s_addr=inet_addr(BROAD_ADDR);
	setsockopt(s,IPPROTO_IP,IP_MULTICAST_TTL,(void*)&TIME_LIVE,sizeof(TIME_LIVE));
	// sendto(socket,buffer,len,0,NULL,0)
	sendto(s,"#",1,0,NULL,0);
	close(s);
	return 0;
}