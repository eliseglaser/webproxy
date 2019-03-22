#include "csapp.h"
#include <stdio.h>
#include <string.h>
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
// char *serverHost;
// char *serverRequest;
// char *serverPort;
int browserfd;

struct cache_entry
{
  int timestamp;
  int valid;
  char *key; //request
  char *value; //response
};
struct cache_entry cache[3]; //for proxy, change to 2 or 3
int timecount;

int connect_client( int argc, char **argv );
void reader(rio_t rio, int connfd);
void read_requesthdrs(rio_t *rp, char *buf);
void parseGet(char *get, char serverHost[MAXLINE], char serverRequest[MAXLINE], char serverPort[MAXLINE]);
void sendheaders();
rio_t connectServer(char *hostname,char* query, char *port);
void sendServerResponse(char *clientbuf, rio_t rio);
int cacheLookup (char *clientbuf);
void initicache();
void cacheInsert( char *clientbuf, char *serverresponse);
char *returnCacheResponse(int i);



int main(int argc, char **argv)
{
  initicache();
  printf("%s", user_agent_hdr);
  if( argc < 2 )
  {
    printf( "USAGE" );
    exit( 0 );
  }
  return connect_client(argc, argv);
}

int connect_client( int argc, char **argv )
{

  char *port = argv[1];

  //tells program to listen on this port
  int listenfd = Open_listenfd(port);
  rio_t rio;
  //buffer is just a pointer to some memory
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  char client_hostname[MAXLINE], client_port[MAXLINE];

  while (1) {
    clientlen = sizeof(clientaddr);
    int connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    browserfd = connfd;
    Rio_readinitb(&rio, connfd);
    Getnameinfo( (SA *) &clientaddr, clientlen, client_hostname, MAXLINE,
    client_port, MAXLINE, 0);
    printf("Connected to (%s, %s)\n", client_hostname, client_port);
    reader(rio, connfd);
    Close(connfd);
  }

  printf ("EXITING");
  exit(0);
  Close(listenfd);
}


void reader(rio_t rio, int connfd)
{
  char buf[MAXLINE];

  char serverHost[MAXLINE], serverRequest[MAXLINE], serverPort[MAXLINE];

  serverHost[0] = 0;
  serverRequest[0] = 0;
  serverPort[0] = 0;
  //have if statements to check validity and type of req
  Rio_readinitb(&rio, connfd);

  if (!Rio_readlineb(&rio, buf, MAXLINE)){  //line:netp:doit:readrequest
      return;
    }
  char *get = buf;
  parseGet(get, serverHost, serverRequest, serverPort);
  //read_requesthdrs(&rio, buf);
  int n= cacheLookup(get);
  //printf ("N is: %d\n",n);
  if (!n){
    rio_t serverRIO = connectServer(serverHost, serverRequest, serverPort);
    sendServerResponse(get, serverRIO);
  }else{
    n = n -1;
    char *cacheResponse = returnCacheResponse(n);
    sprintf(buf, "%s\n", cacheResponse);
    Rio_writen(browserfd, buf, MAXLINE);
  }
}



void read_requesthdrs(rio_t *rp, char *buf)
{
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
	     Rio_readlineb(rp, buf, MAXLINE);
	      printf("%s", buf);
    }

}

void parseGet(char *get, char serverHost[MAXLINE],char serverRequest[MAXLINE],char serverPort[MAXLINE])
{
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE], query[MAXLINE];
  sscanf(get, "%s %s %s", method, uri, version);
  strcpy (serverRequest, method);
  strcat (serverRequest, " ");
  sscanf (uri, "http://%[^:/]%s", serverHost, query);
  sscanf (query, ":%[^/]%s", serverPort, query);
  strcat (serverRequest, query);
  strcat (serverRequest, " HTTP/1.0");
}


rio_t connectServer(char *hostname,char* serverReq, char *port)
{
      char buf[MAXLINE];
      if ((strlen(port))==0){
        port = "80";
      }
      rio_t rio;
      int clientfd = open_clientfd(hostname, port);
      Rio_readinitb(&rio, clientfd);
      sprintf(buf, "%s\r\n", serverReq);
      Rio_writen(clientfd, buf, strlen(buf));
      sprintf(buf, "Host: %s\r\n", hostname);
      Rio_writen(clientfd, buf, strlen(buf));
      sprintf(buf, "User-Agent: %s\r\n", user_agent_hdr);
      Rio_writen(clientfd, buf, strlen(buf));
      sprintf(buf, "Connection: close\r\n");
      Rio_writen(clientfd, buf, strlen(buf));
      sprintf(buf, "Proxy-Connection: close\r\n");
      //if a browser sends any additional request headers as part of an HTTP request, forward them unchanged.
      Rio_writen(clientfd, buf, strlen(buf));
      printf("got here");
      return rio;
     Close(clientfd);
     exit(0);
}


void sendServerResponse(char *get, rio_t rio){
  char buf[MAXLINE];
  char response [MAX_OBJECT_SIZE];
  response[0] = 0;
  size_t n;

  while((n = Rio_readlineb(&rio, buf, MAXLINE))!=0) {
    Rio_writen(browserfd, buf, strlen(buf));
    strcat(response, buf);
    }
  cacheInsert(get, response);
  return;
}



void initicache()
{
  timecount = 0;
  for( int  i = 0; i < 3; ++ i)
  {
    cache[ i ].valid = 0;
  }
}

int cacheLookup (char *clientbuf){
  int hasResponse = 0;
  ++timecount;
  for( int i = 0; i < 3; i++)
  {
    if(cache[ i ].valid && (!strcmp(cache [ i ].key, clientbuf)))
    {
      cache [i].timestamp = timecount;
      hasResponse = i+1;
    }
  }
  return hasResponse;
}

char *returnCacheResponse(int i){
  //return cache response at i, found from cache look up
  return cache[i].value;
}

void cacheInsert( char *clientbuf, char *serverresponse)
{
  ++timecount;
  for( int i = 0; i < 3; i++)
  {
    if(! cache[ i ].valid)
    {
      cache [i].timestamp = timecount;
      cache [i].key = clientbuf;
      cache[i].value = serverresponse;
      cache[i].valid = 1;

      return;
    }
  }
    //find the oldest entry in the cache
}
//cache eviction- keep timestamp of cache, kick out oldest entry

// int fib( int x )
// {
// if (x is in the cache)
//   return cached number;
// if (x < 2 )
//   return 1;
// else
//   int result = fib( x - 1 _ + fib (x - 2);
//   put result in the cache
//   return result;
//
// }
