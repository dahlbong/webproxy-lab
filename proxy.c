#include <stdio.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void doit(int fd);
void doRequest(int serverfd, char *method, char *path, char *hostname);
void doResponse(int serverfd, int clientfd);
int parse_uri(char *uri, char *hostname, char *path, char *port);
void clientError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char *argv[]) {

    setbuf(stdout, NULL);

    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }
}

void doit(int clientfd) {
  int serverfd;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
  rio_t clientRio, serverRio;

  printf("\n<<<< New Client Request >>>>\n");
  Rio_readinitb(&clientRio, clientfd);
  if (!Rio_readlineb(&clientRio, buf, MAXLINE))
      return;

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

  printf("Connecting to server: %s:%s\n", hostname, port);
  serverfd = Open_clientfd(hostname, port);
  if (serverfd < 0) {
      clientError(clientfd, hostname, "404", "Not found", "Could not connect to server");
      return;
  }

  Rio_readinitb(&serverRio, serverfd);
  doRequest(serverfd, method, path, hostname);
  doResponse(serverfd, clientfd);

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

  printf("\n<<<< Proxy Request to Server >>>>\n");

  sprintf(buf, "%s %s HTTP/1.0\r\n", method, path);
  printf("Request Line: %s", buf);
  Rio_writen(serverfd, buf, strlen(buf));

  sprintf(buf, "Host: %s\r\n", hostname);
  printf("<HEADER>\nHost: %s", buf);
  Rio_writen(serverfd, buf, strlen(buf));

  sprintf(buf, "%s", user_agent_hdr);
  printf("User-Agent: %s", buf);
  Rio_writen(serverfd, buf, strlen(buf));

  sprintf(buf, "Connection: close\r\n");
  printf("Connection: %s", buf);
  Rio_writen(serverfd, buf, strlen(buf));

  sprintf(buf, "Proxy-Connection: close\r\n\r\n");
  printf("Proxy-Connection: %s", buf);
  Rio_writen(serverfd, buf, strlen(buf));
}

void doResponse(int serverfd, int clientfd) {
  char buf[MAXLINE];
  rio_t rio;
  ssize_t n;
  int totalBytes = 0, headerEnd = 0;

  printf("\n<<<< Server Response >>>>\n");
  Rio_readinitb(&rio, serverfd);

  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    printf("Received: %s", buf);
    Rio_writen(clientfd, buf, n);
    totalBytes += n;

    if (strcmp(buf, "\r\n") == 0) {
      headerEnd = 1;
      break;
    }
  }
  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    Rio_writen(clientfd, buf, n);
    totalBytes += n;
  }
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