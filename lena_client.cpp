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

const char *DEFAULT_CLIENT_NAME				= "linux user";
const char *DEFAULT_CLIENT_OS				= "linux";
const char *DEFAULT_CLIENT_DISPLAY_WORKDIR	= "[HAHA]";
const char *DEFAULT_CLIENT_REAL_WORKDIR		= "~/TEST/";

using namespace std;

inline void version(FILE *out=stdout) {
	fprintf(out,
"\
Lena v1.0.0\n\
Compatiable with Cena v0.8.1.95\n\
Made By Mivik.\n\
"
	);
}
inline void help(FILE *out=stdout) {
	version(out);
	fprintf(out,
"\
\n\
[USAGE]\n\
  start     - Start Lena client (background) if no\n\
              Lena client is currently running.\n\
  stop      - Stop Lena client if exists.\n\
  info      - Show all the basic info.\n\
  config    - Set the config of Lena with following\n\
              syntax:\n\
\n\
                lena config set [name] [value]\n\
                lena config get [name]\n\
\n\
              where config can be any one of the\n\
              followings items:\n\
\n\
                name - the name of user\n\
                os - the operating system of user\n\
                display-path - displayed work path\n\
                real-path - real work path\n\
\n\
              [MENTION] display-path is only for fun.\n\
                It simply changed the displayed work\n\
                path in server, without changing your\n\
                real work path. YOUR FILES ARE ONLY\n\
                COLLECTED FROM YOUR REAL WORK PATH.\n\
\n\
  help (--help,-h)\n\
            - Show this help.\n\
  version(--version,-v)\n\
            - Print the version of Lena and exit.\n\
"
	);
}
// ========================Main Part========================

const char *CLIENT_NAME = DEFAULT_CLIENT_NAME;
const char *CLIENT_OS = DEFAULT_CLIENT_OS;
const char *CLIENT_DISPLAY_WORKDIR = DEFAULT_CLIENT_DISPLAY_WORKDIR;
const char *CLIENT_REAL_WORKDIR = DEFAULT_CLIENT_REAL_WORKDIR;

socket_t UDP_SOCKET,TCP_SOCKET,DAEMON_SOCKET;
sockaddr_in ServerAddr,ClientAddr,TCPAddr;
sockaddr_un DaemonAddr;
socklen_t isocklen,usocklen;
TPacket packet;
pthread_mutex_t mutex_config,mutex_chdir;

char CONFIG_BUFFER[PATH_MAX];
inline void InitPacket(TPacket &packet) {
	packet.operation=TPacket::ONLINE;
	sprintf(CONFIG_BUFFER,CONFIG_PATH,getenv("HOME"));
	FILE *file=fopen(CONFIG_BUFFER,"rb");
	if (!file) {
		strcpy(packet.info.name,CLIENT_NAME);
		strcpy(packet.info.OS,CLIENT_OS);
		strcpy(packet.info.workDir,CLIENT_DISPLAY_WORKDIR);
	} else {
		fread(&packet.info,sizeof(TClientInfo),1,file);
		fclose(file);
	}
}
void *TaskUDPSend(void *args) {
	int ret;
	while (1) {
		pthread_mutex_lock(&mutex_config);
		ret=sendto(UDP_SOCKET,&packet,sizeof(TPacket),0,(sockaddr*)&ServerAddr,isocklen);
		if (ret<0) reportError("Failed to send to server port");
		ret=sendto(UDP_SOCKET,&packet,sizeof(TPacket),0,(sockaddr*)&ClientAddr,isocklen);
		if (ret<0) reportError("Failed to send to client port");
		pthread_mutex_unlock(&mutex_config);
		usleep(3000000);
	}
	return 0;
}
TString LINE;
BufferedSocket<1024> R;
char FILE_BUFFER[FILEPART_SIZE];
void *TaskTCP(void *args) {
	if (listen(TCP_SOCKET,MAX_LISTEN_SIZE)) {
		reportError("Failed to listen TCP connection request");
		exit(1);
		return 0;
	}
	int i,len;
	bool lst;
	int c;
	int ret;
	set<string> nam,ext;
	set<string> *target;
	vector<string> fs;
	string str;
	struct stat sts;
	off_t totalSize=0;
	while (1) {
		if ((R=accept(TCP_SOCKET,NULL,NULL)).socket<0) {
			reportError("Failed to accept a TCP request");
			exit(1);
			return 0;
		}
		puts("[TCP REQUEST BEGIN]");
		nam.clear();
		ext.clear();
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
		pthread_mutex_lock(&mutex_chdir);
		fs.clear();
		DIR *dir=opendir(".");
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
		pthread_mutex_unlock(&mutex_chdir);
		puts("[TCP REQUEST END]");
		R.close();
	}
}
pthread_t _TaskUDPSend,_TaskTCP;
const char *OPTS[]={"name","os","display-path","real-path"};
inline void saveConfig() {
	sprintf(CONFIG_BUFFER,CONFIG_PATH,getenv("HOME"));
	FILE *file=fopen(CONFIG_BUFFER,"wb");
	if (!file) {
		reportError("Failed to save config");
		return;
	}
	fwrite(&packet.info,sizeof(TClientInfo),1,file);
	fclose(file);
}
inline void setConfig(char op, const char *src) {
	printf("[CONFIG %s set to %s]\n",OPTS[op],src);
	if (op==3) { // real path
		pthread_mutex_lock(&mutex_chdir);
		chdir(src);
		pthread_mutex_unlock(&mutex_chdir);
		return;
	}
	pthread_mutex_lock(&mutex_config);
	switch (op) {
		case 0:strcpy(packet.info.name,src);break;
		case 1:strcpy(packet.info.OS,src);break;
		case 2:strcpy(packet.info.workDir,src);break;
	}
	pthread_mutex_unlock(&mutex_config);
	saveConfig();
}
inline const char *getConfig(char op) {
	if (op==3) {
		getcwd(CONFIG_BUFFER,PATH_MAX);
		return CONFIG_BUFFER;
	}
	switch (op) {
		case 0:return packet.info.name;
		case 1:return packet.info.OS;
		case 2:return packet.info.workDir;
	}
	return NULL;
}
inline int getConfigLimit(char op) { return op==3?PATH_MAX:((op==1||op==2)?256:64); }
inline int initDaemonSocket() {
	if ((DAEMON_SOCKET=socket(AF_UNIX,SOCK_STREAM,0))<0) {
		reportError("Failed to create daemon socket");
		return 1;
	}
	{int on=1;setsockopt(DAEMON_SOCKET,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));}
	usocklen=sizeof(DaemonAddr);
	DaemonAddr.sun_family=AF_UNIX;
	DaemonAddr.sun_path[0]=0;
	strcpy(DaemonAddr.sun_path+1,DAEMON_PATH);
	return 0;
}
int daemon_main() {
	freopen("lena-client.log","w",stdout);
	freopen("lena-client.err","w",stderr);
	int pid=fork();
	if (pid<0) {
		fprintf(stderr,"Failed to fork a subprocess\n");
		return 1;
	} else if (pid) return 0;
	pid=setsid();
	if (pid<0) {
		fprintf(stderr,"Failed to setsid\n");
		return 1;
	}
	pthread_mutex_init(&mutex_config,NULL);
	pthread_mutex_init(&mutex_chdir,NULL);
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

	isocklen=sizeof(sockaddr_in);
	ServerAddr.sin_family=AF_INET;
	ServerAddr.sin_addr.s_addr=inet_addr(BROAD_ADDR);
	ClientAddr=ServerAddr;
	ServerAddr.sin_port=htons(PORT_SERVER);
	ClientAddr.sin_port=htons(PORT_CLIENT);
	InitPacket(packet);
	packet.info.print();
	pthread_create(&_TaskUDPSend,NULL,TaskUDPSend,0);

	TCPAddr.sin_family=AF_INET;
	TCPAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	TCPAddr.sin_port=htons(PORT_CLIENT);
	if (::bind(TCP_SOCKET,(sockaddr*)&TCPAddr,isocklen)<0) {
		reportError("Failed to bind TCP socket");
		return 1;
	}
	pthread_create(&_TaskTCP,NULL,TaskTCP,0);

	// All preparations are done. Start daemon socket.
	initDaemonSocket();
	if (::bind(DAEMON_SOCKET,(sockaddr*)&DaemonAddr,usocklen)<0) {
		reportError("Failed to bind daemon socket");
		return 1;
	}
	if (listen(DAEMON_SOCKET,MAX_LISTEN_SIZE)) {
		reportError("Failed to listen daemon connection request");
		return 0;
	}
	while (1) {
		if ((R=accept(DAEMON_SOCKET,NULL,NULL)).socket<0) {
			reportError("Failed to accept a TCP request");
			return 1;
		}
		switch (R.getchar()) {
			case DOP_KILL:printf("[RECEIVED KILL]");R.close();return 0;
			case DOP_SET_CONFIG:{
				char op=R.getchar();
				int lim=getConfigLimit(op);
				int len=0;
				char c=R.getchar();
				while (len<lim&&c!='\0') CONFIG_BUFFER[len++]=c,c=R.getchar();
				while (c!='\0') c=R.getchar();
				CONFIG_BUFFER[len]='\0';
				setConfig(op,CONFIG_BUFFER);
				break;
			}
			case DOP_GET_CONFIG:{
				char op=R.getchar();
				const char *str=getConfig(op);
				send(R.socket,str,strlen(str),0);
				break;
			}
			case DOP_GET_ALL:{
				send(R.socket,&packet.info,sizeof(TClientInfo),0);
				break;
			}
		}
		R.close();
	}
	return 0;
}
inline bool connectToDaemon() {
	if (initDaemonSocket()) return 1;
	if (connect(DAEMON_SOCKET,(sockaddr*)&DaemonAddr,usocklen)<0) return 1;
	return 0;
}

#define ensure(c) do{if(argc<c)goto tooLess;else if(argc>c)goto tooMuch;}while(false);
int main(int argc, char **argv) {
	int i,j;
	if (argc==1) return help(stderr),1;
	char *s=argv[1];
	if (!strcmp(s,"start")) {
		ensure(2);
		if (!connectToDaemon()) {
			puts("Lena client is already running.");
			close(DAEMON_SOCKET);
			return 1;
		}
		return daemon_main();
	}
	if (!strcmp(s,"stop")) {
		ensure(2);
		if (connectToDaemon()) goto connectFailed;
		char op=DOP_KILL;
		if (send(DAEMON_SOCKET,&op,1,0)<0) {
			reportError("Failed to send the kill command");
			return 1;
		}
		close(DAEMON_SOCKET);
		return 0;
	}
	if (!strcmp(s,"config")) {
		if (argc<3) goto tooLess;
		s=argv[2];
		bool get;
		if (!strcmp(s,"get")) get=1;
		else if (!strcmp(s,"set")) get=0;
		else {
			fprintf(stderr,"Unrecognized argument:%s\n",s);
			return 1;
		}
		if (get) ensure(4)
		else ensure(5)

		const int LENGTH=sizeof(OPTS)/sizeof(OPTS[0]);
		for (i=0;i<LENGTH;i++)
			if (!strcmp(argv[3],OPTS[i])) break;
		if (i==LENGTH) {
			fprintf(stderr,"Unrecognized config name: %s\n", argv[2]);
			return 1;
		}
		if (connectToDaemon()) goto connectFailed;
		if (get) {
			char op=DOP_GET_CONFIG;
			if (send(DAEMON_SOCKET,&op,1,0)<0) goto configFailed;
			op=i;
			if (send(DAEMON_SOCKET,&op,1,0)<0) goto configFailed;
			int len;
			if ((len=recv(DAEMON_SOCKET,CONFIG_BUFFER,sizeof(CONFIG_BUFFER),0))<0) {
				fprintf(stderr,"Failed to receive config\n");
				return 1;
			}
			CONFIG_BUFFER[len]='\0';
			puts(CONFIG_BUFFER);
		} else {
			char op=DOP_SET_CONFIG;
			if (send(DAEMON_SOCKET,&op,1,0)<0) goto configFailed;
			op=i;
			if (send(DAEMON_SOCKET,&op,1,0)<0) goto configFailed;
			if (send(DAEMON_SOCKET,argv[4],strlen(argv[4]),0)<0) goto configFailed;
			op='\0';
			if (send(DAEMON_SOCKET,&op,1,0)<0) goto configFailed;
		}
		close(DAEMON_SOCKET);
		return 0;
	}
	if (!strcmp(s,"info")) {
		if (connectToDaemon()) goto connectFailed;
		char op=DOP_GET_ALL;
		if (send(DAEMON_SOCKET,&op,1,0)<0) goto infoFailed;
		if (recv(DAEMON_SOCKET,&packet.info,sizeof(TClientInfo),0)<0) goto infoFailed;
		packet.info.print();
		return 0;
	}
	if ((!strcmp(s,"help"))||(!strcmp(s,"--help"))||(!strcmp(s,"-h"))) {
		ensure(2);
		return help(),0;
	}
	if ((!strcmp(s,"version"))||(!strcmp(s,"--version"))||(!strcmp(s,"-v"))) {
		ensure(2);
		return version(),0;
	}
	fprintf(stderr,"Unrecognized command: %s\n",s);
	return 1;

	tooMuch:
	fprintf(stderr,"Too much arguments. ");
	goto seeHelp;

	tooLess:
	fprintf(stderr,"Too less arguments. ");
	goto seeHelp;

	seeHelp:
	fprintf(stderr,"Use \"lena help\" for help.\n");
	return 1;

	connectFailed:
	reportError("Failed to connect to existing Lena client or no Lena client is running");
	return 1;

	configFailed:
	reportError("Failed to send config command");
	return 1;

	infoFailed:
	reportError("Failed to get info");
	return 1;
}