// #include <algorithm>
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
// #include <vector>
using namespace std;

#define CGIDIR "/home/c0353416/public_html/cgi_bin"
#define F_CONNECTING 0
#define F_WRITING 1
#define F_READING 2
#define F_DISCONNECT 4
#define F_DONE 3
#define MAX_HOST 5
#define MAX_CHAR 10240
// #define handle_error(msg) \
//   do { cout << "error: " << msg << "<br>\n"; fflush(stdout); exit(EXIT_FAILURE); } while (0)

struct Server{
  int fd;
  int port;
  int status;

  bool leave;
  sockaddr_in addr,proxy;
  string batchfile;
  char* IP;
  char* file;
  bool connect;
  string S_IP;
  int S_PORT;
  // bool leave;
  FILE *filefd;
} host[MAX_HOST] = {0};

int alive=0;
// string QUERY_STRING;
fd_set rfds,rs,wwfd,wfds;

char line[MAX_CHAR];

void splitStr(char *input, char *splitdelim);
void err_dump(const char *msg);
void printForm();
// void checkForm();
void connectToServer();
int readline(int fd,char *ptr,int maxlen);
int readlinefile(int fd,char *ptr,int maxlen);
void showOnhtml(int id, char*input);
int parse(char *input[50], char *splitdelim) ;//&

int main(int argc, char *argv[])
{

  printf("Content-Type: text/html\r\n\r\n");
   // printf(" test= <br>\n");
  char *QUERY_CGI = getenv("QUERY_STRING");
  // char *QUERY_CGI;
  // char QUERY_CGI[] = "h1=140.113.235.166&p1=5533&f1=t5.txt&h2=140.113.235.166&p2=5533&f2=t5.txt&h3=140.113.235.166&p3=5533&f3=t5.txt&h4=140.113.235.166&p4=5533&f4=t5.txt&h5=140.113.235.166&p5=5533&f5=t5.txt";
  int host_num = 5;



 // strcpy(QUERY_CGI,"h1=140.113.235.166&p1=5533&f1=t1.txt&h2=140.113.235.166&p2=5533&f2=t1.txt&h3=140.113.235.166&p3=5533&f3=&h4=140.113.235.166&p4=5533&f4=&h5=140.113.235.166&p5=5544&f5=t1.txt");
 // strcpy(QUERY_CGI,"h1=140.113&p1=5554&f1=t1.txt&h2=nplinux1.cs.nctu.edu.tw&p2=5554&f2=t1.txt&h3=nplinux1.cs.nctu.edu.tw&p3=5554&f3=t1.txt&h4=140.113.72.2&p4=33&f4=t1.txt&h5=140.113.72.8&p5=33&f5=t3.txt");
 // strcpy(QUERY_CGI,"h1=nplinux1.cs.nctu.edu.tw&p1=5554&f1=t1.txt&h2=nplinux1.cs.nctu.edu.tw&p2=5554&f2=t1.txt&h3=nplinux1.cs.nctu.edu.tw&p3=5554&f3=t1.txt&h4=nplinux1.cs.nctu.edu.tw&p4=5554&f4=t1.txt&h5=nplinux1.cs.nctu.edu.tw&p5=5554&f5=t1.txt");
  splitStr(QUERY_CGI,(char*)"&");

  // printf(" begin printForm<br>\n");
  printForm();

  // checkForm();

  int nfds = getdtablesize();
  FD_ZERO(&rs);
  FD_ZERO(&wwfd);
  FD_ZERO(&rfds);
  FD_ZERO(&wfds);

  socklen_t socketN = sizeof(int);

  // printf(" printForm2 <br>\n");

  // printf(" begin connectToServer\n");
  connectToServer();

  // printf(" printForm22<br>\n");
  // printf(" end connectToServer\n");
  int error = 0;

  while(host_num >0 )
  {


    memcpy(&rfds, &rs, sizeof(rfds));
    memcpy(&wfds, &wwfd, sizeof(wfds));
    // cout << "before select <br>\n" << endl;
    if(select(nfds, &rfds, &wfds, (fd_set*)0, (struct timeval*)0) < 0)
      err_dump("select error");
    // cout << "after select <br>\n" << endl;
    for(int i=0; i<MAX_HOST; i++)
    {
      // printf(" host_num=%d, i=%d <br>\n",host_num,i);
      if(host[i].connect)
      {

        if( host[i].status == F_CONNECTING && (FD_ISSET(host[i].fd, &rfds) || FD_ISSET(host[i].fd, &wfds)))
        {

          if(getsockopt(host[i].fd, SOL_SOCKET, SO_ERROR, &error, &socketN) < 0 || error != 0)
          {
            // printf(" getsockopt error \n");
            err_dump("non-blocking connect failed\n");
          }

          host[i].status = F_READING;
          FD_CLR(host[i].fd, &wwfd);


          // printf(" status== %d<br>\n",host[i].status);
        }
        else if(host[i].status == F_WRITING && FD_ISSET(host[i].fd,&wfds))/** wrtie to server,read from file   */
        {
          // printf(" \n -----F_WRITING <br>\n");
          memset ( line, 0, sizeof(char)*MAX_CHAR ) ;
          // filefd
          if(readlinefile(fileno(host[i].filefd), line, MAX_CHAR) < 0)
          {
printf(" \n read file error <br>\n");

            err_dump("181, read error");
          }
          if(write(host[i].fd, line, strlen(line)) < 0)
          {


            printf(" \n write to server  error <br>\n");
             err_dump("185, write error");
          }

          showOnhtml(i,line);
          fflush(stdout);
          // fflush(stdout);
          // if(host[i].status)
          if(strncmp(line, "exit", 4) == 0) host[i].leave=true;


          host[i].status = F_READING;
          FD_SET(host[i].fd, &rs);
          FD_CLR(host[i].fd, &wwfd);
// printf(" \n wrtie to serverr  <br>\n");


        }
        else if(host[i].status == F_READING && FD_ISSET(host[i].fd, &rfds) ) /** read from server */
        {
          // printf(" \n -----F_READING <br>\n");
          memset(line, 0, sizeof(char)*MAX_CHAR);
          int n = readline(host[i].fd,line,MAX_CHAR);
          // printf(" \n  n=%d <br>\n",n);
          // printf(" F_READING, n=%d i= %d  msg::%s <br>\n",n,i,line);
          // printf("  msg::%s \n", line);
          // printf(" fd= %d \n",host[i].fd);
         // printf(" i= %d\n", i);
          if (n <= 0) /** read finished F */
          {
             if(host[i].leave)
            {
              host[i].connect=false;
              host_num--;
              fclose(host[i].filefd);


            }
            // printf(" n < 0  <br>\n");
            FD_SET(host[i].fd, &wwfd);

            FD_CLR(host[i].fd, &rs);
            if(n==0) host[i].status = F_WRITING ;

          }
          else if (n>0)
          {

            showOnhtml(i,line);
            fflush(stdout);
            // fflush(stdout);
            // printf(" n <= 0  <br>\n");
            if(strncmp(line, "% ", 2) == 0)/** receive %cmd*/
            {
              // printf(" find ~paa \n");
              host[i].status = F_WRITING ;
              FD_SET(host[i].fd, &wwfd) ;
              FD_CLR(host[i].fd, &rs) ;
            }


          }


          if(strncmp(line, "exit", 4) == 0) host[i].connect = false;


        }//else if status ==F_READ
//         else if (host[i].status == F_DISCONNECT)
//         {
//             FD_CLR(host[i].fd, &wwfd) ;
//             FD_CLR(host[i].fd, &rs) ;
// printf(" \n F_DISCONNECT <br>\n");
//             // host[i].status = F_DONE ;
//             host[i].connect = false ;
//             fclose(host[i].filefd);
//             close(host[i].fd);
//             host_num -- ;
//         }
      }//if(host.connect)

      // else
      // {

      //     FD_CLR(host[i].fd, &wwfd) ;
      //     FD_CLR(host[i].fd, &rs) ;
      //   printf(" \n F_DISCONNECT <br>\n");
      //       // host[i].status = F_DONE ;
      //       // host[i].connect = false ;
      //       fclose(host[i].filefd);
      //       close(host[i].fd);
      //       host_num -- ;
      // }
    }//for (MAX host)




  }// while(host_num >0)
  printf("</font>\n");
  printf("</body>");
  printf("</html>");
     return 0;

}
string replacetTag(string in, string re ,string ch)
{
  size_t found = in.find(re);
  if(found!=std::string::npos)
    in.replace(found,re.length(),ch);

  return in;

}


void convertTag(string input, int id  )
{
    string result = input;
    result = replacetTag(result,"\n","");
    result = replacetTag(result,"\r","");
    result = replacetTag(result,">","&gt");
    result = replacetTag(result,"<","&lt");
    result = replacetTag(result,"\"","&quot");


    cout << "<script>document.all['m" << id << "'].innerHTML+= \"" << result << "<br>\";</script>";


}


void showOnhtml(int id, char*input)
{
  ostringstream out;
  // char result[MAX_CHAR];
  string result(input);
  convertTag(result,id);

  usleep(20);
  fflush(stdout);

}
void checkForm()
{
  for(int i=0; i<MAX_HOST; i++)
  {
    if((strcmp(host[i].IP, "")!=0) && host[i].port && (strcmp(host[i].file, "")!=0))
    {
      host[i].connect = true;
    }
  }
}
void connectToServer()
{
  // printf("MAX_HOST= %d <br>\n",MAX_HOST);

  for(int i=0; i< MAX_HOST; i++)
  {

      if((host[i].fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        err_dump("open TCP socket error");
      // strcpy(host[i].IP,"nplinux1.cs.nctu.edu.tw");
      // strcpy(host[i].IP,"140.113.235.166");
      // handle address and port
      bzero((char*)&host[i].addr, sizeof(host[i].addr));
      host[i].addr.sin_family = AF_INET;
      // printf("------------ %d in the  connectToServerr   <br>\n",i);
      // printf("IP ==%s=<br>\n",host[i].IP);
      // printf("fd ==%d=<br>\n",host[i].fd);
      // printf("port ==%d=<br>\n",host[i].port);
      host[i].addr.sin_addr.s_addr = inet_addr(host[i].IP);
      // printf(" %d in the  connectToServerr   <br>\n",i);
      // inet_aton(host[i].IP,&host[i].addr.sin_addr);
      host[i].addr.sin_port = htons(host[i].port);
      // printf(" connectToServerr22222 <br>\n");
      // printf(" connectToServerr end <br>\n");
      // non-blocking connect to server
      int flag = fcntl(host[i].fd, F_GETFL, 0);
      // printf(" %d in the  connectToServerr   <br>\n",i);
      if(flag < 0)
        err_dump("get flag of fd fail");
      fcntl(host[i].fd, F_SETFL, flag | O_NONBLOCK);

      // printf(" %d in the  connectToServerr 22  <br>\n",i);
      int n = connect(host[i].fd, (struct sockaddr*)&host[i].addr, sizeof(host[i].addr));
      // printf("i= %d , conn=%d in the  connectToServerr yo   <br>\n",i, n);

      if(n < 0) if(errno != EINPROGRESS)  err_dump("connect error");




      host[i].status = F_CONNECTING;

      // clientAmout++;
      FD_SET(host[i].fd, &rs);
      FD_SET(host[i].fd, &wwfd);
      //      cout << i << "==> is set? " << FD_ISSET(host[i].Fd[i], &ws) << endl;
      // rfds = rs;
      // wfds = wwfd;

// printf(" connectToServerr end<br>\n");


  }
// printf(" connectToServerr end  <br>\n");


}
void printForm()
{
  printf("<html>\n");
  printf("<head>\n");
  printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\n");
  // printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n");
  printf("<title>Network Programming Homework 3</title>\n");
  printf("</head>\n");
  printf("<body bgcolor=#336699>\n");
  printf("<font face=\"Courier New\" size=2 color=#FFFF99>\n");
  printf("<table width=\"800\" border=\"1\">\n");
  printf("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr><tr>\n",host[0].IP,host[1].IP,host[2].IP,host[3].IP,host[4].IP);
  printf("<td valign=\"top\" id=\"m0\"></td><td valign=\"top\" id=\"m1\"></td><td valign=\"top\" id=\"m2\"></td><td valign=\"top\" id=\"m3\"></td><td valign=\"top\" id=\"m4\"></td></tr>\n</table>\n");
// printf(" end printForm");

// printf("------------\n");
// printf(" end printForm\n");

}


void err_dump(const char *msg)
{
  perror(msg);
  fprintf(stderr, "%s, errno = %d\n", msg, errno);
  exit(1);
}

int readlinefile(int fd,char *ptr,int maxlen)
{
  int n,rc;
  char c;
  *ptr = 0;
  for(n=0; n<maxlen; n++)
  {
    if((rc=read(fd,&c,1)) == 1)
    {
      *ptr++ = c;
      // if(c==' '&& *(ptr-2) =='%'){  break; }
      if(c=='\n')  break;
    }
    else if(rc==0)
    {
      if(n==0)     return(0);
      else         break;
    }
    else
      return(-1);
  }
  return(n);
}


int readline(int fd,char *ptr,int maxlen)
{
  int n,rc;
  char c;
  *ptr = 0;
  for(n=0; n<maxlen; n++)
  {
    if((rc=read(fd,&c,1)) == 1)
    {
      *ptr++ = c;
      // if(c==' '&& *(ptr-2) =='%'){  break; }
      if(c=='\n')  break;
    }
    else if(rc==0)
    {
      if(n==0)     return(0);
      else         break;
    }
    else
      break;
  }
  return(n);
}


void splitStr(char *input,  char *splitdelim)
{

  for (int i=0;i<MAX_HOST;i++)
  {
    if(i==0)
      host[0].IP = strtok(input, splitdelim) +3;
    else
    {
      host[i].IP = strtok(NULL, splitdelim) +3;
    }
    printf("IP == %s <br>\n",host[i].IP);

    char *temp =strtok(NULL, splitdelim) +3;
    host[i].port = atoi ( temp );
    printf("port = %d<br>\n",host[i].port);

    host[i].file = strtok(NULL, splitdelim) +3;
    host[i].filefd = fopen(host[i].file, "r");
    host[i].connect = true;
    printf("file = %s <br>\n",host[i].file);
  }

}

// int recv_msg(int from)
// {
//   char buf[3000];
//   int len;
//   if((len=readline(from,buf,sizeof(buf)-1)) <0) return -1;
//   buf[len] = 0;
//   printf("%s",buf); //echo input
//   fflush(stdout);
//   return len;
// }
