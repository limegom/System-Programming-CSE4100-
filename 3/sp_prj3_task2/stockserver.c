#define _POSIX_C_SOURCE 200809L  /* pthread_rwlock_t 등의 POSIX 기능 활성화 */
#include "csapp.h"
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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

/* 연결 큐 노드 */
typedef struct conn_node {
    int connfd;
    struct conn_node *next;
} conn_node_t;

/* 연결 큐 및 동기화 변수 */
static conn_node_t *q_head = NULL, *q_tail = NULL;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  queue_cond  = PTHREAD_COND_INITIALIZER;
/* 현재 서비스 중인 클라이언트 수 */
static int active_clients = 0;

/* 쓰레드 풀 크기 */
#define NTHREADS 4
/* 출력 버퍼 크기 */
#define MAXLINE 8192

/* 주식 데이터 동기화(RW lock) */
static pthread_rwlock_t tree_lock;

/* 함수 원형 */
void load_stock(const char *filename);
void save_stock(const char *filename);
item_t *insert_item(item_t *node, item_t *new);
item_t *find_item(item_t *node, int id);
void inorder_save(item_t *node, FILE *fp);
void build_stock_str(item_t *node, char *out);
void print_stock(int connfd, item_t *node);

void sigint_handler(int sig);
void *worker_thread(void *vargp);
void service_client(int connfd);

/* SIGINT 핸들러: 서버 종료 플래그만 세우고 listenfd 닫기 */
void sigint_handler(int sig) {
    shutdown_requested = 1;
    Close(listenfd);
}

/* 연결 큐에 삽입 */
void enqueue(int connfd) {
    conn_node_t *node = malloc(sizeof(*node));
    if (!node) {
        perror("malloc");
        return;
    }
    node->connfd = connfd;
    node->next = NULL;

    pthread_mutex_lock(&queue_mutex);
    if (q_tail)
        q_tail->next = node;
    else
        q_head = node;
    q_tail = node;
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
}

/* 연결 큐에서 꺼내기 (없으면 조건변수로 대기, 서버 종료 시 -1 반환) */
int dequeue() {
    pthread_mutex_lock(&queue_mutex);
    while (!q_head && !shutdown_requested)
        pthread_cond_wait(&queue_cond, &queue_mutex);

    if (!q_head && shutdown_requested) {
        pthread_mutex_unlock(&queue_mutex);
        return -1;
    }

    conn_node_t *node = q_head;
    q_head = node->next;
    if (!q_head)
        q_tail = NULL;
    pthread_mutex_unlock(&queue_mutex);

    int fd = node->connfd;
    free(node);
    return fd;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* 1) 주식 데이터 로드 */
    load_stock("stock.txt");

    /* 2) RW lock 초기화 */
    if (pthread_rwlock_init(&tree_lock, NULL) != 0) {
        perror("pthread_rwlock_init");
        exit(1);
    }

    /* 3) SIGINT 핸들러 등록 */
    Signal(SIGINT, sigint_handler);

    /* 4) 듣기 소켓 생성 */
    listenfd = Open_listenfd(argv[1]);

    /* 5) 쓰레드 풀 생성 */
    pthread_t tids[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        Pthread_create(&tids[i], NULL, worker_thread, NULL);
    }

    /* 6) Master thread: 연결 받아서 큐에 추가 */
    while (!shutdown_requested) {
        struct sockaddr_storage clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        int connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);

        if (connfd < 0) {
            /* EINTR: signal, EBADF: listenfd 닫힘 */
            if (errno == EINTR || (shutdown_requested && errno == EBADF))
                break;
            unix_error("Accept error");
        }

        /* 활성 클라이언트 수 증가 */
        pthread_mutex_lock(&queue_mutex);
        active_clients++;
        pthread_mutex_unlock(&queue_mutex);

        char host[MAXLINE], port[MAXLINE];
        Getnameinfo((SA *)&clientaddr, clientlen,
                    host, MAXLINE, port, MAXLINE, 0);
        printf("Connected to %s:%s (active: %d)\n",
               host, port, active_clients);

        enqueue(connfd);
    }

    /* 7) 종료 시: 모든 worker 깨우고 join */
    pthread_mutex_lock(&queue_mutex);
    shutdown_requested = 1;
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    for (int i = 0; i < NTHREADS; i++) {
        Pthread_join(tids[i], NULL);
    }

    /* 8) 최종 저장 및 정리 */
    printf("Server shutting down, saving stock.txt...\n");
    save_stock("stock.txt");
    pthread_rwlock_destroy(&tree_lock);
    printf("stock.txt saved. Server exiting.\n");
    return 0;
}

/* Worker thread 함수 */
void *worker_thread(void *vargp) {
    while (1) {
        int connfd = dequeue();
        if (connfd < 0)  /* 서버 종료 시 */
            return NULL;

        service_client(connfd);
        Close(connfd);

        /* 클라이언트 처리 완료 → active_clients 감소.
           마지막 클라이언트가 나갔을 때 stock.txt만 저장 */
        pthread_mutex_lock(&queue_mutex);
        active_clients--;
        if (active_clients == 0) {
            /* 트리 구조가 변경 중이지 않도록 쓰기 잠금 */
            pthread_rwlock_wrlock(&tree_lock);
            save_stock("stock.txt");
            pthread_rwlock_unlock(&tree_lock);
            printf("All clients disconnected, stock.txt saved.\n");
        }
        pthread_mutex_unlock(&queue_mutex);
    }
}

/* 한 클라이언트 요청 처리 */
void service_client(int connfd) {
    rio_t rio;
    char buf[MAXLINE], out[MAXLINE], cmd[MAXLINE];
    int id, num;

    Rio_readinitb(&rio, connfd);
    while (Rio_readlineb(&rio, buf, MAXLINE) > 0) {
        memset(out, 0, sizeof(out));
        if (sscanf(buf, "%s %d %d", cmd, &id, &num) < 1)
            continue;

        if (strcmp(cmd, "show") == 0) {
            /* 읽기 잠금 */
            pthread_rwlock_rdlock(&tree_lock);
            print_stock(connfd, root);
            pthread_rwlock_unlock(&tree_lock);

        } else if (strcmp(cmd, "buy") == 0 || strcmp(cmd, "sell") == 0) {
            /* 쓰기 잠금 */
            pthread_rwlock_wrlock(&tree_lock);
            item_t *it = find_item(root, id);
            if (!it) {
                snprintf(out, MAXLINE, "Invalid stock ID: %d\n", id);
            }
            else if (strcmp(cmd, "buy") == 0) {
                if (it->left_stock >= num) {
                    it->left_stock -= num;
                    snprintf(out, MAXLINE, "[buy] success\n");
                } else {
                    snprintf(out, MAXLINE, "Not enough left stocks\n");
                }
            } else {
                it->left_stock += num;
                snprintf(out, MAXLINE, "[sell] success\n");
            }
            /*  변경 전: Rio_writen(connfd, out, strlen(out)); */
            Rio_writen(connfd, out, MAXLINE);  /* 반드시 8192바이트 전송 */
            pthread_rwlock_unlock(&tree_lock);

        } else if (strcmp(cmd, "exit") == 0) {
            break;

        } else {
            int prefix_len = snprintf(out, MAXLINE, "Unknown command: ");
            if (prefix_len < MAXLINE - 1) {
                snprintf(out + prefix_len,
                         MAXLINE - prefix_len,
                         "%.*s",
                         MAXLINE - prefix_len - 1,
                         buf);
            }
            /*  변경 전: Rio_writen(connfd, out, strlen(out)); */
            Rio_writen(connfd, out, MAXLINE);    /* 반드시 8192바이트 전송 */
        }
    }
}

/* stock.txt → BST 로드 */
void load_stock(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("fopen"); exit(1); }
    while (1) {
        item_t *node = malloc(sizeof(item_t));
        if (fscanf(fp, "%d %d %d",
                   &node->id, &node->left_stock, &node->price) != 3) {
            free(node);
            break;
        }
        node->left = node->right = NULL;
        root = insert_item(root, node);
    }
    fclose(fp);
}

/* BST ← 파일 덮어쓰기 */
void save_stock(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) { perror("fopen"); return; }
    inorder_save(root, fp);
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
    return (id < node->id)
           ? find_item(node->left, id)
           : find_item(node->right, id);
}

/* BST 중위 순회로 파일에 저장 */
void inorder_save(item_t *node, FILE *fp) {
    if (!node) return;
    inorder_save(node->left, fp);
    fprintf(fp, "%d %d %d\n",
            node->id, node->left_stock, node->price);
    inorder_save(node->right, fp);
}

/* BST → 단일 문자열(out)에 누적 (show용) */
void build_stock_str(item_t *node, char *out) {
    char line[64];  /* 한 줄을 임시로 저장 */
    if (!node) return;

    build_stock_str(node->left, out);

    /* sprintf → snprintf으로 변경: line 크기(64바이트) 초과 방지 */
    snprintf(line, sizeof(line), "%d %d %d\n",
             node->id, node->left_stock, node->price);

    /* strcat → strncat으로 변경: out 버퍼(MAXLINE) 초과 방지 */
    strncat(out, line, MAXLINE - strlen(out) - 1);

    build_stock_str(node->right, out);
}

/* 한 번에 MAXLINE 바이트로 전송 */
void print_stock(int connfd, item_t *node) {
    char out[MAXLINE] = {0};
    build_stock_str(node, out);
    /* 변경 전: Rio_writen(connfd, out, strlen(out)); */
    Rio_writen(connfd, out, MAXLINE);   /* 반드시 8192바이트 전송 */
}
