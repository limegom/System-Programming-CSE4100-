#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "20211523",
    /* First member's full name */
    "JongWon Kim",
    /* First member's email address */
    "kimjong1@sogang.ac.kr"
};


/* �⺻ ��� �� ��ũ�� */
#define WSIZE       8       /* Word and header/footer size (bytes) */
#define DSIZE       16      /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* �� Ȯ���� ���� �⺻ ũ�� (4KB) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* ũ��� �Ҵ� ��Ʈ�� �����ؼ� header/footer�� ������ �� ���� */
#define PACK(size, alloc)  ((size) | (alloc))

/* �ּ� p���� word �б�/���� */
#define GET(p)       (*(unsigned long *)(p))
#define PUT(p, val)  (*(unsigned long *)(p) = (val))

/* �ּ� p���� ��� ũ��� �Ҵ� ���� �б� */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* ��� ������ bp�� �̿��� header�� footer�� �ּ� ��� */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* ��� ������ bp�� �̿��� ����/���� ����� �ּ� ��� */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* ���� ����Ʈ ������ ����/���� ��� ������ ���� */
#define PRED_FREE(bp) (*(void**)(bp))
#define SUCC_FREE(bp) (*(void**)(bp + WSIZE))

/* �и� ���� ����Ʈ�� Ŭ���� ���� */
#define NUM_CLASSES 10

/* ���� ���� */
static void* heap_listp; // ���� ������ (prologue block �ٷ� ��)
static void* segregated_lists[NUM_CLASSES]; // �и� ���� ����Ʈ �迭

/* �Լ� ������Ÿ�� */
static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);
static void insert_into_list(void* bp);
static void remove_from_list(void* bp);
static int get_class_index(size_t size);

/*
 * mm_init - Malloc ��Ű�� �ʱ�ȭ.
 */
int mm_init(void)
{
    // �и� ���� ����Ʈ �ʱ�ȭ
    for (int i = 0; i < NUM_CLASSES; i++) {
        segregated_lists[i] = NULL;
    }

    // ����ִ� �ʱ� �� ����
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1)
        return -1;
    PUT(heap_listp, 0);                            /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      /* Epilogue header */
    heap_listp += (2 * WSIZE);

    // ���� CHUNKSIZE ��ŭ Ȯ���ϰ� �ʱ� ���� ��� ����
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * mm_malloc - ��� �Ҵ�.
 */
void* mm_malloc(size_t size)
{
    size_t asize;      /* ������ ��� ũ�� */
    size_t extendsize; /* �� Ȯ�� ũ�� */
    char* bp;

    if (size == 0)
        return NULL;

    // �������� ���� �䱸������ ����Ͽ� ��� ũ�� ����
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE);

    // ������ ���� ��� �˻�
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // ������ ����� ������ �� Ȯ��
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - ��� ����.
 */
void mm_free(void* bp)
{
    if (bp == NULL)
        return;

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - ��� ���Ҵ�. ����ȭ ����.
 */
void* mm_realloc(void* ptr, size_t size)
{
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    void* old_ptr = ptr;
    void* new_ptr;
    size_t old_size = GET_SIZE(HDRP(old_ptr));
    size_t new_size = size + DSIZE; // ������� ������ �� ũ��

    if (new_size <= old_size) {
        return old_ptr; // ũ�Ⱑ ����ϸ� �״�� ��ȯ
    }

    // ����ȭ: ���� ����� ���� �����̰�, ��ģ ũ�Ⱑ ����� ���
    void* next_bp = NEXT_BLKP(old_ptr);
    size_t next_alloc = GET_ALLOC(HDRP(next_bp));
    size_t next_size = GET_SIZE(HDRP(next_bp));

    if (!next_alloc && (old_size + next_size >= new_size)) {
        remove_from_list(next_bp); // ���� ����� ���� ����Ʈ���� ����
        PUT(HDRP(old_ptr), PACK(old_size + next_size, 1));
        PUT(FTRP(old_ptr), PACK(old_size + next_size, 1));
        return old_ptr; // memcpy ���� ���Ҵ� �Ϸ�
    }

    // �Ϲ����� ���Ҵ�
    new_ptr = mm_malloc(size);
    if (new_ptr == NULL)
        return NULL;

    memcpy(new_ptr, old_ptr, old_size - DSIZE);
    mm_free(old_ptr);
    return new_ptr;
}

/*
 * coalesce - ���� ���� ����� ����.
 */
static void* coalesce(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {         /* Case 1: ���� �� �Ҵ� */
        // �ƹ��͵� ���� ����
    }
    else if (prev_alloc && !next_alloc) {   /* Case 2: ���� ��ϸ� ���� */
        remove_from_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {   /* Case 3: ���� ��ϸ� ���� */
        remove_from_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {                                  /* Case 4: ���� �� ���� */
        remove_from_list(PREV_BLKP(bp));
        remove_from_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // ���յ� �� ����� ������ ���� ����Ʈ�� �߰�
    insert_into_list(bp);
    return bp;
}

/*
 * extend_heap - ���� Ȯ��.
 */
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    // ������ �����ϱ� ���� ¦�� ���� word�� ũ�� ����
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // �� ���� ����� header�� footer ����
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  /* New epilogue header */

    // ���� ����� ���� ���¿��ٸ� ����
    return coalesce(bp);
}

/*
 * find_fit - �Ҵ��� ���� ����� �˻�.
 */
static void* find_fit(size_t asize)
{
    void* bp;
    int class_idx = get_class_index(asize);

    // ������ ũ�� Ŭ�������� �����Ͽ� ����Ʈ Ž��
    for (int i = class_idx; i < NUM_CLASSES; i++) {
        for (bp = segregated_lists[i]; bp != NULL; bp = SUCC_FREE(bp)) {
            if (asize <= GET_SIZE(HDRP(bp))) {
                return bp;
            }
        }
    }
    return NULL; // No fit
}

/*
 * place - ����� �Ҵ��ϰ�, ���� ������ ������ ����.
 */
static void place(void* bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_from_list(bp); // �Ҵ�� ���̹Ƿ� ���� ����Ʈ���� ����

    if ((csize - asize) >= (2 * DSIZE)) { // ���� �κ��� �ּ� ��� ũ�� �̻��ΰ�?
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_into_list(bp); // ���ҵ� ������ ����� ���� ����Ʈ�� �߰�
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* * get_class_index - ũ�⿡ �´� Ŭ���� �ε��� ��ȯ.
 */
static int get_class_index(size_t size) {
    if (size <= 16) return 0;
    if (size <= 32) return 1;
    if (size <= 64) return 2;
    if (size <= 128) return 3;
    if (size <= 256) return 4;
    if (size <= 512) return 5;
    if (size <= 1024) return 6;
    if (size <= 2048) return 7;
    if (size <= 4096) return 8;
    return 9;
}

/*
 * insert_into_list - ���� ����� ����Ʈ �� �տ� �߰� (LIFO).
 */
static void insert_into_list(void* bp) {
    int class_idx = get_class_index(GET_SIZE(HDRP(bp)));
    void* head = segregated_lists[class_idx];

    SUCC_FREE(bp) = head; // �� ����� next�� ������ head
    if (head != NULL) {
        PRED_FREE(head) = bp; // ���� head�� prev�� �� ���
    }
    PRED_FREE(bp) = NULL; // �� ����� prev�� NULL
    segregated_lists[class_idx] = bp; // ����Ʈ�� head�� �� ������� ����
}

/*
 * remove_from_list - ���� ����Ʈ���� ��� ����.
 */
static void remove_from_list(void* bp) {
    int class_idx = get_class_index(GET_SIZE(HDRP(bp)));
    void* prev = PRED_FREE(bp);
    void* next = SUCC_FREE(bp);

    if (prev != NULL) {
        SUCC_FREE(prev) = next;
    }
    else { // �����Ϸ��� ����� head�� ���
        segregated_lists[class_idx] = next;
    }

    if (next != NULL) {
        PRED_FREE(next) = prev;
    }
}