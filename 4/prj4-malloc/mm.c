#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// 내 정보 제출전에 작성하기
team_t team = {
    // 학번
    "20211523",
    // 전체 이름
    "JongWon Kim",
    // 이메일 주소
    "kimjong1@sogang.ac.kr",
};

//// 기본 상수 및 매크로
#define WSIZE 4             // 워드 크기 (byte)
#define DSIZE 8             // 더블 워드 크기 (byte)
#define CHUNKSIZE (1<<12)   // 이 크기만큼씩 힙을 확장 (byte)
#define LIST_LIMIT 20       // segregated lists 개수

////매크로 정의
// max 매크로
#define MAX(x, y) ((x) > (y)? (x) : (y))

// size와 alloc(할당할 비트)를 하나의 워드로 묶는 매크로
#define PACK(size, alloc) ((size) | (alloc))

// 포인터p에서 워드를 읽고 쓰는 매크로
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// 포인터p에서 크기와 할당된 필드를 읽음
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// 블록 포인터 bp가 주어졌을 때, header와 footer의 주소를 계산
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 블록 포인터 bp가 주어졌을 때, next 및 prev 블록의 주소 계산
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// free list를 위한 포인터
#define PRED_PTR(bp) (*(char **)(bp))
#define SUCC_PTR(bp) (*(char **)(((char *)(bp)) + WSIZE))


//// 전역 변수
static void** segregated_free_lists;

//// 함수 정의
static void* extend_heap(size_t words);
static void* place(void* bp, size_t asize);
static void* find_fit(size_t asize);
static void* coalesce(void* bp);
static void add_to_free_list(void* bp);
static void remove_from_free_list(void* bp);
static int get_list_index(size_t size);


// mm_init - malloc 초기화
int mm_init(void)
{
    char* heap_listp;

    if ((segregated_free_lists = mem_sbrk(LIST_LIMIT * WSIZE)) == (void*)-1) {
        return -1;
    }

    for (int i = 0; i < LIST_LIMIT; i++) {
        segregated_free_lists[i] = NULL;
    }

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1) {
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    return 0;
}


// mm_malloc - 블록을 할당
void* mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char* bp;

    if (size == 0) {
        return NULL;
    }
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    }
    else {
        asize = DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE);
    }
    if ((bp = find_fit(asize)) != NULL) {
        bp = place(bp, asize); // place 함수 -> 반환된 정확한 포인터로 bp 업데이트함
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    bp = place(bp, asize); // 여기서도 place 함수로 bp 업데이트하기
    return bp;
}


// mm_free - 블록 free해주기
void mm_free(void* ptr)
{
    if (ptr == NULL) {
        return;
    }

    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}


// coalesce - 인접한 free list을 합치기
static void* coalesce(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && !next_alloc) {
        remove_from_free_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {
        remove_from_free_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else if (!prev_alloc && !next_alloc) {
        remove_from_free_list(PREV_BLKP(bp));
        remove_from_free_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    add_to_free_list(bp);
    return bp;
}


// mm_realloc - 메모리 블록을 재할당
void* mm_realloc(void* ptr, size_t size)
{
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t new_size = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

    // 1. 블록 크기를 줄이는 경우
    if (new_size <= old_size) {
        if ((old_size - new_size) >= (2 * DSIZE)) {
            PUT(HDRP(ptr), PACK(new_size, 1));
            PUT(FTRP(ptr), PACK(new_size, 1));
            void* remainder_bp = NEXT_BLKP(ptr);
            PUT(HDRP(remainder_bp), PACK(old_size - new_size, 0));
            PUT(FTRP(remainder_bp), PACK(old_size - new_size, 0));
            coalesce(remainder_bp);
        }
        return ptr;
    }

    // 2. 블록 크기를 늘리는 경우
    void* prev_bp = PREV_BLKP(ptr);
    void* next_bp = NEXT_BLKP(ptr);
    size_t prev_alloc = GET_ALLOC(FTRP(prev_bp));
    size_t next_alloc = GET_ALLOC(HDRP(next_bp));
    size_t current_size = old_size;
    size_t total_size;

    //이전-다음 블록 모두 free인 경우
    if (!prev_alloc && !next_alloc) {
        total_size = current_size + GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp));
        if (total_size >= new_size) {
            remove_from_free_list(prev_bp);
            remove_from_free_list(next_bp);

            memmove(prev_bp, ptr, old_size - DSIZE);
            ptr = prev_bp;

            if ((total_size - new_size) >= (2 * DSIZE)) {
                PUT(HDRP(ptr), PACK(new_size, 1));
                PUT(FTRP(ptr), PACK(new_size, 1));
                void* remainder_bp = NEXT_BLKP(ptr);
                PUT(HDRP(remainder_bp), PACK(total_size - new_size, 0));
                PUT(FTRP(remainder_bp), PACK(total_size - new_size, 0));
                coalesce(remainder_bp);
            }
            else {
                PUT(HDRP(ptr), PACK(total_size, 1));
                PUT(FTRP(ptr), PACK(total_size, 1));
            }
            return ptr;
        }
    }

    //다음 블록만 free인 경우
    if (!next_alloc) {
        total_size = current_size + GET_SIZE(HDRP(next_bp));
        if (total_size >= new_size) {
            remove_from_free_list(next_bp);
            if ((total_size - new_size) >= (2 * DSIZE)) {
                PUT(HDRP(ptr), PACK(new_size, 1));
                PUT(FTRP(ptr), PACK(new_size, 1));
                void* remainder_bp = NEXT_BLKP(ptr);
                PUT(HDRP(remainder_bp), PACK(total_size - new_size, 0));
                PUT(FTRP(remainder_bp), PACK(total_size - new_size, 0));
                coalesce(remainder_bp);
            }
            else {
                PUT(HDRP(ptr), PACK(total_size, 1));
                PUT(FTRP(ptr), PACK(total_size, 1));
            }
            return ptr;
        }
    }

    //이전 블록만 free인 경우
    if (!prev_alloc) {
        total_size = current_size + GET_SIZE(HDRP(prev_bp));
        if (total_size >= new_size) {
            remove_from_free_list(prev_bp);
            memmove(prev_bp, ptr, old_size - DSIZE);
            ptr = prev_bp;

            if ((total_size - new_size) >= (2 * DSIZE)) {
                PUT(HDRP(ptr), PACK(new_size, 1));
                PUT(FTRP(ptr), PACK(new_size, 1));
                void* remainder_bp = NEXT_BLKP(ptr);
                PUT(HDRP(remainder_bp), PACK(total_size - new_size, 0));
                PUT(FTRP(remainder_bp), PACK(total_size - new_size, 0));
                coalesce(remainder_bp);
            }
            else {
                PUT(HDRP(ptr), PACK(total_size, 1));
                PUT(FTRP(ptr), PACK(total_size, 1));
            }
            return ptr;
        }
    }

    //블록이 힙의 끝에 있는 경우
    if (next_alloc && GET_SIZE(HDRP(next_bp)) == 0) {
        size_t extend_size = new_size - old_size;
        if (mem_sbrk(extend_size) == (void*)-1) {
            return NULL; // 힙 확장 실패
        }
        // 새로운 블록의 header와 footer 업데이트
        PUT(HDRP(ptr), PACK(old_size + extend_size, 1));
        PUT(FTRP(ptr), PACK(old_size + extend_size, 1));
        return ptr;
    }

    //3. 새 블록을 할당--------util 낮아지는 부분임-------
    void* new_ptr = mm_malloc(size);
    if (new_ptr == NULL) return NULL;
    memcpy(new_ptr, ptr, old_size - DSIZE);
    mm_free(ptr);
    return new_ptr;
}


// extend_heap - 새 free 블록으로 힙을 확장한다
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}


// place - 블록을 배치함-> 남은 공간이 어느정도 크면 분할도 함
// 할당된 블록의 포인터를 반환한다
static void* place(void* bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_from_free_list(bp);
    void* allocated_bp = bp;

    if ((csize - asize) >= (2 * DSIZE)) {
        if (asize < 96) {
            // 블록의 끝 부분에 할당
            PUT(HDRP(bp), PACK(csize - asize, 0));
            PUT(FTRP(bp), PACK(csize - asize, 0));
            add_to_free_list(bp);

            allocated_bp = NEXT_BLKP(bp);
            PUT(HDRP(allocated_bp), PACK(asize, 1));
            PUT(FTRP(allocated_bp), PACK(asize, 1));
        }
        else {
            // 블록의 시작 부분에 할당
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
            void* free_part_bp = NEXT_BLKP(bp);
            PUT(HDRP(free_part_bp), PACK(csize - asize, 0));
            PUT(FTRP(free_part_bp), PACK(csize - asize, 0));
            coalesce(free_part_bp);
        }
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    return allocated_bp;
}


// find_fit - best-fit 탐색으로 블록을 찾기
// --util 위해
static void* find_fit(size_t asize)
{
    void* bp;
    void* best_fit = NULL;
    int index = get_list_index(asize);

    for (int i = index; i < LIST_LIMIT; i++) {
        for (bp = segregated_free_lists[i]; bp != NULL; bp = SUCC_PTR(bp)) {
            size_t current_size = GET_SIZE(HDRP(bp));
            if (asize <= current_size) {
                if (best_fit == NULL || current_size < GET_SIZE(HDRP(best_fit))) {
                    best_fit = bp;
                }
            }
        }
        if (best_fit != NULL) {
            return best_fit;
        }
    }
    return NULL;
}


// add_to_free_list - 포인터 주소 순서에 블록을 추가
static void add_to_free_list(void* bp)
{
    int index = get_list_index(GET_SIZE(HDRP(bp)));
    void* current = segregated_free_lists[index];
    void* prev = NULL;

    // 삽입할 위치 찾기
    while (current != NULL && current < bp) {
        prev = current;
        current = SUCC_PTR(current);
    }

    if (prev == NULL) { // 맨 앞에 삽입
        SUCC_PTR(bp) = current;
        if (current != NULL) {
            PRED_PTR(current) = bp;
        }
        PRED_PTR(bp) = NULL;
        segregated_free_lists[index] = bp;
    }
    else { // 중간이나 끝에 삽입
        SUCC_PTR(prev) = bp;
        PRED_PTR(bp) = prev;
        SUCC_PTR(bp) = current;
        if (current != NULL) {
            PRED_PTR(current) = bp;
        }
    }
}


// remove_from_free_list - 블록을 제거
static void remove_from_free_list(void* bp)
{
    int index = get_list_index(GET_SIZE(HDRP(bp)));

    if (PRED_PTR(bp)) {
        SUCC_PTR(PRED_PTR(bp)) = SUCC_PTR(bp);
    }
    else {
        segregated_free_lists[index] = SUCC_PTR(bp);
    }

    if (SUCC_PTR(bp) != NULL) {
        PRED_PTR(SUCC_PTR(bp)) = PRED_PTR(bp);
    }
}


// get_list_index - 주어진 크기에 대한 인덱스 얻기
static int get_list_index(size_t size) {
    int index = 0;
    size_t current_size = 16;

    while (index < LIST_LIMIT - 1) {
        if (size <= current_size) {
            return index;
        }
        current_size <<= 1;
        index++;
    }

    return LIST_LIMIT - 1;
}