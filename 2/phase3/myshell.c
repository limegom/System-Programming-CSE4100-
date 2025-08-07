#include "myshell.h"
#include <ctype.h> // phase2: isspace()를 위해 추가(다중파이프라인을 위해서)

volatile pid_t fg_pid = 0; // 중복 wait 방지

/* phase3: job 전역 변수 정의 */
job job_list[MAXJOBS];          // phase3: 전역 job 리스트 정의
int next_jid = 1;               // phase3: 다음 job 번호 초기화
// phase3: job 추가
int add_job(pid_t pid, char *cmdline, int running) {
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == 0) {
            job_list[i].pid = pid;
            job_list[i].jid = next_jid++;
            strncpy(job_list[i].command, cmdline, MAXLINE);
            job_list[i].command[MAXLINE-1] = '\0';
            job_list[i].running = running;
            return job_list[i].jid;
        }
    }
    fprintf(stderr, "add_job: Too many jobs\n"); // phase3: job 수 초과 에러
    return -1;
}

// phase3: 종료된 job을 리스트에서 제거
void delete_job(pid_t pid) {
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == pid) {
            job_list[i].pid = 0;
            break;
        }
    }
}

// phase3: jid로 job 찾기
job* find_job_by_jid(int jid) {
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid != 0 && job_list[i].jid == jid)
            return &job_list[i];
    }
    return NULL;
}

// phase3: PID로 job 검색
job* find_job_by_pid(pid_t pid) {
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == pid)
            return &job_list[i];
    }
    return NULL;
}

// phase3: job 리스트 출력
void list_jobs(void) {
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid != 0) {
            printf("[%d] %d %s %s\n",
                   job_list[i].jid,
                   job_list[i].pid,
                   job_list[i].running ? "Running" : "Stopped",
                   job_list[i].command);
        }
    }
}

/* SIGCHLD 핸들러 */
void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    // phase3: 자식의 종료 및 중지를 감지하기 위해 WNOHANG | WUNTRACED 옵션 사용
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) { // phase3: 종료/중지 감지
        if (pid == fg_pid){
            continue; // phase3: fg는 eval()에서 대기하므로 건너뜀
        }
        job* jb = find_job_by_pid(pid);
        if (jb) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {// 자식이 종료
                // job 리스트에서 삭제하고 알림 출력
                printf("\nJob [%d] (%d) terminated\n", jb->jid, pid); // terminated 알림
                delete_job(pid);
            } else if (WIFSTOPPED(status)) {// 자식이 stopped
                //상태를 업데이트하고 알림 출력
                jb->running = 0;
                printf("\nJob [%d] (%d) stopped by signal\n", jb->jid, pid); // stopped 알림
            }
        }
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
        return; // 빈 명령어 처리
    }
    if (builtin_command(argv)){
        return; // 내장 명령어이면 여기서 처리
    }
    if ((pid = Fork()) == 0) { // 자식 프로세스 생성

        Setpgid(0, 0); // phase3: 자식이 자신만의 새로운 프로세스 그룹 생성

        // phase3: 자식 Signal 처리 (bg/fg 제어 위해)
        Signal(SIGINT, SIG_DFL); // phase3: SIGINT
        Signal(SIGTSTP, SIG_DFL); // phase3: SIGTSTP

        if (execvp(argv[0], argv) < 0) { // execvp()를 사용하여 PATH 기반으로 외부 명령어 실행
            printf("%s: Command not found.\n", argv[0]);
            exit(0);
        }
    }
    /* 부모 프로세스 */
    if (!bg) { // 포그라운드 작업이면
        int status;
        fg_pid = pid; // phase3: fg PID 저장
        // phase3: 자식의 중지 상태도 감지하기 위해 WUNTRACED 옵션 사용
        if (waitpid(pid, &status, WUNTRACED) < 0){
            unix_error("waitpid error");
        }
        if (WIFSTOPPED(status)) { // phase3: 자식이 중지된 경우 잡 리스트에 추가
            int jid = add_job(pid, cmdline, 0); // phase3: 상태 0 = 중지됨
            printf("\nJob [%d] (%d) stopped\n", jid, pid); // phase3: 중지 알림
        }
    } else { // 백그라운드 작업이면
        int jid = add_job(pid, cmdline, 1); // phase3: 잡을 백그라운드 작업으로 추가 (1 = 실행중)
        printf("[%d] %d\n", jid, pid); // phase3: 백그라운드 잡 정보 출력
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
    // phase3: jobs 명령어 – 잡 목록 출력
    if (strcmp(argv[0], "jobs") == 0) {
        list_jobs(); // phase3: 잡 목록 출력 함수 호출
        return 1;
    }
    // phase3: bg 명령어 – 중지된 잡을 백그라운드에서 재개
    if (strcmp(argv[0], "bg") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "bg: usage: bg <jobid>\n"); // phase3: 사용법 출력
            return 1;
        }
        int jid = atoi(argv[1]); // phase3: 잡 번호 변환
        job* jb = find_job_by_jid(jid);
        if (jb == NULL) {
            fprintf(stderr, "bg: No such job\n"); // phase3: 잡이 없으면 에러 메시지
            return 1;
        }
        if (!jb->running) {
            if (kill(-jb->pid, SIGCONT) < 0) { // phase3: 프로세스 그룹에 SIGCONT 전송으로 재개
                perror("bg: kill (SIGCONT) error");
            }
            jb->running = 1;
            printf("Job [%d] (%d) resumed in background\n", jb->jid, jb->pid); // phase3: 재개 알림
        }
        return 1;
    }
    // phase3: fg 명령어 – bg job을 fg로 전환
    if (strcmp(argv[0], "fg") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "fg: usage: fg <jobid>\n"); // 잘못누르면 사용법 출력
            return 1;
        }
        int jid = atoi(argv[1]); // jid 변환
        job* jb = find_job_by_jid(jid);
        if (jb == NULL) {
            fprintf(stderr, "fg: No such job\n"); // job 없으면 에러
            return 1;
        }
        if (kill(-jb->pid, SIGCONT) < 0) { // SIGCONT 전송 -> fg로 전환
            perror("fg: kill (SIGCONT) error");
        }
        int status;
        if (waitpid(jb->pid, &status, WUNTRACED) < 0) { // fg 작업 대기하고
            perror("fg: waitpid error");
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) // 프로세스 종료되면,
            delete_job(jb->pid); // job 삭제
        else if (WIFSTOPPED(status)) // ㄴstopped 상태면,
            jb->running = 0; // 상태 업데이트
        return 1;
    }
    // phase3: kill – 특정 job 종료 명령어
    if (strcmp(argv[0], "kill") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "kill: usage: kill <jobid>\n"); // 파라미터 적으면, 사용법 출력
            return 1;
        }
        int jid = atoi(argv[1]); // jid 변환
        job* jb = find_job_by_jid(jid);
        if (jb == NULL) {
            fprintf(stderr, "kill: No such job\n"); //job 없음 에러
            return 1;
        }
        if (kill(-jb->pid, SIGTERM) < 0) { // SIGTERM 전송하고, 종료
            perror("kill error");
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

    // phase3: 시그널 핸들러 설정 – SIGCHLD: 자식 종료/중지, SIGINT/SIGTSTP는 쉘 무시
    Signal(SIGCHLD, sigchld_handler); // 자식 종료 및 중지 감지
    Signal(SIGINT, SIG_IGN);          // 쉘 ctrl-c(SIGINT) 무시
    Signal(SIGTSTP, SIG_IGN);         // 쉘 ctrl-z(SIGTSTP) 무시

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