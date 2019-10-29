// Created by Mivik
// At 2019.10.28
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <set>

#include <termio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <dirent.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const char *DEFAULT_CLIENT_NAME				= "linux user";
const char *DEFAULT_CLIENT_OS				= "linux";
const char *DEFAULT_CLIENT_DISPLAY_WORKDIR	= "[HAHA]";
const char *DEFAULT_CLIENT_REAL_WORKDIR		= "~/TEST/";

const int FILEPART_SIZE = 16384;
using namespace std;

inline void randomize(char *dst, int socklen) {
	FILE *file=fopen("/dev/urandom","rb");
	fread(dst,sizeof(char),socklen,file);
	fclose(file);
}
inline void dump(char *dst, const char *src) { int socklen=strlen(src);memcpy(dst,src,socklen);dst[socklen]='\0'; }
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

// ========================Main Part========================

const char *BROAD_ADDR="255.255.255.255";
const int PORT_CLIENT = 12574;
const int PORT_SERVER = 12575;

const char *CLIENT_NAME = DEFAULT_CLIENT_NAME;
const char *CLIENT_OS = DEFAULT_CLIENT_OS;
const char *CLIENT_DISPLAY_WORKDIR = DEFAULT_CLIENT_DISPLAY_WORKDIR;
const char *CLIENT_REAL_WORKDIR = DEFAULT_CLIENT_REAL_WORKDIR;

typedef int socket_t;

socket_t UDP_SOCKET,TCP_SOCKET;
sockaddr_in ServerAddr,ClientAddr,TCPAddr;
socklen_t socklen;
TPacket packet;
void reportError(const char *msg) { fprintf(stderr,"%s: %s(ErrorCode:%d)\n",msg,strerror(errno),errno); }

inline void InitPacket(TPacket &packet) {
	packet.operation=TPacket::ONLINE;
	dump(packet.info.name,CLIENT_NAME);
	dump(packet.info.OS,CLIENT_OS);
	dump(packet.info.workDir,CLIENT_DISPLAY_WORKDIR);
}
void *TaskUDPSend(void *args) {
	int ret;
	while (1) {
		ret=sendto(UDP_SOCKET,&packet,sizeof(TPacket),0,(sockaddr*)&ServerAddr,socklen);
		if (ret<0) reportError("Failed to send to server port");
		ret=sendto(UDP_SOCKET,&packet,sizeof(TPacket),0,(sockaddr*)&ClientAddr,socklen);
		if (ret<0) reportError("Failed to send to client port");
		usleep(3000000);
	}
	return 0;
}
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
	void printf(const char *d, ...) {
		va_list args;
		va_start(args,d);
		int len=vsprintf(buffer,d,args);
		send(socket,buffer,len,0);
	}
};
TString LINE;
BufferedSocket<1024> RTCP_SOCKET;
char FILE_BUFFER[FILEPART_SIZE];
void *TaskTCP(void *args) {
	if (listen(TCP_SOCKET,600)) {
		reportError("Failed to listen TCP connection request");
		return 0;
	}
	int i,len;
	bool lst;
	char c;
	int ret;
	set<string> nam,ext;
	set<string> *target;
	vector<string> fs;
	string str;
	struct stat sts;
	off_t totalSize=0;
	BufferedSocket<1024> &R=RTCP_SOCKET;
	while (1) {
		if ((R=accept(TCP_SOCKET,NULL,NULL)).socket<0) {
			reportError("Failed to accept a TCP request");
			return 0;
		}
		puts("[TCP REQUEST BEGIN]");
		for (i=0;;i++) {
			c=R.getchar();
			if (c==EOF) break;
			if (c=='\r'||c=='\n') break;
			else {
				lst=0;
				if (c!='A') {
					while (c!='\n'&&c!=EOF) c=R.getchar();
					if (c==EOF) break;
					continue;
				}
				do c=R.getchar(); while (c!='-'&&c!=EOF);
				if (c==EOF) break;
				c=R.getchar();
				if (c==EOF) break;
				target=c=='N'?(&nam):(&ext);
				do c=R.getchar(); while (c!=' '&&c!=EOF);
				if (c==EOF) break;
				c=R.getchar();
				len=0;
				while (c!='\n'&&c!='\r'&&c!=EOF) {
					LINE[len++]=c;
					c=R.getchar();
				}
				LINE[len]='\0';
				if (c==EOF) break;
				puts(LINE);
				str=LINE;
				target->insert(str);
				while (c!='\n') c=R.getchar();
			}
		}
		DIR *dir=opendir(CLIENT_REAL_WORKDIR);
		dirent *file;
		while ((file=readdir(dir))!=NULL) {
			char *name=file->d_name;
			stat(name,&sts);
			if (S_ISDIR(sts.st_mode)) continue;
			int flen=strlen(name);
			for (len=flen-1;len>=0&&name[len]!='.';len--);
			if (len==-1) continue;
			str=name+len+1;
			if (ext.find(str)==ext.end()) continue;
			name[len]='\0';
			str=name;
			if (nam.find(str)==nam.end()) continue;
			name[len]='.';
			str=name;
			fs.push_back(str);
			totalSize+=sts.st_size;
		}
		R.printf("OK\r\nFiles-Count: %d\r\nTotal-Length: %lld\r\n\r\n",fs.size(),totalSize);
		for (string filename : fs) {
			printf("Sending: %s\n",filename.c_str());
			stat(filename.c_str(),&sts);
			R.printf("FILE\r\nFile-Name: %s\r\nStatus: 0\r\nFile-Size: %lld\r\n\r\n",filename.c_str(),sts.st_size);
			int lim=sts.st_size/FILEPART_SIZE;
			FILE *cfile=fopen(filename.c_str(),"r");
			for (i=0;i<lim;i++) {
				R.printf("FILEPART\r\nStatus: 0\r\nContent-Length: %d\r\n\r\n",FILEPART_SIZE);
				fread(FILE_BUFFER,sizeof(char),FILEPART_SIZE,cfile);
				send(R.socket,FILE_BUFFER,FILEPART_SIZE,0);
			}
			lim=sts.st_size%FILEPART_SIZE;
			if (lim) {
				R.printf("FILEPART\r\nStatus: 0\r\nContent-Length: %d\r\n\r\n",lim);
				fread(FILE_BUFFER,sizeof(char),lim,cfile);
				send(R.socket,FILE_BUFFER,lim,0);
			}
		}
		puts("[TCP REQUEST END]");
		close(R.socket);
		R.socket=0;
	}
}
pthread_t _TaskUDPSend,_TaskTCP;

void CloseSockets(int signo) {
	close(UDP_SOCKET);
	close(TCP_SOCKET);
	if (RTCP_SOCKET.socket) close(RTCP_SOCKET.socket);
}
int main(int argc, char **args) {
	for (int i=1;i<argc;i++) {
		char *s=args[i];
		if (s[0]=='-') {
			if (i==argc-1) break;
			switch (s[1]) {
				case 'n':CLIENT_NAME=args[i+1];break;
				case 'o':CLIENT_OS=args[i+1];break;
				case 'd':CLIENT_DISPLAY_WORKDIR=args[i+1];break;
				case 'r':CLIENT_REAL_WORKDIR=args[i+1];break;
				default:--i;
			}
			++i;
		} else {
			printf("Unrecognized argument:%s\n",args[i]);
			return 1;
		}
	}
	chdir(CLIENT_REAL_WORKDIR);
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
	/*signal(SIGINT,CloseSockets);
	signal(SIGABRT,CloseSockets);
	signal(SIGILL,CloseSockets);
	signal(SIGQUIT,CloseSockets);
	signal(SIGHUP,CloseSockets);*/

	socklen=sizeof(sockaddr_in);
	ServerAddr.sin_family=AF_INET;
	ServerAddr.sin_addr.s_addr=inet_addr(BROAD_ADDR);
	ClientAddr=ServerAddr;
	ServerAddr.sin_port=htons(PORT_SERVER);
	ClientAddr.sin_port=htons(PORT_CLIENT);
	if (bind(UDP_SOCKET,(sockaddr*)&ClientAddr,socklen)<0) {
		reportError("Failed to bind UDP socket");
		return 1;
	}
	InitPacket(packet);
	packet.info.print();
	pthread_create(&_TaskUDPSend,NULL,TaskUDPSend,0);

	TCPAddr.sin_family=AF_INET;
	TCPAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	TCPAddr.sin_port=htons(PORT_CLIENT);
	if (bind(TCP_SOCKET,(sockaddr*)&TCPAddr,socklen)<0) {
		reportError("Failed to bind TCP socket");
		return 1;
	}
	pthread_create(&_TaskTCP,NULL,TaskTCP,0);

	int ret;
	while (1) {
		ret=recvfrom(UDP_SOCKET,&packet,sizeof(TPacket),0,(sockaddr*)&ClientAddr,&socklen);
		printf("%s | %s | %s\n",packet.info.name,packet.info.workDir,packet.info.OS);
	}
	close(UDP_SOCKET);
	return 0;
}