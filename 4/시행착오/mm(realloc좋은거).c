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


/* 기본 상수 및 매크로 */
#define WSIZE       8       /* Word and header/footer size (bytes) */
#define DSIZE       16      /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* 힙 확장을 위한 기본 크기 (4KB) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* 크기와 할당 비트를 통합해서 header/footer에 저장할 값 생성 */
#define PACK(size, alloc)  ((size) | (alloc))

/* 주소 p에서 word 읽기/쓰기 */
#define GET(p)       (*(unsigned long *)(p))
#define PUT(p, val)  (*(unsigned long *)(p) = (val))

/* 주소 p에서 블록 크기와 할당 상태 읽기 */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* 블록 포인터 bp를 이용해 header와 footer의 주소 계산 */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 블록 포인터 bp를 이용해 다음/이전 블록의 주소 계산 */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* 가용 리스트 내에서 이전/다음 블록 포인터 접근 */
#define PRED_FREE(bp) (*(void**)(bp))
#define SUCC_FREE(bp) (*(void**)(bp + WSIZE))

/* 분리 가용 리스트의 클래스 개수 */
#define NUM_CLASSES 10

/* 전역 변수 */
static void* heap_listp; // 힙의 시작점 (prologue block 바로 뒤)
static void* segregated_lists[NUM_CLASSES]; // 분리 가용 리스트 배열

/* 함수 프로토타입 */
static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);
static void insert_into_list(void* bp);
static void remove_from_list(void* bp);
static int get_class_index(size_t size);

/*
 * mm_init - Malloc 패키지 초기화.
 */
int mm_init(void)
{
    // 분리 가용 리스트 초기화
    for (int i = 0; i < NUM_CLASSES; i++) {
        segregated_lists[i] = NULL;
    }

    // 비어있는 초기 힙 생성
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1)
        return -1;
    PUT(heap_listp, 0);                            /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      /* Epilogue header */
    heap_listp += (2 * WSIZE);

    // 힙을 CHUNKSIZE 만큼 확장하고 초기 가용 블록 생성
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * mm_malloc - 블록 할당.
 */
void* mm_malloc(size_t size)
{
    size_t asize;      /* 조정된 블록 크기 */
    size_t extendsize; /* 힙 확장 크기 */
    char* bp;

    if (size == 0)
        return NULL;

    // 오버헤드와 정렬 요구사항을 고려하여 블록 크기 조정
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE);

    // 적합한 가용 블록 검색
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // 적합한 블록이 없으면 힙 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - 블록 해제.
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
 * mm_realloc - 블록 재할당. 최적화 포함.
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
    size_t new_size = size + DSIZE; // 오버헤드 포함한 새 크기

    if (new_size <= old_size) {
        return old_ptr; // 크기가 충분하면 그대로 반환
    }

    // 최적화: 다음 블록이 가용 상태이고, 합친 크기가 충분한 경우
    void* next_bp = NEXT_BLKP(old_ptr);
    size_t next_alloc = GET_ALLOC(HDRP(next_bp));
    size_t next_size = GET_SIZE(HDRP(next_bp));

    if (!next_alloc && (old_size + next_size >= new_size)) {
        remove_from_list(next_bp); // 다음 블록을 가용 리스트에서 제거
        PUT(HDRP(old_ptr), PACK(old_size + next_size, 1));
        PUT(FTRP(old_ptr), PACK(old_size + next_size, 1));
        return old_ptr; // memcpy 없이 재할당 완료
    }

    // 일반적인 재할당
    new_ptr = mm_malloc(size);
    if (new_ptr == NULL)
        return NULL;

    memcpy(new_ptr, old_ptr, old_size - DSIZE);
    mm_free(old_ptr);
    return new_ptr;
}

/*
 * coalesce - 인접 가용 블록을 통합.
 */
static void* coalesce(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {         /* Case 1: 양쪽 다 할당 */
        // 아무것도 하지 않음
    }
    else if (prev_alloc && !next_alloc) {   /* Case 2: 다음 블록만 가용 */
        remove_from_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {   /* Case 3: 이전 블록만 가용 */
        remove_from_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {                                  /* Case 4: 양쪽 다 가용 */
        remove_from_list(PREV_BLKP(bp));
        remove_from_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // 통합된 새 블록을 적절한 가용 리스트에 추가
    insert_into_list(bp);
    return bp;
}

/*
 * extend_heap - 힙을 확장.
 */
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    // 정렬을 유지하기 위해 짝수 개의 word로 크기 조정
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // 새 가용 블록의 header와 footer 설정
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  /* New epilogue header */

    // 이전 블록이 가용 상태였다면 통합
    return coalesce(bp);
}

/*
 * find_fit - 할당할 가용 블록을 검색.
 */
static void* find_fit(size_t asize)
{
    void* bp;
    int class_idx = get_class_index(asize);

    // 적절한 크기 클래스부터 시작하여 리스트 탐색
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
 * place - 블록을 할당하고, 남는 공간이 있으면 분할.
 */
static void place(void* bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_from_list(bp); // 할당될 것이므로 가용 리스트에서 제거

    if ((csize - asize) >= (2 * DSIZE)) { // 남는 부분이 최소 블록 크기 이상인가?
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_into_list(bp); // 분할된 나머지 블록을 가용 리스트에 추가
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* * get_class_index - 크기에 맞는 클래스 인덱스 반환.
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
 * insert_into_list - 가용 블록을 리스트 맨 앞에 추가 (LIFO).
 */
static void insert_into_list(void* bp) {
    int class_idx = get_class_index(GET_SIZE(HDRP(bp)));
    void* head = segregated_lists[class_idx];

    SUCC_FREE(bp) = head; // 새 블록의 next는 기존의 head
    if (head != NULL) {
        PRED_FREE(head) = bp; // 기존 head의 prev는 새 블록
    }
    PRED_FREE(bp) = NULL; // 새 블록의 prev는 NULL
    segregated_lists[class_idx] = bp; // 리스트의 head를 새 블록으로 변경
}

/*
 * remove_from_list - 가용 리스트에서 블록 제거.
 */
static void remove_from_list(void* bp) {
    int class_idx = get_class_index(GET_SIZE(HDRP(bp)));
    void* prev = PRED_FREE(bp);
    void* next = SUCC_FREE(bp);

    if (prev != NULL) {
        SUCC_FREE(prev) = next;
    }
    else { // 제거하려는 블록이 head인 경우
        segregated_lists[class_idx] = next;
    }

    if (next != NULL) {
        PRED_FREE(next) = prev;
    }
}