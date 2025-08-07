#include "myshell.h"
#include <ctype.h> // phase2: isspace()를 위해 추가(다중파이프라인을 위해서)

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

    if (strchr(buf, '|') != NULL) {   // phase2: 파이프가 포함된 경우
        eval_pipe(buf);
        return;
    }

    bg = parseline(buf, argv);
    if (argv[0] == NULL){
        return;  /* Ignore empty lines */
    }
    if (builtin_command(argv)){
        return;  // 내장 명령어이면 여기서 처리
    }
    if ((pid = Fork()) == 0) {  // 자식 프로세스 생성
        if (execvp(argv[0], argv) < 0) { // execvp()를 사용하여 PATH 기반으로 외부 명령어 실행
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

/* phase2: parseline() 수정 – 공백 기준 분리 시 인용부호로 묶인 인자는 제거 */
int parseline(char *buf, char **argv) {
    int argc = 0;
    char *s = buf;
    char *token;
    /* 트레일링 뉴라인 제거 */
    char *newline = strchr(buf, '\n');
    if (newline)
        *newline = '\0';
    while (*s) {
        while (*s && isspace((unsigned char)*s))
            s++;
        if (!*s)
            break;
        if (*s == '"') {
            s++;  /* phase2: 시작 인용부호 건너뛰기 */
            token = s;
            while (*s && *s != '"')
                s++;
            if (*s == '"') {
                *s = '\0';
                s++;
            }
        } else {
            token = s;
            while (*s && !isspace((unsigned char)*s))
                s++;
            if (*s) {
                *s = '\0';
                s++;
            }
        }
        argv[argc++] = token;
    }
    argv[argc] = NULL;
    if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
        argv[argc - 1] = NULL;
        return 1;
    }
    return 0;
}

/* builtin_command: 내장 명령어 처리 */
int builtin_command(char **argv) {
    // quit, exit
    if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "exit") == 0){
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

/* phase2: 파이프라인 명령어 처리를 위한 함수 (여러 파이프 '|'가 연결된 경우 처리) */
void eval_pipe(char *cmdline) {
    char *commands[100];   // 파이프라인 명령어 배열 (최대 100개)
    int num_commands = 0;
    char *token = strtok(cmdline, "|");
    while (token != NULL) {
        while (*token == ' ')
            token++;
        commands[num_commands++] = token;
        token = strtok(NULL, "|");
    }

    int i;
    int pipefd[2];
    int prev_fd = -1;
    pid_t pid;

    for (i = 0; i < num_commands; i++) {
        if (i < num_commands - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe error");
                exit(1);
            }
        }
        if ((pid = Fork()) == 0) {
            if (i > 0) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (i < num_commands - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);
            }
            char *argv[MAXARGS];
            char cmd_copy[MAXLINE];
            strncpy(cmd_copy, commands[i], MAXLINE);
            cmd_copy[MAXLINE-1] = '\0';
            // size_t len = strlen(cmd_copy);
            // if (len > 0 && cmd_copy[len-1] != '\n') {
            //     if (len < MAXLINE - 1) {
            //         cmd_copy[len] = '\n';
            //         cmd_copy[len+1] = '\0';
            //     }
            // }
            /* phase2: 인용부호 제거를 위해 새 parseline() 사용 */
            int bg = parseline(cmd_copy, argv);
            if (builtin_command(argv))
                exit(0);
            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
        if (i > 0)
            close(prev_fd);
        if (i < num_commands - 1) {
            prev_fd = pipefd[0];
            close(pipefd[1]);
        }
    }
    for (i = 0; i < num_commands; i++) {
        int status;
        wait(&status);
    }
}

int main() 
{
    char cmdline[MAXLINE];

    while (1) { // 파이프 라인을 위해 무한루프
        printf("CSE4100-SP-P2> ");
        fflush(stdout); // 출력 버퍼 비우기

        /* 사용자가 Ctrl+D로 EOF를 보냈는지 체크 */
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
            fprintf(stderr, "fgets error\n");
            continue;
        }
        if (feof(stdin)) {                // Ctrl+D 등으로 EOF
            printf("\n");
            exit(0);
        }

        /* (3) 명령어 해석 및 실행 */
        eval(cmdline);  // eval() 안에서 내부 명령어 처리 + fork/exec 수행
    }
    return 0;
}