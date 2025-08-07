#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>

/* BST 노드: 주식 ID, 재고, 가격, 좌/우 자식 */
typedef struct item {
    int id;
    int left_stock;
    int price;
    struct item *left, *right;
} item_t;

static item_t *root = NULL;               /* BST 루트 */
static int listenfd;                      /* 듣기 소켓 */
static volatile sig_atomic_t shutdown_requested = 0;
static int active_client_count = 0;       /* 연결된 클라이언트 수 */
static rio_t rio_pool[FD_SETSIZE];        /* 각 connfd 별 RIO 버퍼 */

/* 함수 원형 */
void load_stock(const char *filename);
void save_stock(const char *filename);
item_t *insert_item(item_t *node, item_t *new);
item_t *find_item(item_t *node, int id);
void inorder_save(item_t *node, FILE *fp);
void build_stock_str(item_t *node, char *out);
void print_stock(int connfd, item_t *node);
void sigint_handler(int sig);
int handle_request(int connfd);

int main(int argc, char **argv) {
    fd_set master_set, read_set;
    int maxfd, nready, connfd, fd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char host[MAXLINE], port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    load_stock("stock.txt");                     /* 초기 데이터 로드 */
    Signal(SIGINT, sigint_handler);              /* Ctrl-C 핸들러 */

    listenfd = Open_listenfd(argv[1]);            /* 듣기 소켓 생성 */
    FD_ZERO(&master_set);
    FD_SET(listenfd, &master_set);
    maxfd = listenfd;

    /* 이벤트 루프 */
    while (!shutdown_requested || active_client_count > 0) {
        read_set = master_set;
        int rc;
        /* 시스템 select() 호출, EINTR 재시도 */
        do {
            rc = select(maxfd + 1, &read_set, NULL, NULL, NULL);
        } while (rc < 0 && errno == EINTR);

        if (rc < 0) {
            if (!shutdown_requested) {
                fprintf(stderr, "select error: %s\n", strerror(errno));
                exit(1);
            }
            break;
        }
        nready = rc;

        /* 1) 새 연결 처리 */
        if (!shutdown_requested && FD_ISSET(listenfd, &read_set)) {
            clientlen = sizeof(clientaddr);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            Getnameinfo((SA *)&clientaddr, clientlen,
                        host, MAXLINE, port, MAXLINE, 0);
            printf("Connected to %s:%s  (active clients: %d→%d)\n",
                   host, port, active_client_count, active_client_count + 1);

            Rio_readinitb(&rio_pool[connfd], connfd);
            FD_SET(connfd, &master_set);
            if (connfd > maxfd) maxfd = connfd;
            active_client_count++;
            nready--;
        }

        /* 2) 기존 클라이언트 요청 처리 */
        for (fd = 0; fd <= maxfd && nready > 0; fd++) {
            if (!FD_ISSET(fd, &read_set) || fd == listenfd) continue;
            nready--;
            if (handle_request(fd) < 0) {
                /* 클라이언트 연결 종료 감지 */
                printf("Client fd=%d disconnected  (remaining clients: %d→%d)\n",
                       fd, active_client_count, active_client_count - 1);
                Close(fd);
                FD_CLR(fd, &master_set);
                active_client_count--;

                /* 마지막 클라이언트 나가면 자동 저장 */
                if (active_client_count == 0 && !shutdown_requested) {
                    printf("Last client gone, saving stock.txt...\n");
                    save_stock("stock.txt");
                    printf("stock.txt saved.\n");
                }
            }
        }
    }

    /* Ctrl-C 시 또는 shutdown_requested 상태에서 모든 클라이언트 종료 후 */
    printf("All clients done, saving stock.txt...\n");
    save_stock("stock.txt");
    printf("stock.txt saved. Server exiting.\n");
    return 0;
}

/* SIGINT(Ctrl-C) 시 더 이상 새 연결 받지 않고 select() 탈출 유도 */
void sigint_handler(int sig) {
    shutdown_requested = 1;
    Close(listenfd);
}

/* stock.txt → BST 로드 */
void load_stock(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("fopen"); exit(1); }
    while (1) {
        item_t *node = malloc(sizeof(item_t));
        if (fscanf(fp, "%d %d %d", &node->id, &node->left_stock, &node->price) != 3) {
            free(node);
            break;
        }
        node->left = node->right = NULL;
        root = insert_item(root, node);
    }
    fclose(fp);
}

/* BST에 새 노드 삽입 */
item_t *insert_item(item_t *node, item_t *new) {
    if (!node) return new;
    if (new->id < node->id)
        node->left = insert_item(node->left, new);
    else
        node->right = insert_item(node->right, new);
    return node;
}

/* BST에서 ID로 검색 */
item_t *find_item(item_t *node, int id) {
    if (!node) return NULL;
    if (id == node->id) return node;
    return (id < node->id) ? find_item(node->left, id) : find_item(node->right, id);
}

/* BST 중위 순회로 파일에 저장 */
void inorder_save(item_t *node, FILE *fp) {
    if (!node) return;
    inorder_save(node->left, fp);
    fprintf(fp, "%d %d %d\n", node->id, node->left_stock, node->price);
    inorder_save(node->right, fp);
}

/* stock.txt ← BST 내용 덮어쓰기 */
void save_stock(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) { perror("fopen"); return; }
    inorder_save(root, fp);
    fclose(fp);
}

/* BST → 단일 문자열(out)에 누적 (show용) */
void build_stock_str(item_t *node, char *out) {
    char line[64];
    if (!node) return;
    build_stock_str(node->left, out);
    sprintf(line, "%d %d %d\n", node->id, node->left_stock, node->price);
    strcat(out, line);
    build_stock_str(node->right, out);
}

/* 한 번에 MAXLINE 바이트로 전송 */
void print_stock(int connfd, item_t *node) {
    char out[MAXLINE] = {0};
    build_stock_str(node, out);
    Rio_writen(connfd, out, MAXLINE);
}

/* 한 클라이언트 요청 처리 */
int handle_request(int connfd) {
    char buf[MAXLINE], out[MAXLINE] = {0}, cmd[MAXLINE];
    int id, num;
    item_t *it;

    /* 요청 한 줄 수신 */
    if (Rio_readlineb(&rio_pool[connfd], buf, MAXLINE) <= 0)
        return -1;  /* EOF 또는 오류 시 종료 */

    if (sscanf(buf, "%s %d %d", cmd, &id, &num) < 1)
        return 0;

    if (strcmp(cmd, "show") == 0) {
        print_stock(connfd, root);
    } else if (strcmp(cmd, "buy") == 0 || strcmp(cmd, "sell") == 0) {
        it = find_item(root, id);
        if (!it) {
            sprintf(out, "Invalid stock ID: %d\n", id);
        }
        else if (strcmp(cmd, "buy") == 0) {
            if (it->left_stock >= num) {
                it->left_stock -= num;
                //sprintf(out, "[buy] %d %d %d\n", it->id, it->left_stock, it->price);
                sprintf(out, "[buy] success\n");
            }
            else {
                //sprintf(out, "Not enough left stock: %d\n", id);
                sprintf(out, "Not enough left stocks\n");
            }
        }
        else {
            it->left_stock += num;
            //sprintf(out, "[sell] %d %d %d\n", it->id, it->left_stock, it->price);
            sprintf(out, "[sell] success\n");
        }
        Rio_writen(connfd, out, MAXLINE);
    } else if (strcmp(cmd, "exit") == 0) {
        return -1;
    } else {
        sprintf(out, "Unknown command: %s", buf);
        Rio_writen(connfd, out, MAXLINE);
    }

    return 0;
}
