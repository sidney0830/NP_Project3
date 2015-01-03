#include <windows.h>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <errno.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>
#include <fstream>

using namespace std;

#include "resource.h"

#define SERVER_PORT 7799

#define WM_SOCKET_NOTIFY (WM_USER + 1)
#define CGI_CONNECT_NOTIFY (WM_SOCKET_NOTIFY + 1)

#define MAX_CLIENT 5
#define MAX_CHAR 200
#define MAX_WORD 3000
#define DISCONNECT -1
#define CONNECT 0
#define RECEIVE 1
#define SEND 2
#define LOGOUT 3


struct ClientToServ{
  char Ip[MAX_CLIENT][MAX_CHAR];
  char Port[MAX_CLIENT][MAX_CHAR];
  char File[MAX_CLIENT][MAX_CHAR];
  int Fd[MAX_CLIENT];
  ifstream InputFd[MAX_CLIENT];
  bool NeedConnect[MAX_CLIENT];
  sockaddr_in addr[MAX_CLIENT];
  int STATUS[MAX_CLIENT];
  bool Logout[MAX_CLIENT];
};
ClientToServ c;

BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf (HWND, TCHAR *, ...);
int readline(int fd, char *ptr, int maxlen);
void response(int sockfd, string params, HWND hwnd);
bool strcompare(string s1, string s2);
void callCGI(int sockfd, string query_string, HWND hwnd);
void clientDisplay(int sockfd, string s);
int sendToServer(int sockfd, string s);
void initialUserInfo();
int splitStr2(char *input, char** output, char *splitdelim);
int findSocketIdex(int sockfd);
int readline2(int fd, char *ptr, int maxlen);
string addToHtml(int userid, char *msg, bool n_To_Br);
void convertTag(const char *input, char *output, size_t len);
int readlineFromFile(FILE *fp, char*line);
void initialAfterLeave(int i);
//=================================================================
//  Global Variables
//=================================================================
list<SOCKET> Socks;
static HWND hwndEdit;
int connectAmount;
static SOCKET outputSockfd;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
  LPSTR lpCmdLine, int nCmdShow)
{

  return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
  WSADATA wsaData;

  //static HWND hwndEdit;
  static SOCKET msock, ssock;
  static struct sockaddr_in sa;

  int err;
  string params;
  int sockindex;


  switch(Message)
  {
  case WM_INITDIALOG:
    hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
    break;
  case WM_COMMAND:  //用戶選擇一條菜單命令項或某控件發送一條通知消息給其父窗,或某快捷鍵被翻譯時,本消息被發送
    switch(LOWORD(wParam))
    {
    case ID_LISTEN:

      WSAStartup(MAKEWORD(2, 0), &wsaData);

      //create master socket
      msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

      if( msock == INVALID_SOCKET ) {
        EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
        WSACleanup();
        return TRUE;
      }

      err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

      if ( err == SOCKET_ERROR ) {
        EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
        EditPrintf(hwndEdit, TEXT("=== 112----close sock %d\r\n"), msock);
        closesocket(msock);
        WSACleanup();
        return TRUE;
      }

      //fill the address info about server
      sa.sin_family   = AF_INET;
      sa.sin_port     = htons(SERVER_PORT);
      sa.sin_addr.s_addr  = INADDR_ANY;

      //bind socket
      err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

      if( err == SOCKET_ERROR ) {
        EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
        WSACleanup();
        return FALSE;
      }

      err = listen(msock, 2);

      if( err == SOCKET_ERROR ) {
        EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
        WSACleanup();
        return FALSE;
      }
      else {
        EditPrintf(hwndEdit, TEXT("=== Server START ===\r\n"));
      }

      break;
    case ID_EXIT:
      EndDialog(hwnd, 0);
      break;
    };
    break;

  case WM_CLOSE: //"用戶關閉窗口時會發送本消息,緊接著會發送WM_DESTROY消息"
    EndDialog(hwnd, 0);
    break;

  case WM_SOCKET_NOTIFY:  // "已在MSDN中公開的MFC內部消息,本消息告訴socket窗口socket事件已經發生(socket窗口:CSocketWnd,隱藏,接收本消息,響應:OnSocketNotify)),"
    ssock = wParam;
    switch( WSAGETSELECTEVENT(lParam) )
    {
    case FD_ACCEPT:
      ssock = accept(msock, NULL, NULL);
      Socks.push_back(ssock);
      EditPrintf(hwndEdit, TEXT("=== Accept one new client(%d), List size:%d ===\r\n"), ssock, Socks.size());
      break;
    case FD_READ:
      //Write your code for read event here.
      while(true)
      {
        char line[MAX_WORD];
        memset(line, 0, sizeof(line));
        int n = readline(ssock, line, MAX_WORD);
        if(n < 0) // no more to read
        {
          /*when receive exit then close socket*/
          //EditPrintf(hwndEdit, TEXT("=== 172----close sock %d\r\n"), ssock);
          //closesocket(ssock);
          //response(ssock, params, hwnd);
          break;
        }
        else if(n == 0)
        {
          EditPrintf(hwndEdit, TEXT("=== client(%d), close connection\r\n"), ssock);
          break;
        }
        //GET /hw3.cgi HTTP/1.1
        EditPrintf(hwndEdit, TEXT("=== from client(%d), meg:%s\r\n"), ssock,line);

        string requestInfo(line);
        EditPrintf(hwndEdit, TEXT("=== 150 from client(%d), meg:%s\r\n"), ssock,requestInfo.c_str());

        string method = requestInfo.substr(0,3);
        EditPrintf(hwndEdit, TEXT("=== from client(%d), method ====> %s\r\n"), ssock,method.c_str());

        if(strcmp(method.c_str(),"GET") == 0)
        {
          EditPrintf(hwndEdit, TEXT("=== from client(%d), request GET\r\n"), ssock);
          params = requestInfo.substr(4);
          EditPrintf(hwndEdit, TEXT("=== from client(%d), params ==> %s\r\n"), ssock, params.c_str());
          response(ssock, params, hwnd);
        }
      }
      break;
    case FD_WRITE:
      //Write your code for write event here

      break;
    case FD_CLOSE:
      break;
    };
    break;

  case CGI_CONNECT_NOTIFY:
    ssock = wParam;
    sockindex = findSocketIdex(ssock);
    //  EditPrintf(hwndEdit, TEXT("===  webPage socket ===> %d\r\n"), outputSockfd);
  //  EditPrintf(hwndEdit, TEXT("===  sockfd is selected ===> %d\r\n"), sockindex);
    switch( WSAGETSELECTEVENT(lParam) )
    {
    case FD_CONNECT:
      EditPrintf(hwndEdit, TEXT("=== %d, CGI connect successful\r\n"), sockindex);
      break;
    case FD_CLOSE:
      if(sockindex != -1)
      {
        WSAAsyncSelect(c.Fd[sockindex],hwnd,0,0);
        connectAmount--;
        EditPrintf(hwndEdit, TEXT("=== %d, connect close (unregular)\r\n"), sockindex);
        if(connectAmount == 0)
        {
          EditPrintf(hwndEdit, TEXT("=== %d, all client leave (notify by fd_close)\r\n"), sockindex);
          closesocket(outputSockfd);
          Socks.remove(outputSockfd);
        }
      }
      EditPrintf(hwndEdit, TEXT("=== %d, CGI connect close\r\n"), sockindex);
      break;
    case FD_READ:
      //EditPrintf(hwndEdit, TEXT("=== %d, CGI readable\r\n"), sockindex);
      if(c.STATUS[sockindex] == RECEIVE)
      {
        while(true)
        {
          char line[MAX_WORD];
          memset(line, 0, sizeof(line));
          int n = readline2(ssock, line, MAX_WORD);
          if(n <= 0) // no more to read
          {
            if(c.Logout[sockindex]==true)
            {
              EditPrintf(hwndEdit, TEXT("=== 230----close sock %d\r\n\r\n"), c.Fd[sockindex]);
              c.STATUS[sockindex] = DISCONNECT;
              connectAmount--;
              EditPrintf(hwndEdit, TEXT("===----connectAmount ===> %d\r\n"), connectAmount);
              c.InputFd[sockindex].close();
              WSAAsyncSelect(c.Fd[sockindex], hwnd, 0, 0);
              closesocket(c.Fd[sockindex]);
              initialAfterLeave(sockindex);

              if(connectAmount==0)
              {
                EditPrintf(hwndEdit, TEXT("=== All connect leave %d\r\n"));
                closesocket(outputSockfd);
                Socks.remove(outputSockfd);
                //WSACleanup();
              }
            }
            break;
          }
          else if(n > 0)
          {
            if(strncmp(line,"% ",2)==0)
            {
              EditPrintf(hwndEdit, TEXT("=====Client(%d) Status is SEND =======\r\n"), sockindex);
              clientDisplay(outputSockfd, addToHtml(sockindex,line,false));

              EditPrintf(hwndEdit, TEXT("=====Client(%d) Ready to Send =======\r\n"), sockindex);

              char line[MAX_WORD];
              memset(line, 0, sizeof(line));

              if(c.InputFd[sockindex].getline(line,MAX_WORD))
              {
                clientDisplay(outputSockfd,addToHtml(sockindex,line,true));
                line[strlen(line)] = '\n';
                sendToServer(c.Fd[sockindex], line);
                if(strcompare(line, "exit\r\n") || strcompare(line, "exit\n") || strcompare(line, "exit"))
                {
                  c.Logout[sockindex] = true;
                  EditPrintf(hwndEdit, TEXT("=== Client(%d), Logout \r\n\r\n"), sockindex);
                }
              }
              else
              {
                EditPrintf(hwndEdit, TEXT("=== Client(%d), Read FIle error \r\n"), sockindex);
              }
            }
            else
              clientDisplay(outputSockfd, addToHtml(sockindex,line,true));
          }
        }
      }
      break;
    case FD_WRITE:
      //EditPrintf(hwndEdit, TEXT("=== %d, CGI writeable\r\n"), sockindex);
      break;
    }
    break;
  default:
    return FALSE;
  };
  return TRUE;
}

void response(int sockfd, string params, HWND hwnd)
{
  string filetype, query_params;
  string filepath;
  EditPrintf(hwndEdit, TEXT("===response, client(%d), param : %s \r\n"), sockfd,params.c_str());

  size_t pos = params.find("?");
  if(pos!=std::string::npos)
    filepath = params.substr(0,pos);
  else
  {
    pos = params.find(" ");
    filepath = params.substr(0,pos);
  }
  EditPrintf(hwndEdit, TEXT("===response, client(%d), filepath : %s \r\n"), sockfd,filepath.c_str());

  size_t pos2 = params.find(" ", pos) - strlen(filepath.c_str());
  query_params = params.substr(pos+1, pos2);
  EditPrintf(hwndEdit, TEXT("===response, client(%d), query parms : %s \r\n"), sockfd,query_params.c_str());

  size_t pos3 = filepath.find_last_of(".");
  filetype = filepath.substr(pos3+1);
  EditPrintf(hwndEdit, TEXT("===response, client(%d), file type : %s \r\n"), sockfd,filetype.c_str());

  if(strcompare(filetype,"cgi"))
  {
    EditPrintf(hwndEdit, TEXT("===response, client(%d), Call GCI \r\n"), sockfd);

    callCGI(sockfd, query_params,hwnd);


  }
  else
  {

    if(strcompare(filetype, "htm") || strcompare(filetype, "html"))
    {
      EditPrintf(hwndEdit, TEXT("===response, client(%d), Call HTM or HTML \r\n"), sockfd);
      EditPrintf(hwndEdit, TEXT("===filepath : %s \r\n"), filepath.c_str());

      filepath = filepath.substr(1);
      ifstream f2;
      f2.open(filepath.c_str());
      if(f2.is_open())
        EditPrintf(hwndEdit, TEXT("===file open.....  \r\n"));
      else
        EditPrintf(hwndEdit, TEXT("===file not open.....  \r\n"));
      clientDisplay(sockfd, "HTTP/1.1 200 OK\n");
      clientDisplay(sockfd, "Content-Type: text/html\n\n");

      char buffer[MAX_WORD];
      memset(buffer,0,sizeof(buffer));
      while(f2.getline(buffer,MAX_WORD))
      {
        EditPrintf(hwndEdit, TEXT("===reading.....  \r\n"));
        clientDisplay(sockfd,buffer);
        memset(buffer,0,sizeof(buffer));
      }

      f2.close();
      //closesocket(sockfd);
      return;

    }
  }


}

void callCGI(int sockfd, string query_string, HWND hwnd)
{
  int arg_count = 0;
  char *arg[50];
  EditPrintf(hwndEdit, TEXT("===CGI, client(%d), query_string ==> %s \r\n"), sockfd, query_string.c_str());

  clientDisplay(sockfd, "HTTP/1.1 200 OK\n");
  clientDisplay(sockfd, "Content-Type: text/html\n\n");

  initialUserInfo();

  outputSockfd = sockfd;
  arg_count = splitStr2((char*)query_string.c_str(), arg, (char*)"&");
  EditPrintf(hwndEdit, TEXT("===CGI, client(%d), argument count ==> %d \r\n"), sockfd, arg_count);
  EditPrintf(hwndEdit, TEXT("===CGI, client(%d), arg ==> %s \r\n"), sockfd, arg[0]);

  char *pair[15][2];
  int num=0;
  for(int i=0,j=0; i< arg_count; i+=3,j++)
  {
    int count1 = 0, count2 = 0, count3 = 0;
    count1 = splitStr2(arg[i], pair[i], (char*)"=");
    count2 = splitStr2(arg[i+1], pair[i+1], (char*)"=");
    count3 = splitStr2(arg[i+2], pair[i+2], (char*)"=");
    if(count1 != 1)
      strcpy_s(c.Ip[num], pair[i][1]);
    if(count2 != 1)
      strcpy_s(c.Port[num], pair[i+1][1]);
    if(count3 != 1)
      strcpy_s(c.File[num], pair[i+2][1]);
    num++;
  }

  for(int i=0; i<MAX_CLIENT; i++)
  {
    if((strcmp(c.Ip[i], "")!=0) && (strcmp(c.Port[i], "")!=0) && (strcmp(c.File[i], "")!=0))
    {
      c.NeedConnect[i] = true;
      c.STATUS[i] = CONNECT;
    }
  }

  clientDisplay(sockfd, "<html>\n");
  clientDisplay(sockfd, "<head>\n");
  clientDisplay(sockfd, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf8\" />\n");
  clientDisplay(sockfd, "</head>\n");
  clientDisplay(sockfd, "<body>\n");
  clientDisplay(sockfd, "<body bgcolor=#336699>\n");
  clientDisplay(sockfd, "<font face=\"Courier New\" size=2 color=#FFFF99>\n");
  clientDisplay(sockfd, "<table width=\"800\" border=\"1\">\n");
  clientDisplay(sockfd, "<tr>\n");


  EditPrintf(hwndEdit, TEXT("===CGI, client(%d), needconnect  ==> 1:%d, 2:%d, 3:%d, 4:%d, 5:%d \r\n"), sockfd, c.NeedConnect[0],c.NeedConnect[1],c.NeedConnect[2],c.NeedConnect[3],c.NeedConnect[4] );
  connectAmount = 0;
  for(int i=0; i<MAX_CLIENT; i++)
  {
    if(c.NeedConnect[i]==true)
    {
      // handle socket
      if((c.Fd[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        EditPrintf(hwndEdit, TEXT("===CGI, client(%d), socket error \r\n"),i);

      // handle address and port
      c.addr[i].sin_family = AF_INET;
      c.addr[i].sin_addr.s_addr = inet_addr(c.Ip[i]);
      c.addr[i].sin_port = htons(atoi(c.Port[i]));

      int n = connect(c.Fd[i], (struct sockaddr*)&c.addr[i], sizeof(c.addr[i]));
      if(n == SOCKET_ERROR)
      {
        EditPrintf(hwndEdit, TEXT("===CGI, client(%d), connect error \r\n"),i);
        EditPrintf(hwndEdit, TEXT("=== 357----close sock %d\r\n"), sockfd);
        closesocket(sockfd);
        WSACleanup();
      }
      else
      {
        // open file and  print table
        //c.InputFd[i] = fopen(c.File[i],_O_RDONLY);
        c.InputFd[i].open(c.File[i]);
        if(c.InputFd[i] < 0)
          EditPrintf(hwndEdit, TEXT("===CGI, client(%d), open file error \r\n"),i);
        else
          EditPrintf(hwndEdit, TEXT("===client(%d), open file success, filefd:%d \r\n"),i,c.InputFd[i]);

        // print IP
        ostringstream oss1;
        oss1 << "<td>" << c.Ip[i] <<"</td>" << endl;
        clientDisplay(sockfd, oss1.str());

        // set event
        int err =  WSAAsyncSelect(c.Fd[i], hwnd, CGI_CONNECT_NOTIFY, FD_CONNECT | FD_CLOSE | FD_READ | FD_WRITE);
        if(err == SOCKET_ERROR)
        {
          EditPrintf(hwndEdit, TEXT("===CGI, client(%d), select error \r\n"),i);
          EditPrintf(hwndEdit, TEXT("=== 380----close sock %d\r\n"), sockfd);
          closesocket(sockfd);
          WSACleanup();
          return;
        }
        else
        {
          c.STATUS[i] = RECEIVE;
          connectAmount++;
        }
      }
    }
    else
    {
      ostringstream oss1;
      oss1 << "<td>" << c.Ip[i] <<"</td>" << endl;
      clientDisplay(sockfd, oss1.str());
    }
  }
  clientDisplay(sockfd, "</tr>\n");
  clientDisplay(sockfd, "<tr>\n");
  for(int i = 0; i<MAX_CLIENT; i++)
  {
    ostringstream oss2;
    oss2 << "<td valign=\"top\"><pre id=\"m" << i << "\"></pre></td>" << endl;
    clientDisplay(sockfd, oss2.str());
  }
  clientDisplay(sockfd, "</tr>\n");
  clientDisplay(sockfd, "</table>\n");
  clientDisplay(sockfd, "</font>\n");
  clientDisplay(sockfd, "</body>\n");
  clientDisplay(sockfd, "</html>\n");
}

int findSocketIdex(int sockfd)
{
  for(int i=0; i<MAX_CLIENT; i++)
  {
    if(c.Fd[i] == sockfd)
      return i;
  }
  return -1;
}

int splitStr2(char *input, char** output, char *splitdelim)
{
  char *p;
  int word = 0;
  char *token=strtok_s(input, splitdelim ,&p);
  while(token!=NULL){
    output[word] = token;
    word++;
    token=strtok_s(NULL, splitdelim, &p);
  }
  return word;
}


void initialUserInfo()
{
  memset(c.Ip, 0, sizeof(c.Ip));
  memset(c.Port, 0, sizeof(c.Port));
  memset(c.File, 0, sizeof(c.File));
  for(int i=0; i<MAX_CLIENT; i++)
  {
    c.NeedConnect[i] = false;
    c.Fd[i] = -1;
    c.STATUS[i] = -1;
    c.Logout[i] = false;
    //c.InputFd[i] = -1;
  }

}

void initialAfterLeave(int i)
{
  memset(c.Ip[i], 0, sizeof(c.Ip[i]));
  memset(c.Port[i], 0, sizeof(c.Port[i]));
  memset(c.File[i], 0, sizeof(c.File[i]));
  c.NeedConnect[i] = false;
  c.Fd[i] = -1;
  c.STATUS[i] = -1;
  c.Logout[i] = false;
  //c.InputFd[i] = -1;
}

bool strcompare(string s1, string s2)
{
  if(strcmp(s1.c_str(),s2.c_str()) == 0)
    return true;
  else
    return false;
}

void clientDisplay(int sockfd, string s)
{
  send(sockfd, s.c_str(), strlen(s.c_str()), 0);
  //EditPrintf(hwndEdit, TEXT("===Client Display ===> %s \r\n"), s.c_str());
}

int sendToServer(int sockfd, string s)
{ int n;
if((n =send(sockfd, s.c_str(), s.length(), 0)) < 0)
{
  EditPrintf(hwndEdit, TEXT("===Client(%d) write to Server error === \r\n"), sockfd);
  return -1;
}
EditPrintf(hwndEdit, TEXT("===Client send ===> %s \r\n"), s.c_str());
return n;
}


int EditPrintf (HWND hwndEdit, TCHAR * szFormat, ...)
{
  TCHAR   szBuffer [1024] ;
  va_list pArgList ;

  va_start (pArgList, szFormat) ;
  wvsprintf (szBuffer, szFormat, pArgList) ;
  va_end (pArgList) ;

  SendMessage (hwndEdit, EM_SETSEL, (WPARAM) -1, (LPARAM) -1) ;
  SendMessage (hwndEdit, EM_REPLACESEL, FALSE, (LPARAM) szBuffer) ;
  SendMessage (hwndEdit, EM_SCROLLCARET, 0, 0) ;
  return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0);
}

int readline(int fd, char *ptr, int maxlen)
{
  int n, rc;
  char c;
  for(n = 0; n<maxlen; n++){
    rc = recv(fd, &c, 1, 0);
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
int readline2(int fd, char *ptr, int maxlen)
{
  int n, rc;
  char c;
  for(n = 0; n<maxlen; n++){
    rc = recv(fd, &c, 1,0);
    if(rc==1){
      //      if(c == '\n') break;
      *ptr++=c;
      if( c=='\n')
        break;
    }
    else if(rc ==0){
      if(n==0) return 0;   /*END of File*/
      else break;
    }
    else break;
  }
  //    *ptr = 0;
  return n;
}

string addToHtml(int userid, char *msg, bool n_To_Br)
{
  std::ostringstream oss;
  for (int i=0; i < strlen(msg)+1; i++)
  {
    /* for browser showing command successfully*/
    if (msg[i] == '\n')
    {
      msg[i] = ' ';
    }
    if(msg[i] == '\r')
    {
      msg[i] = ' ';
    }
  }
  char output[300]="";
  convertTag(msg, output, strlen(msg));
  //
  if(n_To_Br)
    oss << "<script>document.all['m" << userid << "'].innerHTML+= \"" << output << "<br>\";</script>";
  else
    oss << "<script>document.all['m" << userid << "'].innerHTML+= \"" << output << "\";</script>";
  //  cout << "<script>document.all['m"<<userid<< "'].innerHTML += \"" << msg << "<br>\";</script>";
  return oss.str();
}

void convertTag(const char *input, char *output, size_t len)
{
  int n;
  char c;
  for (n = 0; n < len; n++) {
    c = *(input+n);
    //    cout << "t >> " << c << endl;
    switch (c) {
    case '<':
      *output++ = '&';
      *output++ = 'l';
      *output++ = 't';
      break;
    case '>':
      *output++ = '&';
      *output++ = 'g';
      *output++ = 't';
      break;
    case '"':
      *output++ = '\\';
      *output++ = '"';
      break;
    case '\'':
      *output++ = '\\';
      *output++ = '\'';
    default:
      *output++ = c;
      break;
    }
  }
  *output=0;
}

int readlineFromFile(FILE *fp, char*line)
{
  int num_chars = 0, ch = 'a';
  for(; ch != EOF && ch != '\n';){
    ch = fgetc(fp);
    line[num_chars++] = ch;
  }
  line[num_chars] = '\0';
  return num_chars;
}
