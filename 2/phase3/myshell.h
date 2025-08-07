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
#define MAXJOBS 128     // phase3: 최대 job 수 정의
/* phase3: 백그라운드 작업 및 job 제어를 위한 자료구조와 상수 정의 */
typedef struct job {
    int jid;                    // phase3: job 번호
    pid_t pid;                  // phase3: 프로세스 ID
    char command[MAXLINE];      // phase3: 잡에 해당하는 명령어 문자열
    int running;                // phase3: 실행 중이면 1, 중지되면 0
} job;

extern job job_list[MAXJOBS];   // phase3: 전역 job 리스트
extern int next_jid;            // phase3: 다음 job 번호


/* 함수 프로토타입 */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
void sigchld_handler(int sig);
void eval_pipe(char *cmdline);

////phase3////
int add_job(pid_t pid, char *cmdline, int running);   // phase3: job 추가
void delete_job(pid_t pid);                           // phase3: job 삭제
job* find_job_by_jid(int jid);                        // phase3: JID로 job 검색
job* find_job_by_pid(pid_t pid);                      // phase3: PID로 job 검색
void list_jobs(void);                                 // phase3: job 목록 출력

#endif
