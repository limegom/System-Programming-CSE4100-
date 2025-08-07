#ifndef MYSHELL_H
#define MYSHELL_H

#include <sys/types.h>  /* pid_t 등의 타입 정의 */
#include <stddef.h>     /* NULL 정의 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include "csapp.h"

/* 상수 정의 */
#define MAXLINE 8192    /* 명령어 최대 길이 */
#define MAXARGS 128     /* 명령어 인자 최대 개수 */

/* 함수 프로토타입 */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
void sigchld_handler(int sig);

#endif
