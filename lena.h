
#ifndef __LENA_H_
#define __LENA_H_

#include <cstdio>
#include <cstring>

#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <termio.h>
#include <errno.h>

#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

typedef int socket_t;

const char *BROAD_ADDR="255.255.255.255";
const int PORT_CLIENT = 12574;
const int PORT_SERVER = 12575;

const int MAX_LISTEN_SIZE = 512;

const char *DAEMON_PATH="lena";
const char *CONFIG_PATH="%s/.config/lena_client";

const int FILEPART_SIZE = 16384;

const char DOP_KILL = 1;
const char DOP_SET_CONFIG = 2;
const char DOP_GET_CONFIG = 3;
const char DOP_GET_ALL = 4;

template <int BufferSize>
struct BufferedSocket {
	socket_t socket;
	char buffer[BufferSize];
	char *cur,*end;
	BufferedSocket() {}
	BufferedSocket(const socket_t &socket):socket(socket) {cur=end=0;}
	inline int getchar() {
		if (cur==end) {
			int len=recv(socket,cur=buffer,BufferSize,0);
			if (len<0) return EOF;
			end=buffer+len;
		}
		return *(cur++);
	}
	inline void printf(const char *d, ...) {
		va_list args;
		va_start(args,d);
		int len=vsprintf(buffer,d,args);
		send(socket,buffer,len,0);
	}
	inline void close() { ::close(socket);socket=0; }
};
inline void reportError(const char *msg) { fprintf(stderr,"%s: %s(ErrorCode:%d)\n",msg,strerror(errno),errno); }
inline void randomize(char *dst, int isocklen) {
	FILE *file=fopen("/dev/urandom","rb");
	fread(dst,sizeof(char),isocklen,file);
	fclose(file);
}
inline void printHex(FILE *out, const char *src, int len, bool upper=0) {
	int i;
	char q;
	for (i=0;i<len;i++) {
		q=src[i]>>4;
		q&=15;
		fputc(q<10?(q+'0'):(q-10+(upper?'A':'a')),out);
		q=src[i]&15;
		fputc(q<10?(q+'0'):(q-10+(upper?'A':'a')),out);
	}
}
inline void splitLine(FILE *out=stdout, const char c='=') {
	winsize size;
	ioctl(STDIN_FILENO,TIOCGWINSZ,(char*)&size);
	while ((size.ws_col--)>0) fputc(c,out);
}

// ========================Type Defs========================

typedef int TInteger;
typedef bool TBoolean;
typedef char TClientID[32];
typedef char TName[64];
typedef char TString[256];
typedef unsigned int TCardinal;

struct TClientInfo {
	// 0x32
	TInteger version;
	// 0x36
	TClientID clientID;
	TName name;
	TString workDir;
	TBoolean autoLaunch;
	TInteger status;
	TString OS;
	TCardinal permissions;
	TBoolean useComputerName;
	TClientInfo():version(95) { randomize(clientID,sizeof(TClientID)); }
	inline void print(FILE *out=stdout) {
		splitLine(out);
		fprintf(out,"[Name] %s\n",name);
		fprintf(out,"[WorkDir] %s\n",workDir);
		fprintf(out,"[OS] %s\n",OS);
		fprintf(out,"[Cena Protocal Version] %d\n",version);
		fprintf(out,"[ClientID] ");printHex(out,clientID,sizeof(TClientID),1);fputc('\n',out);
		splitLine(out);
		fprintf(out,"\n\b");
	}
};

struct TPacket {
	// 0x2a
	enum Operation : TCardinal {
		BROADCAST=0U,ONLINE=1U,UPDATE=2U,OFFLINE=3U,NAME_CONFLICT=4U,SHOW_TEST_MESSAGE=6U
	};
	TCardinal protocalVersion;
	Operation operation;
	TClientInfo info;
	TPacket():protocalVersion(1) {}
};

struct TTCPPacket {
	TCardinal operation;
	TInteger fileCount;
	TString fileName;
	TInteger fileSize;
};

#endif