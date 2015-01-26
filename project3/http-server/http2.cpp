#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <errno.h>
//using std::atoi;
//using std::cout;
//using std::cerr;
//using std::endl;
//using std::string;


#define DEFAULT_WWW_DIR "/home/maydaycha/public_html"
#define DEFAULT_PORT 40000
#define MAX_CHAR 3000
#define GCI_PATH "/home/maydaycha/public_html/cgi-bin"

int readline(int fd, char *ptr, int maxlen);
int sockInit(int argc, char *argv[]);
void err_dump(const char *msg);
void clientRequest(int sockfd);
void response(int sockfd, std::string params);

using namespace std;


int main(int argc, char *argv[])
{
	int msockfd, ssockfd, cpid;
//	socklen_t clientlen;
//	struct sockaddr_in client_addr;
	chdir(DEFAULT_WWW_DIR);

	msockfd = sockInit(argc,argv);
	cout << "http server stat" << endl;

	while(1)
	{
		socklen_t clientlen;
		struct sockaddr_in client_addr;
		clientlen = sizeof(client_addr);

		cerr << "parent still accept......." << endl;
		/* for chrome */
//		while((ssockfd = accept(msockfd, (struct sockaddr*)&client_addr, (socklen_t*)&clientlen))< 0 && errno==EINTR
//			  );
		ssockfd = accept(msockfd, (struct sockaddr*)&client_addr, (socklen_t*)&clientlen);
		if(ssockfd < 0 && errno	!= EINTR) err_dump("accept error");
		cout << ssockfd << " => client connect " << endl;

		/* fork child to serve client */
		if ((cpid = fork()) < 0)
			err_dump("fork error");
		else if(cpid == 0)	 /* child process*/
		{
			close(msockfd);
			clientRequest(ssockfd);
			close(ssockfd);
			exit(0);
		}
		else
			close(ssockfd);
	}
}

void clientRequest(int sockfd)
{
	char line[MAX_CHAR];
	int n;
	bool retriveMethod = true;
	while(1)
	{
		memset(line, 0, sizeof(line));
		n = readline(sockfd,line, MAX_CHAR);
		if(n < 0)
			err_dump("read error");
		else if(n == 0)
		{
			cerr << "client close connection" << endl;
			return;
		}

//		GET /hw3.cgi HTTP/1.1
		string requestInfo(line);
		cerr << requestInfo;

		if(retriveMethod)
		{
			string method = requestInfo.substr(0,3);
			cerr << "method: " << method << endl;
			if(method == "GET")
			{
				cerr << "request GET" << endl;
				string params = requestInfo.substr(4);
//				cerr << ">>" << params << endl;
//				size_t pos = params.find(" ");
//				cerr << "pos ==> " << pos << endl;
//				cerr << params.substr(0,pos) << endl;
				response(sockfd, params);
				cerr << "<<<<>>>> client leave" << endl;
				return;
			}
		}
		retriveMethod = false;
	}
}

void response(int sockfd, string params)
{
	string filepath, query_param, fileType;
	cerr << "par: " << params << endl;
	size_t pos = params.find("?");
	if(pos!=std::string::npos)
		filepath = params.substr(0, pos);
	else
	{
		pos = params.find(" ");
		filepath = params.substr(0, pos);
	}
	cerr << "file path: " << filepath << endl;
	size_t pos2 = params.find(" ",pos) - strlen(filepath.c_str());
//	cerr << "pos1: " << pos << endl;
//	cerr << "pos2: " << pos2 << endl;
	query_param = params.substr(pos+1, pos2);
	cerr << "query string: " << query_param << endl;

	/* ser environment params */
	setenv("REQUEST_METHOD", "GET", 1);
	setenv("QUERY_STRING", query_param.c_str(), 1);
	setenv("SCRIPT_NAME", filepath.c_str(), 1);
	setenv("REMOTE_HOST", "maydaycha.iim.nctu.edu.tw", 1);
	setenv("REMOTE_ADDR", "140.113.72.8", 1);
	setenv("ANTH_TYPE", "", 1);
    setenv("REMOTE_USER", "", 1);
    setenv("REMOTE_IDENT", "", 1);

	size_t pos3 = filepath.find_last_of(".");
	fileType = filepath.substr(pos3);
	cerr << "file type == > "<< filepath.substr(pos3) << endl;

	if(fileType == ".cgi")	/* exec cgi program */
	{
		char status[] = "HTTP/1.1 200 OK\n";
		write(sockfd,status, strlen(status));

		char *cmd[2];
		memset(cmd, 0, sizeof(cmd));

		string cgi_path = GCI_PATH;
		string fullpath;

		cerr << "filepath ==>" << filepath << endl;
		cerr << "queryString ==>" << getenv("QUERY_STRING") << endl;

		fullpath = cgi_path.append(filepath);
		cmd[0] = (char*)malloc(sizeof(char) * fullpath.size()+1);
		strcpy(cmd[0], fullpath.c_str());
		cmd[1] = NULL;

		cerr << "size of null ==>"<< sizeof(NULL) << endl;
		cerr << "cmd[0] ==>" << cmd[0] << endl;

		dup2(sockfd, STDIN_FILENO);
		dup2(sockfd, STDOUT_FILENO);
		close(sockfd);
		if(execvp(fullpath.c_str(), cmd) < 0)
			err_dump("cgi execute fail");
	}
    else if(fileType == "")
	else /* return a page*/
	{
		/* /Users/Maydaycha/NP_project/Project3 */
		cerr << "@@file path ==>" << filepath << endl;

		string wwwDirectory = "/home/maydaycha/public_html";
		string fullpath = wwwDirectory.append(filepath);
		cerr << " full path for www ==>"<< fullpath << endl;

		FILE *file = fopen(fullpath.c_str(), "r");
		cerr << file << endl;

		if(file)
		{
			cerr << "open file success" << endl;
			char status[] = "HTTP/1.1 200 OK\n";
			write(sockfd,status, strlen(status));
			string headers;
			/* html page */
			if(fileType == ".htm" || fileType == ".html")
				headers = "Content-Type: text/html\n\n";
			else
				headers = "Content-Type: text/plain\n\n";

				write(sockfd,headers.c_str(), headers.size());

				while(true)
				{
					char line[MAX_CHAR];
					memset(line, 0, sizeof(line));

					int n = readline(fileno(file), line, MAX_CHAR);
					if(n == 0 || n < 0)
					{
						cerr << "no more to read" << endl;
						fclose(file);
						return;
					}
					else
					{
//						cerr << "reading ......." << endl;
						write(sockfd, line, strlen(line));
					}
				}
		}
		else
			cerr << "open file fail" << endl;
	}
}


/* handle socket*/
int sockInit(int argc, char *argv[])
{
	int sockfd;
	sockaddr_in serv_addr;
//	serv_addr = (struct sockaddr_in)malloc(sizeof(struct sockaddr_in));
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		err_dump("open TCP socket error");

	bzero((char*)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (argc > 1)
		serv_addr.sin_port = htons(atoi(argv[1]));
	else
		serv_addr.sin_port = htons(DEFAULT_PORT);

	/* Tell the kernel that you are willing to re-use the port anyway */
    int yes=1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
		err_dump("setsockpt");
    /*******************************************************************/


	if(bind(sockfd,(struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        err_dump("server: cant bind local address");
	if(listen(sockfd, 10) < 0)
		err_dump("listen error");

	return sockfd;
}



int readline(int fd, char *ptr, int maxlen)
{
    int n, rc;
    char c;
    for(n = 0; n<maxlen; n++){
        rc = read(fd, &c, 1);
        if(rc==1){
            *ptr++=c;
            if(c=='\n') break;
        }
        else if(rc ==0){
            if(n==0) return 0;   /*END of File*/
            else break;
        }
        else return -1;
    }
    *ptr = 0;
    return n;
}

void err_dump(const char *msg)
{
	perror(msg);
	fprintf(stderr, "%s, errno = %d\n", msg, errno);
	exit(1);
}



