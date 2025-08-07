#include "myshell.h"

/* SIGCHLD 핸들러 */
void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // 필요시 종료된 자식 정보를 남길 수 있게 만들어 둠
    }
    return;
}

/* eval: 명령어 문자열을 평가하여 내장 명령어 처리 또는 외부 명령어 실행 */
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL){
        return;  /* Ignore empty lines */
    }
    if (builtin_command(argv)){
        return;  // 내장 명령어이면 여기서 처리
    }
    if ((pid = Fork()) == 0) {  // 자식 프로세스 생성
        if (execvp(argv[0], argv) < 0) {
            printf("%s: Command not found.\n", argv[0]);
            exit(0);
        }
    }
    /* Parent waits for foreground job to terminate */
    if (!bg) {
        int status;
        if (waitpid(pid, &status, 0) < 0)
            unix_error("waitpid error");
    } else {    //when there is backgrount process!
        printf("%d %s", pid, cmdline);
    }
    return;
}

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim;         /* Points to first space delimiter */
    int argc = 0;        /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf) - 1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) {/* Ignore leading spaces */
        buf++;
    }
    while ((delim = strchr(buf, ' '))) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) {
            buf++;
        }
    }
    argv[argc] = NULL;
    if (argc == 0) {
        return 1;
    }
    if ((bg = (argv[argc - 1][0] == '&')) != 0) {
        argv[--argc] = NULL;
    }
    return bg;
}

/* builtin_command: 내장 명령어 처리 */
int builtin_command(char **argv) {
    // exit
    if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "exit") == 0){
        exit(0);
    }
    // quit
    if (strcmp(argv[0], "quit") == 0) {
        exit(0);
    }
    // cd
    if (strcmp(argv[0], "cd") == 0) {
        if (argv[1] == NULL){
            fprintf(stderr, "cd: missing argument\n");
        } else {
            if (chdir(argv[1]) < 0) {
                perror("cd error");
            }
        }
        return 1;
    }
    // &(백그라운드)
    if (strcmp(argv[0], "&") == 0) {
        return 1;
    }
    return 0;
}

int main()
{
    char cmdline[MAXLINE];

    /* SIGCHLD 핸들러 */
    Signal(SIGCHLD, sigchld_handler);

    while (1) {
        printf("CSE4100-SP-P2> ");
        if (fgets(cmdline, MAXLINE, stdin) == NULL) {
            if (feof(stdin))
                exit(0);
            continue;
        }
        eval(cmdline);
    }
    return 0;
}