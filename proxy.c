#include <stdio.h>
#include "csapp.h"
#include "cache.c"

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void doit(int fd);
void *thread(void *varp);
void doRequest(int serverfd, char *method, char *path, char *hostname);
void doResponse(int serverfd, int clientfd, char *path);
int parse_uri(char *uri, char *hostname, char *path, char *port);
void clientError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* 캐싱 및 동기화에 필요한 전역변수 선언 및 캐시 자료구조 생성 */
Cache *cache;
sem_t cacheMutex;

int main(int argc, char *argv[]) {
  int listenfd, *connfdp;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;                        // 멀티쓰레딩용 쓰레드id
  Sem_init(&cacheMutex, 0, 1);          // 세마포어 초기화

  if (argc != 2) {
      fprintf(stderr, "usage: %s <port>\n", argv[0]);
      exit(0);
  }
  listenfd = Open_listenfd(argv[1]);    // 지정 포트에서 연결 요청 수신할 준비: 소켓 생성 & 바인딩 & 수신 대기 상태 전환
  cache = createCache();
  // while (1) {
  //     clientlen = sizeof(clientaddr);
  //     connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
  //     Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
  //     printf("Accepted connection from (%s, %s)\n", hostname, port);
  //     doit(connfd);
  //     Close(connfd);
  // }

  /* PART2 동시성 서버: 멀티쓰레딩 구현 */
  while (1) {
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int));                                // 각 쓰레드가 독립적으로 fd 참조 가능하도록 동적메모리 할당
    *connfdp = Accept(listenfd, (SA*) &clientaddr, &clientlen);   // 클라이언트의 연결 받아들이고, connfdp 포인터에 연결된 소켓의 fd 저장
    Pthread_create(&tid, NULL, thread, connfdp);                  // 새로운 쓰레드 생성되어 connfdp 값 참조 가능하게 됨
  }
  freeCache(cache);
  return 0;
}

/* Thread Routine */
void *thread(void *vargp) {
  int connfd = *((int*)vargp);
  int niters = 0, cnt = 0;
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

void doit(int clientfd) {
  int serverfd;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
  rio_t clientRio, serverRio;
  CacheNode *cached;

  printf("\n<<<< New Client Request >>>>\n");
  Rio_readinitb(&clientRio, clientfd);
  if (!Rio_readlineb(&clientRio, buf, MAXLINE)) return;

  printf("Client Request Line: %s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("Method: %s, URI: %s, Version: %s\n", method, uri, version);

  if (strcasecmp(method, "GET") != 0 && strcasecmp(method, "HEAD") != 0) {
      clientError(clientfd, method, "501", "Not implemented", "Proxy does not implement this method");
      return;
  }

  if (parse_uri(uri, hostname, path, port) < 0) {
      clientError(clientfd, uri, "400", "Bad request", "Proxy could not parse URI");
      return;
  }

  /* PART3 캐싱 서버: 서버 접속 전에 Cache에서 데이터 먼저 찾기 */

  sem_wait(&cacheMutex);  // 세마포어 대기
  /* -------------------- race condition 발생 가능 지역 ---------------------------------*/
  cached = readCache(cache, path);
  if (cached !=  NULL) {
    sendCache(cache, clientfd, cached);
    sem_post(&cacheMutex);
    return; 
  }
  /* -------------------- race condition 발생 가능 지역 ---------------------------------*/
  sem_post(&cacheMutex);
  
  
  // 캐시에 데이터 없으면 서버 접속
  printf("Connecting to server: %s:%s\n", hostname, port);
  serverfd = Open_clientfd(hostname, port);
  if (serverfd < 0) {
      clientError(clientfd, hostname, "404", "Not found", "Could not connect to server");
      return;
  }

  Rio_readinitb(&serverRio, serverfd);
  doRequest(serverfd, method, path, hostname);  
  doResponse(serverfd, clientfd, path);               //PART3: 서버에서 데이터 받아오고 client 응답주기 전에 cache에 write 필요
  Close(serverfd);
}

int parse_uri(char *uri, char *hostname, char *path, char *port) {
  strcpy(port, "80");
  char *hoststart = strstr(uri, "://") ? strstr(uri, "://") + 3 : uri;
  char *pathstart = strchr(hoststart, '/');

  if (pathstart) {
      strcpy(path, pathstart);
      *pathstart = '\0';
  } else
      strcpy(path, "/");

  char *portstart = strchr(hoststart, ':');
  if (portstart) {
      *portstart = '\0';
      strcpy(port, portstart + 1);
  }
  strcpy(hostname, hoststart);
  return 0;
}

void doRequest(int serverfd, char *method, char *path, char *hostname) {
  char buf[MAXLINE];
  int idx = 0;

  printf("\n<<<< Proxy Request to Server >>>>\n");

  sprintf(buf, "%s %s HTTP/1.0\r\n", method, path);
  printf("<Request line>\n%s", buf);
  Rio_writen(serverfd, buf, strlen(buf));

  idx += sprintf(buf + idx, "Host: %s\r\n", hostname);
  idx += sprintf(buf + idx, "%s", user_agent_hdr);
  idx += sprintf(buf + idx, "Connection: close\r\n");
  idx += sprintf(buf + idx, "Proxy-Connection: close\r\n\r\n");
  Rio_writen(serverfd, buf, strlen(buf));

  printf("<Header>\n%s", buf);
}

void doResponse(int serverfd, int clientfd, char *path) {
  char buf[MAXLINE];
  char *responseBuffer = malloc(MAX_OBJECT_SIZE);
  rio_t rio;
  ssize_t n;
  int totalBytes = 0, responseSize = 0;

  printf("\n<<<< Server Response >>>>\n");
  Rio_readinitb(&rio, serverfd);
  int isHeader = 1;                       // 헤더 구분자

  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    printf("Received: %s", buf);
    Rio_writen(clientfd, buf, n);

    if (responseSize + n < MAX_OBJECT_SIZE) {     // 전체 응답을 responseBuffer에 누적 
        memcpy(responseBuffer + responseSize, buf, n);
        responseSize += n;
    }

    if (isHeader && strcmp(buf, "\r\n") == 0) {    // 헤더가 끝났는지 확인
        isHeader = 0;
    }
    totalBytes += n;
  }

  sem_wait(&cacheMutex);
  /* -------------------- race condition 발생 가능 지역 ---------------------------------*/
  writeCache(cache, path, responseBuffer, responseSize);     // 캐시에 전체 응답 데이터 새로 저장
  /* -------------------- race condition 발생 가능 지역 ---------------------------------*/
  sem_post(&cacheMutex);

  free(responseBuffer);
  printf("<<<< Response Complete >>>>\r\n");
}

void clientError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Proxy Error</title>");
  sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Proxy Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}