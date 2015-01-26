#include <algorithm>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <unistd.h>
#include <vector>

#define HTMLDIR "./"
#define CGIDIR "./"
#define PORTNUM 2870
#define MAX_CHAR 10240
#define handle_error(msg) \
	do { cout << "error: " << msg << "<br>"; fflush(stdout); exit(EXIT_FAILURE); } while (0)

using namespace std;

void client(int sockfd);
void display(int sockfd,string s);
int readline(int fd, char* ptr, int maxlen);
int socketInit(int argc, char *argv[]);
void wakeCGI(int fd,string url);

int main(int argc, char *argv[]){
	int sockfd;
	chdir(HTMLDIR);
	struct sockaddr_in serv_addr;
	int newsockfd, childpid;
	sockaddr_in cli_addr;
	socklen_t clientLen = sizeof(cli_addr);

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) handle_error("server: can't open stream socket\n");

	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	if(argc == 2) serv_addr.sin_port = htons(atoi(argv[1]));
	else serv_addr.sin_port = htons(PORTNUM);

	int sock_opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&sock_opt, sizeof(sock_opt) ) == -1){
		perror("setsockopt error");
			exit(1);
	}

	if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0) handle_error("server: can't bind local address\n");
	listen(sockfd, 10);

	while(true){

		newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clientLen);

		if(newsockfd < 0)
			handle_error("server: accept error");
		if((childpid = fork()) < 0)
			handle_error("server: fork error");
		else if(childpid == 0){ // child process
			close(sockfd);
			client(newsockfd);
			return 0;
		}
		close(newsockfd);
	}
	return 0;
}

void client(int sockfd){
	int n = 0;
	char line[MAX_CHAR];
	string input;
	stringstream m;
	while(true){
		memset(line, 0, sizeof(char)*MAX_CHAR);
		n = readline(sockfd, line, MAX_CHAR);
		//cout << n << endl;
		if(n == 1 && line[0]=='\r'){
            wakeCGI(sockfd, m.str());
            break;
        }else if(n<=0)
			break;

		input = string(line);
		cout << "input=="<<input << endl;
		//cout << input.substr(0,3) << endl;
		if(input.substr(0,3).compare("GET") == 0){
			size_t pos = input.substr(5).find(" ");
			m << input.substr(5,pos);
			//wakeCGI(sockfd,m.str());
		}else{
			continue;
		}
	}

}
void  parse(char *line, char **argv){
	 while (*line != '\0') {       /* if not the end of line ....... */
		  while (*line == ' ' || *line == '\t' || *line == '\n')
			   *line++ = '\0';     /* replace white spaces with 0    */
		  *argv++ = line;          /* save the argument position     */
		  while (*line != '\0' && *line != ' ' &&
				 *line != '\t' && *line != '\n')
			   line++;             /* skip the argument until ...    */
	 }
	 *argv = '\0';                 /* mark the end of argument list  */
}

void wakeCGI(int fd,string url){
	cout << "helloCGI" << endl;
cout << "parseRequest:" << url << endl;
	size_t pos = url.find("?");
	string file,para,sub;
	if(pos == string::npos){
		para="";
		file=url;
	}else{
		para=url.substr(pos+1);
		file=url.substr(0,pos);
	}
	clearenv();
	setenv("QUERY_STRING", para.c_str(), true);
	setenv("SCRIPT_NAME", file.c_str(), true);
	setenv("CONTENT_LENGTH", "", true);
	setenv("REQUEST_METHOD", "GET", true);
	setenv("REMOTE_HOST", "nplinux1.cs.nctu.edu.tw", true);
	setenv("REMOTE_ADDR", "140.113.235.166", true);
	setenv("ANTH_TYPE", "blah", true);
	setenv("REMOTE_USER", "blah", true);
	setenv("REMOTE_IDENT", "blahblah", true);

	pos=file.find_last_of(".");
	if(pos == string::npos){
		display(fd, "HTTP/1.1 404 Not Found\r\n");
		display(fd, "Server: ubuntu-iim\r\n");
	}
	else
	{
		sub = file.substr(pos+1);
		if(sub.compare("cgi")==0)
		{
			chdir(CGIDIR);
		}
		else if(sub.compare("html")==0 || sub.compare("htm")==0)
		{
			chdir(HTMLDIR);
		}
		if(access( file.c_str(), F_OK )==-1){
			cout << file.c_str() <<" not found" << endl;
			display(fd, "HTTP/1.1 404 Not Found\r\n");
			return;
		}
		display(fd, "HTTP/1.1 200 OK\r\n");
		display(fd, "Server: ubuntu-iim\r\n");
		if(sub.compare("cgi")==0){
			pos=file.find_last_of("/");
			string p=file.substr(pos+1);
			char *a=new char[p.size()+1];
			a[p.size()]=0;
			memcpy(a,p.c_str(),p.size());
			char *arg[20];
			parse(a,arg);

			dup2(fd, 0);
			dup2(fd, 1);
			close(fd);

			if(execvp(file.c_str(),arg)<0)
				perror("exec");
		}else if(sub.compare("html")==0 || sub.compare("htm")==0){
			cout << "return HTML" << endl;
			display(fd,"Content-Type: text/html\r\n\r\n");
			FILE *fp = fopen(file.c_str(), "r");
			while(true){
				char line[MAX_CHAR];
				memset(line, 0, sizeof(char)*MAX_CHAR);
				int n = readline(fileno(fp), line, MAX_CHAR);
				if(n == -1 || n == 0)
					break;
				else{
					display(fd, string(line));
					cout << line << endl;
				}
			}
		}
	}
}

void display(int sockfd,string s){

	char *a=new char[s.size()+1];
	a[s.size()]=0;
	memcpy(a,s.c_str(),s.size());

	if(write(sockfd,a,strlen(a))!=strlen(a)){
		cout << "display write error<br>";
		exit(-1);
	}
}

int readline(int fd, char* ptr, int maxlen){
	int n, rc;
	char c;

	for(n=0; n<maxlen; n++){
		if((rc = read(fd, &c, 1)) == 1){
			*ptr++ = c;
			if(c == '\n' || c == '\0'){
				if(n == 1) return 1;
				break;
			}
		}else if(rc == 0){
			if(n == 0) return (0);
			else break;
		}else return (-1);
	}
	return n;
}
