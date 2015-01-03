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
#include <string>
#include <string.h>
// #include <cstring>
#include <sstream>
#include <unistd.h>
#include <vector>
using namespace std;



#define SERV_TCP_PORT 3334
#define MAX_CHAR 1024
#define HOSTNAME "nplinux2.cs.nctu.edu.tw"
#define HOSTADDR "140.113.235.167"
#define pwd "/net/other/2015_1/c0353416/public_html/"
#define MAXLINE 10100

int set_msockfd(int argc, char *argv[]);
void err_dump(const char *msg);
void HTTPrequest (int sockfd);
int cut_line (int fd,char line[MAXLINE]);
int parseRequest (int fd, string input);

int readline(int fd, char * ptr, int maxlen );



int main(int argc, char *argv[])
{
  int msockfd, ssockfd ,childpid;
  int clilen;
  int client_pid;
  struct sockaddr_in cli_addr;
  msockfd =set_msockfd(argc,argv);
  cerr << "msockfd: " << msockfd << endl;
  cerr << "port: " << SERV_TCP_PORT<< endl;

// cerr << "main:" << endl;
  for ( ; ; )
  {
    // fprintf(stderr, "---------FOR ---------\n");
    // cerr << "inf for  "<< endl;
    clilen = sizeof(cli_addr);
    ssockfd = accept(msockfd, (struct sockaddr *) &cli_addr, (socklen_t *)&clilen);
 // cerr << "ssockfd: " << ssockfd << endl;
     if (ssockfd < 0)
         err_dump("server: accept error");
     if ( (childpid = fork()) < 0)
         err_dump("server: fork error");
    else if (childpid == 0)
    {
      // cerr << "ssockfd: " << ssockfd << endl;
      close(msockfd);

      HTTPrequest(ssockfd);

      close(ssockfd);
      exit(0);
    }

    close(ssockfd); /* parent process */

  }

}

int set_msockfd(int argc, char *argv[])
{
  int sockfd;
  sockaddr_in  serv_addr;
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    err_dump("server: can't open stream socket");
  }
  bzero( (char *) &serv_addr, sizeof(serv_addr) );//clear

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(SERV_TCP_PORT);
  int yes=1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
  {
      perror("setsockopt");
      exit(1);
  }


  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    err_dump("server: can't bind local address");
if(listen(sockfd, 10) < 0)
    err_dump("listen error");

  return sockfd;

}

void HTTPrequest (int sockfd)
{
  char line[MAXLINE];

  int n ;
  string a;
  bool first =true;
  while(1)
  {
      memset(line, 0, sizeof(char)*MAX_CHAR);
      n = readline(sockfd, line, MAX_CHAR);
      string input = string(line);
      if (n == 0) return; /* connection terminated */
      else if (n < 0)
      {
         err_dump("str_echo: readline error");
      }

      // first
      if(first)
      {
        size_t pos = input.find("GET");
        if (pos != std::string::npos )//found
        {
          size_t pos2 = input.substr(4).find(" ");
          a = input.substr(5,pos2-1);
          // m << input.substr(5,pos2);
        }
         cout << "input ===="<<input << endl;
         parseRequest(sockfd,a);
         first=false;
     }
  }

}



int parseRequest (int fd, string input)
{
  cerr << "parseRequest:" << input << endl;
  // size_t pos,pos_Q ;
  string filepath ;
  string cgipath,type ;
//   pos = input.find("?") ;//http://java.csie/test.cgi?a=aa&b=bb HTTP/1.
//   if(pos!= string::npos)
//     cgipath = input.substr( 0, pos-1 ) ;//http://java.csie/test.cgi

//   size_t path_len;
//   path_len = cgipath.length();// size_t posend ＝ path_len + 1 ;
//   pos_Q = input.find(" ") ;
//   if(pos_Q!= string::npos)
//     filepath = input.substr(pos_Q+1 , input.length()) ;
//   size_t pos2 = filepath.find(" ") - filepath.lenght();
// cerr << "filepath:" << filepath << endl;

  size_t pos = input.find("?");
  string file,Q_str,sub;
  if(pos == string::npos){
    Q_str="";
    file=input;
  }
  else
  {
    Q_str=input.substr(pos+1);
    file=input.substr(0,pos);
  }
  cout << "QUERY_STRING ="<<Q_str<<"="<< endl;
  cout << "SCRIPT_NAME ="<<file<<"="<< endl;

  setenv("REQUEST_METHOD", "GET", 1);
  setenv("QUERY_STRING", Q_str.c_str(), 1);
  setenv("CONTENT_LENGTH", "", true);
  setenv("SCRIPT_NAME", file.c_str(), 1);
  setenv("REMOTE_HOST", HOSTNAME, 1);
  setenv("REMOTE_ADDR", HOSTADDR, 1);
  setenv("ANTH_TYPE", "", 1);
  setenv("REMOTE_USER", "", 1);
  setenv("REMOTE_IDENT", "", 1);

  pos = file.find_last_of(".");
  type = file.substr( pos + 1 );
 cout << "type ="<<type<< endl;

  if(access( file.c_str(), F_OK )==-1)
  {
      cout << file.c_str() <<" not found" << endl;
      char outt [] ="HTTP/1.1 404 Not Found\r\n";
      write(fd,outt, strlen(outt));
      return 0;
  }
  char outt [] ="HTTP/1.1 200 OK\r\n";
    write(fd,outt, strlen(outt));
  if(type.compare("cgi")==0)
  {


    char *exec_cgi[2];
    memset(exec_cgi, 0, sizeof(exec_cgi));

    string dir;
    dir=file.insert(0,pwd);
    exec_cgi[0] = (char*)malloc(sizeof(char) * dir.size()+1);
    strcpy(exec_cgi[0],dir.c_str());
    // cout<< "exec_cgi[0] =="<<exec_cgi[0] <<endl;
    exec_cgi[1] =NULL;

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    close(fd);
    if(execvp(dir.c_str(), exec_cgi) < 0)
      err_dump("cgi execute fail");



  }
  else if(type.compare("html")==0 )
  {
    char outt2 [] ="Content-Type: text/html\r\n\r\n";
    write(fd,outt2, strlen(outt2));
    FILE *filefd = fopen (file.c_str(), "r");
    while(1)
    {
      char line [MAXLINE];
      memset(line, 0, sizeof(char)*MAX_CHAR);
      int n;
      n=readline(fileno(filefd), line, MAX_CHAR);
      string input = string(line);

      if (n <= 0) break;/* connection terminated */
      else
      {

         write(fd,line, strlen(line));
          cout << "line ="<<input<< endl;
      }
    }//while(1)
    cout << "********** end read tag "<< endl;
    fclose(filefd);

  }
// return 0 ;

}


void err_dump(const char *msg)
{
  perror(msg);
  fprintf(stderr, "%s, errno = %d\n", msg, errno);
  exit(1);
}


int readline(int fd, char * ptr, int maxlen)
{
        // fprintf (stderr,"--rrrrrrrrrrrrr\n");
  int n, rc;
  char c;
  for (n = 0; n < maxlen; n++)
  {
    if ( (rc = read(fd, &c, 1)) == 1)
    {
      *ptr++ = c;
      if (c == '\n') break;
    }
    else if (rc == 0)
    {
    if (n == 0) return(0); /* EOF, no data read */
    else break; /* EOF, some data was read */
    }
    else
      return(-1); /* error */
  }
  *ptr = 0;
  return(n);
}
