#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "list.h"
#include "hash.h"
#include "bitmap.h"
#include <stddef.h>
#include <stdint.h>


// 최대 자료구조 개수 (리스트, 해시테이블, 비트맵 각각 30개 이하)
#define MAX_DS 30

// 자료구조 종류 정의
typedef enum { DS_LIST, DS_HASH, DS_BITMAP } ds_type;

// 자료구조 엔트리: 이름, 종류, 그리고 실제 자료구조 포인터
typedef struct {
    char title[32];
    ds_type type;
    void *ds;
} ds_entry;

ds_entry ds_array[MAX_DS];
int ds_count = 0;

/* --- 추가: 리스트와 해시 테이블에 저장할 데이터 --- */

// list_item: 리스트 원소는 정수 데이터와 list_elem를 포함함.
struct list_item {
    struct list_elem elem; // list.h에 정의된 자료구조
    int real;
};


/* 해시 함수 및 비교 함수 (명세에 따라 hash_int 사용) */
unsigned hash_item_hash(const struct hash_elem *e, void *aux) {
    struct hash_item *item = hash_item_entry(e);
    return hash_int(item->real);
}

bool hash_item_less(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
    struct hash_item *ia = hash_item_entry(a);
    struct hash_item *ib = hash_item_entry(b);
    return ia->real < ib->real;
}

/* list_item 비교 함수 (list_unique 등에서 사용) */
bool list_item_less(const struct list_elem *a, const struct list_elem *b, void *aux) {
    struct list_item *ia = (struct list_item *) ((uint8_t *)a - offsetof(struct list_item, elem));
    struct list_item *ib = (struct list_item *) ((uint8_t *)b - offsetof(struct list_item, elem));
    return ia->real < ib->real;
}

/* 자료구조 검색: 이름으로 ds_entry 찾기 */
ds_entry* find_ds(const char *title) {
    for (int i = 0; i < ds_count; i++) {
        if (strcmp(ds_array[i].title, title) == 0)
            return &ds_array[i];
    }
    return NULL;
}

/* ---------- 기본 명령어 처리 함수들 ---------- */

/* create list <LIST> */
void cmd_create_list(char *title) {
    if (ds_count >= MAX_DS) {
        printf("error: maximum data structures reached\n");
        return;
    }
    struct list *lst = malloc(sizeof(struct list));
    if (!lst) {
        printf("error: memory allocation failed\n");
        return;
    }
    list_init(lst);
    strncpy(ds_array[ds_count].title, title, sizeof(ds_array[ds_count].title)-1);
    ds_array[ds_count].title[sizeof(ds_array[ds_count].title)-1] = '\0';
    ds_array[ds_count].type = DS_LIST;
    ds_array[ds_count].ds = lst;
    ds_count++;
}

/* create hashtable <HASH TABLE> */
void cmd_create_hash(char *title) {
    if (ds_count >= MAX_DS) {
        printf("error: maximum data structures reached\n");
        return;
    }
    struct hash *ht = malloc(sizeof(struct hash));
    if (!ht) {
        printf("error: memory allocation failed\n");
        return;
    }
    if (!hash_init(ht, hash_item_hash, hash_item_less, NULL)) {
        printf("error: hash_init failed\n");
        free(ht);
        return;
    }
    strncpy(ds_array[ds_count].title, title, sizeof(ds_array[ds_count].title)-1);
    ds_array[ds_count].title[sizeof(ds_array[ds_count].title)-1] = '\0';
    ds_array[ds_count].type = DS_HASH;
    ds_array[ds_count].ds = ht;
    ds_count++;
}

/* create bitmap <BITMAP> <BIT CNT> */
void cmd_create_bitmap(char *title, char *bit_cnt_str) {
    if (ds_count >= MAX_DS) {
        printf("error: maximum data structures reached\n");
        return;
    }
    size_t bit_cnt = (size_t) atoi(bit_cnt_str);
    struct bitmap *bm = bitmap_create(bit_cnt);
    if (!bm) {
        printf("error: bitmap_create failed\n");
        return;
    }
    strncpy(ds_array[ds_count].title, title, sizeof(ds_array[ds_count].title)-1);
    ds_array[ds_count].title[sizeof(ds_array[ds_count].title)-1] = '\0';
    ds_array[ds_count].type = DS_BITMAP;
    ds_array[ds_count].ds = bm;
    ds_count++;
}

/* delete <DATA_STRUCTURE> */
void cmd_delete(char *title) {
    for (int i = 0; i < ds_count; i++) {
        if (strcmp(ds_array[i].title, title) == 0) {
            if (ds_array[i].type == DS_LIST) {
                // TODO: 필요시 리스트 원소 메모리 해제
                free(ds_array[i].ds);
            } else if (ds_array[i].type == DS_HASH) {
                hash_destroy(ds_array[i].ds, NULL);
            } else if (ds_array[i].type == DS_BITMAP) {
                bitmap_destroy(ds_array[i].ds);
            }
            for (int j = i; j < ds_count - 1; j++)
                ds_array[j] = ds_array[j+1];
            ds_count--;
            return;
        }
    }
    printf("error: %s not found\n", title);
}

/* dumpdata <DATA_STRUCTURE> */
void cmd_dumpdata(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry) {
        printf("error: %s not found\n", title);
        return;
    }
    if (entry->type == DS_LIST) {
        struct list *lst = (struct list*) entry->ds;
        for (struct list_elem *e = list_begin(lst); e != list_end(lst); e = list_next(e)) {
            struct list_item *item = list_entry(e, struct list_item, elem);
            printf("%d ", item->real);
        }
        printf("\n");
    } else if (entry->type == DS_HASH) {
        struct hash *ht = (struct hash*) entry->ds;
        struct hash_iterator i;
        for (hash_first(&i, ht); i.elem != NULL; hash_next(&i)) {
            struct hash_item *item = hash_item_entry(i.elem);
            printf("%d ", item->real);
        }
        printf("\n");
    } else if (entry->type == DS_BITMAP) {
        struct bitmap *bm = (struct bitmap*) entry->ds;
        size_t size = bitmap_size(bm);
        for (size_t i = 0; i < size; i++)
            printf("%d", bitmap_test(bm, i) ? 1 : 0);
        printf("\n");
    }
}

/* ---------- 추가: LIST 명령어 ---------- */

/* list_push_front */
void cmd_list_push_front(char *title, char *num_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
        printf("error: list %s not found\n", title);
        return;
    }
    int value = atoi(num_str);
    struct list *lst = (struct list*) entry->ds;
    struct list_item *item = malloc(sizeof(struct list_item));
    if (!item) {
        printf("error: memory allocation failed\n");
        return;
    }
    item->real = value;
    list_push_front(lst, &item->elem);
}

/* list_push_back */
void cmd_list_push_back(char *title, char *num_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
        printf("error: list %s not found\n", title);
        return;
    }
    int value = atoi(num_str);
    struct list *lst = (struct list*) entry->ds;
    struct list_item *item = malloc(sizeof(struct list_item));
    if (!item) {
        printf("error: memory allocation failed\n");
        return;
    }
    item->real = value;
    list_push_back(lst, &item->elem);
}

/* list_pop_front: 삭제 후 원소 출력 */
void cmd_list_pop_front(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
        printf("error: list %s not found\n", title);
        return;
    }
    struct list *lst = (struct list*) entry->ds;
    if (list_empty(lst)) {
        printf("error: list is empty\n");
        return;
    }
    struct list_elem *e = list_pop_front(lst);
    struct list_item *item = list_entry(e, struct list_item, elem);
    free(item);
}

/* list_pop_back */
void cmd_list_pop_back(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
        printf("error: list %s not found\n", title);
        return;
    }
    struct list *lst = (struct list*) entry->ds;
    if (list_empty(lst)) {
        printf("error: list is empty\n");
        return;
    }
    struct list_elem *e = list_pop_back(lst);
    struct list_item *item = list_entry(e, struct list_item, elem);
    free(item);
}

/* list_front: 첫 원소 출력 */
void cmd_list_front(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
        printf("error: list %s not found\n", title);
        return;
    }
    struct list *lst = (struct list*) entry->ds;
    if (list_empty(lst)) {
        printf("error: list is empty\n");
        return;
    }
    struct list_item *item = list_entry(list_front(lst), struct list_item, elem);
    printf("%d\n", item->real);
}

/* list_back: 마지막 원소 출력 */
void cmd_list_back(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
        printf("error: list %s not found\n", title);
        return;
    }
    struct list *lst = (struct list*) entry->ds;
    if (list_empty(lst)) {
        printf("error: list is empty\n");
        return;
    }
    struct list_item *item = list_entry(list_back(lst), struct list_item, elem);
    printf("%d\n", item->real);
}

/* list_insert: 인덱스 위치에 원소 삽입 */
void cmd_list_insert(char *title, char *idx_str, char *num_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
        printf("error: list %s not found\n", title);
        return;
    }
    int idx = atoi(idx_str);
    int value = atoi(num_str);
    struct list *lst = (struct list*) entry->ds;
    struct list_elem *e = list_begin(lst);
    int i = 0;
    while (e != list_end(lst) && i < idx) {
        e = list_next(e);
        i++;
    }
    struct list_item *item = malloc(sizeof(struct list_item));
    if (!item) {
        printf("error: memory allocation failed\n");
        return;
    }
    item->real = value;
    list_insert(e, &item->elem);
}

/* list_insert_ordered: 정렬된 위치에 원소 삽입 */
void cmd_list_insert_ordered(char *title, char *num_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
         printf("error: list %s not found\n", title);
         return;
    }
    int value = atoi(num_str);
    struct list *lst = (struct list*) entry->ds;
    struct list_item *item = malloc(sizeof(struct list_item));
    if (!item) {
         printf("error: memory allocation failed\n");
         return;
    }
    item->real = value;
    list_insert_ordered(lst, &item->elem, list_item_less, NULL);
}

/* list_empty: 빈 리스트 여부 출력 */
void cmd_list_empty(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
         printf("error: list %s not found\n", title);
         return;
    }
    struct list *lst = (struct list*) entry->ds;
    printf("%s\n", list_empty(lst) ? "true" : "false");
}

/* list_size: 리스트 크기 출력 */
void cmd_list_size(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
         printf("error: list %s not found\n", title);
         return;
    }
    struct list *lst = (struct list*) entry->ds;
    printf("%zu\n", list_size(lst));
}

/* list_max: 최소 원소 출력 (list_max 함수가 있다고 가정) */
void cmd_list_max(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
         printf("error: list %s not found\n", title);
         return;
    }
    struct list *lst = (struct list*) entry->ds;
    if (list_empty(lst)) {
         printf("error: list is empty\n");
         return;
    }
    struct list_elem *max_elem = list_max(lst, list_item_less, NULL);
    struct list_item *item = list_entry(max_elem, struct list_item, elem);
    printf("%d\n", item->real);
}

/* list_min: 최소 원소 출력 (list_min 함수가 있다고 가정) */
void cmd_list_min(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
         printf("error: list %s not found\n", title);
         return;
    }
    struct list *lst = (struct list*) entry->ds;
    if (list_empty(lst)) {
         printf("error: list is empty\n");
         return;
    }
    struct list_elem *min_elem = list_min(lst, list_item_less, NULL);
    struct list_item *item = list_entry(min_elem, struct list_item, elem);
    printf("%d\n", item->real);
}

/* list_remove: 인덱스 위치 원소 제거 후 출력 */
void cmd_list_remove(char *title, char *idx_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
         printf("error: list %s not found\n", title);
         return;
    }
    int idx = atoi(idx_str);
    struct list *lst = (struct list*) entry->ds;
    if (idx < 0 || idx >= list_size(lst)) {
         printf("error: index out of range\n");
         return;
    }
    struct list_elem *e = list_begin(lst);
    for (int i = 0; i < idx; i++) {
         e = list_next(e);
    }
    struct list_item *item = list_entry(e, struct list_item, elem);
    list_remove(e);
    free(item);
}

/* list_reverse */
void cmd_list_reverse(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
         printf("error: list %s not found\n", title);
         return;
    }
    struct list *lst = (struct list*) entry->ds;
    list_reverse(lst);
}

/* list_sort */
void cmd_list_sort(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
         printf("error: list %s not found\n", title);
         return;
    }
    struct list *lst = (struct list*) entry->ds;
    list_sort(lst, list_item_less, NULL);
}
// cmd_list_splice(list1, idx1, list2, idx2, idx3);로 (char *src_title, char *start_str, char *end_str, char *tgt_title, char *tgt_idx_str)
void cmd_list_splice(char *tgt_title, char *tgt_idx_str, char *src_title, char *start_str, char *end_str) {
    ds_entry *entry_src = find_ds(src_title);
    ds_entry *entry_tgt = find_ds(tgt_title);
    if (!entry_src || entry_src->type != DS_LIST || !entry_tgt || entry_tgt->type != DS_LIST) {
        printf("error: list not found\n");
        return;
    }
    struct list *lst_src = (struct list*) entry_src->ds;
    struct list *lst_tgt = (struct list*) entry_tgt->ds;
    int start = atoi(start_str);
    int end = atoi(end_str);
    int tgt_idx = atoi(tgt_idx_str);

    if (start < 0 || end > list_size(lst_src) || start >= end) {
        printf("error: invalid splice range\n");
        return;
    }
    if (tgt_idx < 0 || tgt_idx > list_size(lst_tgt)) {
        printf("error: target index out of range\n");
        return;
    }

    // source 리스트에서 splice할 첫 번째 요소인 first와 마지막 요소인 last를 찾는다 (last는 splice 범위의 끝으로 포함되지 않음)
    struct list_elem *first = list_begin(lst_src);
    for (int i = 0; i < start; i++) {
        first = list_next(first);
    }
    struct list_elem *last = list_begin(lst_src);
    for (int i = 0; i < end; i++) {
        last = list_next(last);
    }
    
    // target 리스트에서 before를 찾는다.
    struct list_elem *before = list_begin(lst_tgt);
    for (int i = 0; i < tgt_idx; i++) {
        before = list_next(before);
    }
    
    // splice 수행: before 위치에, source의 [first, last) 요소를 삽입.
    list_splice(before, first, last);
}


/* list_swap: 이미 구현됨 */
void cmd_list_swap(char *title, char *idx1_str, char *idx2_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
        printf("error: list %s not found\n", title);
        return;
    }
    size_t idx1 = atoi(idx1_str), idx2 = atoi(idx2_str);
    struct list *lst = (struct list*) entry->ds;
    if (idx1 == idx2) return;
    if (idx1 >= list_size(lst) || idx2 >= list_size(lst)) {
        printf("error: index out of range\n");
        return;
    }
    struct list_elem *e1 = list_begin(lst);
    struct list_elem *e2 = list_begin(lst);
    for (size_t i = 0; i < idx1; i++) e1 = list_next(e1);
    for (size_t i = 0; i < idx2; i++) e2 = list_next(e2);
    list_swap(e1, e2);
}

/* cmd_list_shuffle: 제목에 해당하는 리스트를 찾아서 리스트를 무작위 섞음 */
void cmd_list_shuffle(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_LIST) {
        printf("error: list %s not found\n", title);
        return;
    }
    struct list *lst = (struct list*) entry->ds;
    list_shuffle(lst);
}

/* cmd_list_unique:  리스트에서 인접한 중복 원소를 제거 */
void cmd_list_unique(char *list1, char *list2) {
    ds_entry *entry1 = find_ds(list1);
    if(list2 == NULL){
        struct list *l1 = (struct list*) entry1->ds;
        list_unique(l1, NULL, list_item_less, NULL);
        return ;
    }
    ds_entry *entry2 = find_ds(list2);
    if (!entry1 || entry1->type != DS_LIST) {
        printf("error: list %s not found\n", list1);
        return;
    }
    if (!entry2 || entry2->type != DS_LIST) {
        printf("error: list %s not found\n", list2);
        return;
    }
    struct list *l1 = (struct list*) entry1->ds;
    struct list *l2 = (struct list*) entry2->ds;
    /* 중복 계수를를 NULL로 전달하여 중복된 원소를 제거한다다 */
    list_unique(l1, l2, list_item_less, NULL);
}

/* ---------- 추가: BITMAP 명령어 ---------- */

/* bitmap_mark */
void cmd_bitmap_mark(char *title, char *bit_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    size_t bit = (size_t) atoi(bit_str);
    struct bitmap *bm = (struct bitmap*) entry->ds;
    bitmap_mark(bm, bit);
}

/* bitmap_all */
void cmd_bitmap_all(char *title, char *start_str, char *cnt_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    struct bitmap *bm = (struct bitmap*) entry->ds;
    size_t start = (size_t) atoi(start_str);
    size_t cnt = (size_t) atoi(cnt_str);
    printf("%s\n", bitmap_all(bm, start, cnt) ? "true" : "false");
}

/* bitmap_any */
void cmd_bitmap_any(char *title, char *start_str, char *cnt_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    struct bitmap *bm = (struct bitmap*) entry->ds;
    size_t start = (size_t) atoi(start_str);
    size_t cnt = (size_t) atoi(cnt_str);
    printf("%s\n", bitmap_any(bm, start, cnt) ? "true" : "false");
}

/* bitmap_contains */
void cmd_bitmap_contains(char *title, char *start_str, char *cnt_str, char *bool_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    struct bitmap *bm = (struct bitmap*) entry->ds;
    size_t start = (size_t) atoi(start_str);
    size_t cnt = (size_t) atoi(cnt_str);
    bool flag = (strcmp(bool_str, "true") == 0);
    printf("%s\n", bitmap_contains(bm, start, cnt, flag) ? "true" : "false");
}

/* bitmap_count */
void cmd_bitmap_count(char *title, char *start_str, char *cnt_str, char *bool_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    struct bitmap *bm = (struct bitmap*) entry->ds;
    size_t start = (size_t) atoi(start_str);
    size_t cnt = (size_t) atoi(cnt_str);
    bool flag = (strcmp(bool_str, "true") == 0);
    printf("%zu\n", bitmap_count(bm, start, cnt, flag));
}

/* bitmap_dump */
void cmd_bitmap_dump(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    struct bitmap *bm = (struct bitmap*) entry->ds;
    bitmap_dump(bm);
}

/* bitmap_expand */
void cmd_bitmap_expand(char *title, char *cnt) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    struct bitmap *bm = (struct bitmap*) entry->ds;
    size_t additional = (size_t) atoi(cnt);
    size_t new_cnt = bm->bit_cnt + additional;  // 현재 비트 수에 추가할 비트를 더함.
    if (!bitmap_expand(bm, new_cnt)) {
        printf("error: failed to expand bitmap\n");
    }
}

/* bitmap_set */
void cmd_bitmap_set(char *title, char *idx_str, char *bool_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    size_t idx = (size_t) atoi(idx_str);
    bool flag = (strcmp(bool_str, "true") == 0);
    struct bitmap *bm = (struct bitmap*) entry->ds;
    bitmap_set(bm, idx, flag);
}

/* bitmap_set_all */
void cmd_bitmap_set_all(char *title, char *bool_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    bool flag = (strcmp(bool_str, "true") == 0);
    struct bitmap *bm = (struct bitmap*) entry->ds;
    bitmap_set_all(bm, flag);
}

/* bitmap_flip */
void cmd_bitmap_flip(char *title, char *idx_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    size_t idx = (size_t) atoi(idx_str);
    struct bitmap *bm = (struct bitmap*) entry->ds;
    bitmap_flip(bm, idx);
}

/* bitmap_none */
void cmd_bitmap_none(char *title, char *start_str, char *cnt_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    size_t start = (size_t) atoi(start_str);
    size_t cnt = (size_t) atoi(cnt_str);
    struct bitmap *bm = (struct bitmap*) entry->ds;
    printf("%s\n", bitmap_none(bm, start, cnt) ? "true" : "false");
}

/* bitmap_reset */
void cmd_bitmap_reset(char *title, char *idx_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    size_t idx = (size_t) atoi(idx_str);
    struct bitmap *bm = (struct bitmap*) entry->ds;
    bitmap_reset(bm, idx);
}

/* bitmap_scan_and_flip */
void cmd_bitmap_scan_and_flip(char *title, char *start_str, char *cnt_str, char *bool_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    size_t start = (size_t) atoi(start_str);
    size_t cnt = (size_t) atoi(cnt_str);
    bool flag = (strcmp(bool_str, "true") == 0);
    struct bitmap *bm = (struct bitmap*) entry->ds;
    printf("%zu\n", bitmap_scan_and_flip(bm, start, cnt, flag));
}

/* bitmap_scan */
void cmd_bitmap_scan(char *title, char *start_str, char *cnt_str, char *bool_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    size_t start = (size_t) atoi(start_str);
    size_t cnt = (size_t) atoi(cnt_str);
    bool flag = (strcmp(bool_str, "true") == 0);
    struct bitmap *bm = (struct bitmap*) entry->ds;
    printf("%zu\n", bitmap_scan(bm, start, cnt, flag));
}

/* bitmap_set_multiple */
void cmd_bitmap_set_multiple(char *title, char *start_str, char *cnt_str, char *bool_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    size_t start = (size_t) atoi(start_str);
    size_t cnt = (size_t) atoi(cnt_str);
    bool flag = (strcmp(bool_str, "true") == 0);
    struct bitmap *bm = (struct bitmap*) entry->ds;
    bitmap_set_multiple(bm, start, cnt, flag);
}

/* bitmap_size */
void cmd_bitmap_size(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    struct bitmap *bm = (struct bitmap*) entry->ds;
    printf("%zu\n", bitmap_size(bm));
}

/* bitmap_test */
void cmd_bitmap_test(char *title, char *idx_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_BITMAP) {
        printf("error: bitmap %s not found\n", title);
        return;
    }
    size_t idx = (size_t) atoi(idx_str);
    struct bitmap *bm = (struct bitmap*) entry->ds;
    printf("%s\n", bitmap_test(bm, idx) ? "true" : "false");
}

/* ---------- 추가: HASH 명령어 ---------- */

/* 해시테이블 연산: hash_insert */
void cmd_hash_insert(char *name, char *num_str) {
    ds_entry *entry = find_ds(name);
    if (!entry || entry->type != DS_HASH) {
        printf("error: hashtable %s not found\n", name);
        return;
    }
    int value = atoi(num_str);
    struct hash *ht = (struct hash*) entry->ds;
    struct hash_item *item = malloc(sizeof(struct hash_item));
    if (!item) {
        printf("error: memory allocation failed\n");
        return;
    }
    item->real = value;
    struct hash_elem *old = hash_insert(ht, &item->elem);
    if (old != NULL) {
        free(item);  // 중복된 경우 새 항목 해제
    }
}

/* hash_size */
void cmd_hash_size(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_HASH) {
        printf("error: hashtable %s not found\n", title);
        return;
    }
    struct hash *ht = (struct hash*) entry->ds;
    printf("%zu\n", hash_size(ht));
}

// 
void cmd_apply_Hash(char *title, char *func_title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_HASH) {
        printf("error: hashtable %s not found\n", title);
        return;
    }
    struct hash *ht = (struct hash*) entry->ds;
    apply_Hash(ht, func_title); // 라이브러리 제공
}

/* hash_delete */
void cmd_hash_delete(char *title, char *num_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_HASH) {
        printf("error: hashtable %s not found\n", title);
        return;
    }
    struct hash *ht = (struct hash*) entry->ds;
    int value = atoi(num_str);
    hash_delete(ht, make_Hash_elem(value));
}

/* hash_empty */
void cmd_hash_empty(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_HASH) {
        printf("error: hashtable %s not found\n", title);
        return;
    }
    struct hash *ht = (struct hash*) entry->ds;
    printf("%s\n", hash_empty(ht) ? "true" : "false");
}

/* hash_clear */
void cmd_hash_clear(char *title) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_HASH) {
        printf("error: hashtable %s not found\n", title);
        return;
    }
    struct hash *ht = (struct hash*) entry->ds;
    hash_clear(ht, remove_Hash_elem);
}

/* hash_find */
void cmd_hash_find(char *title, char *num_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_HASH) {
        printf("error: hashtable %s not found\n", title);
        return;
    }
    struct hash *ht = (struct hash*) entry->ds;
    int value = atoi(num_str);
    struct hash_elem *he = hash_find(ht, make_Hash_elem(value));
    if (he != NULL) {
        // he->real는 직접 접근이 어려울 수 있으므로, 해시_item로 변환
        struct hash_item *item = hash_item_entry(he);
        printf("%d\n", item->real);
    }
}

/* hash_replace */
void cmd_hash_replace(char *title, char *num_str) {
    ds_entry *entry = find_ds(title);
    if (!entry || entry->type != DS_HASH) {
        printf("error: hashtable %s not found\n", title);
        return;
    }
    struct hash *ht = (struct hash*) entry->ds;
    int value = atoi(num_str);
    hash_replace(ht, make_Hash_elem(value));
}

/* ---------- process_command: 명령어 파싱 및 실행 ---------- */
void process_command(char *line) {
    // 줄바꿈 문자 제거
    line[strcspn(line, "\n")] = '\0';
    char *cmd = strtok(line, " ");
    if (cmd == NULL)
        return;
    
    if (strcmp(cmd, "quit") == 0) {
        exit(0);
    } else if (strcmp(cmd, "create") == 0) {
        char *type = strtok(NULL, " ");
        char *title = strtok(NULL, " ");
        if (type && title) {
            if (strcmp(type, "list") == 0) {
                cmd_create_list(title);
            } else if (strcmp(type, "hashtable") == 0) {
                cmd_create_hash(title);
            } else if (strcmp(type, "bitmap") == 0) {
                char *bit_cnt_str = strtok(NULL, " ");
                if (bit_cnt_str)
                    cmd_create_bitmap(title, bit_cnt_str);
                else
                    printf("error: missing bit count for bitmap\n");
            } else {
                printf("error: unknown create type %s\n", type);
            }
        }
    } else if (strcmp(cmd, "delete") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_delete(title);
    } else if (strcmp(cmd, "dumpdata") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_dumpdata(title);
    }
    /* LIST 명령어들 */
    else if (strcmp(cmd, "list_push_front") == 0) {
        char *title = strtok(NULL, " ");
        char *num_str = strtok(NULL, " ");
        if (title && num_str)
            cmd_list_push_front(title, num_str);
    } else if (strcmp(cmd, "list_push_back") == 0) {
        char *title = strtok(NULL, " ");
        char *num_str = strtok(NULL, " ");
        if (title && num_str)
            cmd_list_push_back(title, num_str);
    } else if (strcmp(cmd, "list_pop_front") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_list_pop_front(title);
    } else if (strcmp(cmd, "list_pop_back") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_list_pop_back(title);
    } else if (strcmp(cmd, "list_front") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_list_front(title);
    } else if (strcmp(cmd, "list_back") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_list_back(title);
    } else if (strcmp(cmd, "list_insert") == 0) {
        char *title = strtok(NULL, " ");
        char *idx_str = strtok(NULL, " ");
        char *num_str = strtok(NULL, " ");
        if (title && idx_str && num_str)
            cmd_list_insert(title, idx_str, num_str);
    } else if (strcmp(cmd, "list_insert_ordered") == 0) {
        char *title = strtok(NULL, " ");
        char *num_str = strtok(NULL, " ");
        if (title && num_str)
            cmd_list_insert_ordered(title, num_str);
    } else if (strcmp(cmd, "list_empty") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_list_empty(title);
    } else if (strcmp(cmd, "list_size") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_list_size(title);
    } else if (strcmp(cmd, "list_min") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_list_min(title);
    } else if (strcmp(cmd, "list_max") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_list_max(title);
    } else if (strcmp(cmd, "list_remove") == 0) {
        char *title = strtok(NULL, " ");
        char *idx_str = strtok(NULL, " ");
        if (title && idx_str)
            cmd_list_remove(title, idx_str);
    } else if (strcmp(cmd, "list_reverse") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_list_reverse(title);
    } else if (strcmp(cmd, "list_sort") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_list_sort(title);
    } else if (strcmp(cmd, "list_splice") == 0) {
        char *list1 = strtok(NULL, " ");
        char *idx1 = strtok(NULL, " ");
        char *list2 = strtok(NULL, " ");
        char *idx2 = strtok(NULL, " ");
        char *idx3 = strtok(NULL, " ");
        if (list1 && idx1 && list2 && idx2 && idx3)
            cmd_list_splice(list1, idx1, list2, idx2, idx3);
    } else if (strcmp(cmd, "list_swap") == 0) {
        char *title = strtok(NULL, " ");
        char *idx1 = strtok(NULL, " ");
        char *idx2 = strtok(NULL, " ");
        if (title && idx1 && idx2)
            cmd_list_swap(title, idx1, idx2);
    } else if (strcmp(cmd, "list_shuffle") == 0) {
    char *title = strtok(NULL, " ");
    if (title)
        cmd_list_shuffle(title);
} else if (strcmp(cmd, "list_unique") == 0) {
    char *list1 = strtok(NULL, " ");
    char *list2 = strtok(NULL, " ");
    if (list1){
        if (list2) {
            cmd_list_unique(list1, list2);
        }
        else{
            cmd_list_unique(list1, NULL);
        }
    }
}
    /* BITMAP 명령어들 */
    else if (strcmp(cmd, "bitmap_mark") == 0) {
        char *title = strtok(NULL, " ");
        char *bit_str = strtok(NULL, " ");
        if (title && bit_str)
            cmd_bitmap_mark(title, bit_str);
    } else if (strcmp(cmd, "bitmap_all") == 0) {
        char *title = strtok(NULL, " ");
        char *start_str = strtok(NULL, " ");
        char *cnt_str = strtok(NULL, " ");
        if (title && start_str && cnt_str)
            cmd_bitmap_all(title, start_str, cnt_str);
    } else if (strcmp(cmd, "bitmap_any") == 0) {
        char *title = strtok(NULL, " ");
        char *start_str = strtok(NULL, " ");
        char *cnt_str = strtok(NULL, " ");
        if (title && start_str && cnt_str)
            cmd_bitmap_any(title, start_str, cnt_str);
    } else if (strcmp(cmd, "bitmap_contains") == 0) {
        char *title = strtok(NULL, " ");
        char *start_str = strtok(NULL, " ");
        char *cnt_str = strtok(NULL, " ");
        char *bool_str = strtok(NULL, " ");
        if (title && start_str && cnt_str && bool_str)
            cmd_bitmap_contains(title, start_str, cnt_str, bool_str);
    } else if (strcmp(cmd, "bitmap_count") == 0) {
        char *title = strtok(NULL, " ");
        char *start_str = strtok(NULL, " ");
        char *cnt_str = strtok(NULL, " ");
        char *bool_str = strtok(NULL, " ");
        if (title && start_str && cnt_str && bool_str)
            cmd_bitmap_count(title, start_str, cnt_str, bool_str);
    } else if (strcmp(cmd, "bitmap_dump") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_bitmap_dump(title);
    } else if (strcmp(cmd, "bitmap_expand") == 0) {
        char *title = strtok(NULL, " ");
        char *cnt = strtok(NULL, " ");
        if (title && cnt)
            cmd_bitmap_expand(title, cnt);
    } else if (strcmp(cmd, "bitmap_set") == 0) {
        char *title = strtok(NULL, " ");
        char *idx_str = strtok(NULL, " ");
        char *bool_str = strtok(NULL, " ");
        if (title && idx_str && bool_str)
            cmd_bitmap_set(title, idx_str, bool_str);
    } else if (strcmp(cmd, "bitmap_set_all") == 0) {
        char *title = strtok(NULL, " ");
        char *bool_str = strtok(NULL, " ");
        if (title && bool_str)
            cmd_bitmap_set_all(title, bool_str);
    } else if (strcmp(cmd, "bitmap_flip") == 0) {
        char *title = strtok(NULL, " ");
        char *idx_str = strtok(NULL, " ");
        if (title && idx_str)
            cmd_bitmap_flip(title, idx_str);
    } else if (strcmp(cmd, "bitmap_none") == 0) {
        char *title = strtok(NULL, " ");
        char *start_str = strtok(NULL, " ");
        char *cnt_str = strtok(NULL, " ");
        if (title && start_str && cnt_str)
            cmd_bitmap_none(title, start_str, cnt_str);
    } else if (strcmp(cmd, "bitmap_reset") == 0) {
        char *title = strtok(NULL, " ");
        char *idx_str = strtok(NULL, " ");
        if (title && idx_str)
            cmd_bitmap_reset(title, idx_str);
    } else if (strcmp(cmd, "bitmap_scan_and_flip") == 0) {
        char *title = strtok(NULL, " ");
        char *start_str = strtok(NULL, " ");
        char *cnt_str = strtok(NULL, " ");
        char *bool_str = strtok(NULL, " ");
        if (title && start_str && cnt_str && bool_str)
            cmd_bitmap_scan_and_flip(title, start_str, cnt_str, bool_str);
    } else if (strcmp(cmd, "bitmap_scan") == 0) {
        char *title = strtok(NULL, " ");
        char *start_str = strtok(NULL, " ");
        char *cnt_str = strtok(NULL, " ");
        char *bool_str = strtok(NULL, " ");
        if (title && start_str && cnt_str && bool_str)
            cmd_bitmap_scan(title, start_str, cnt_str, bool_str);
    } else if (strcmp(cmd, "bitmap_set_multiple") == 0) {
        char *title = strtok(NULL, " ");
        char *start_str = strtok(NULL, " ");
        char *cnt_str = strtok(NULL, " ");
        char *bool_str = strtok(NULL, " ");
        if (title && start_str && cnt_str && bool_str)
            cmd_bitmap_set_multiple(title, start_str, cnt_str, bool_str);
    } else if (strcmp(cmd, "bitmap_size") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_bitmap_size(title);
    } else if (strcmp(cmd, "bitmap_test") == 0) {
        char *title = strtok(NULL, " ");
        char *idx_str = strtok(NULL, " ");
        if (title && idx_str)
            cmd_bitmap_test(title, idx_str);
    }
    /* HASH 명령어들 */
    else if (strcmp(cmd, "hash_insert") == 0) {
        char *name = strtok(NULL, " ");
        char *num_str = strtok(NULL, " ");
        if (name && num_str){
            cmd_hash_insert(name, num_str);
        }
    } else if (strcmp(cmd, "hash_size") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_hash_size(title);
    } else if (strcmp(cmd, "hash_apply") == 0) {
        char *title = strtok(NULL, " ");
        char *func = strtok(NULL, " ");
        if (title && func)
            cmd_apply_Hash(title, func);
    } else if (strcmp(cmd, "hash_delete") == 0) {
        char *title = strtok(NULL, " ");
        char *num_str = strtok(NULL, " ");
        if (title && num_str)
            cmd_hash_delete(title, num_str);
    } else if (strcmp(cmd, "hash_empty") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_hash_empty(title);
    } else if (strcmp(cmd, "hash_clear") == 0) {
        char *title = strtok(NULL, " ");
        if (title)
            cmd_hash_clear(title);
    } else if (strcmp(cmd, "hash_find") == 0) {
        char *title = strtok(NULL, " ");
        char *num_str = strtok(NULL, " ");
        if (title && num_str)
            cmd_hash_find(title, num_str);
    } else if (strcmp(cmd, "hash_replace") == 0) {
        char *title = strtok(NULL, " ");
        char *num_str = strtok(NULL, " ");
        if (title && num_str)
            cmd_hash_replace(title, num_str);
    } else {
        printf("error: unknown command %s\n", cmd);
    }
}

int main(void) {
    srand(time(NULL)); // 랜덤 시드 초기화
    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        process_command(line);
    }
    return 0;
}
