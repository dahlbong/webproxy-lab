/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "../csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* 두 인자 추출 */
  if((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');
    *p = '\0';
    // strcpy(arg1, buf);
    // strcpy(arg2, p+1);
    // n1 = atoi(arg1);
    // n2 = atoi(arg2);

    /* 숙제 10 */
    sscanf(buf, "num1 = %d", &n1);
    sscanf(p + 1, "num2 = %d", &n2);
  }

  /* 응답 바디 생성 */
  int idx = 0;
  idx += sprintf(content + idx, "QUERY_STRING=%s", buf);
  idx += sprintf(content + idx, "Welcome to add.com: ");
  idx += sprintf(content + idx, "The Internet addition portal. \r\n<p>");
  idx += sprintf(content + idx, "The answer is: %d + %d = %d \r\n", n1, n2, n1+n2);
  idx += sprintf(content + idx, "Thanks for visiting! \r\n");

  /* HTTP 응답 생성 */
  printf("Connection: close \r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);
  
  exit(0);
}
/* $end adder */
