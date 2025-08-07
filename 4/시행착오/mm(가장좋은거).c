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
    /* Student ID */
    "20211523",
    /* Full name */
    "JongWon Kim",
    /* Email address */
    "kimjong1@sogang.ac.kr",
};

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */
#define LIST_LIMIT 20       /* Number of segregated lists */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Pointers for the free list */
#define PRED_PTR(bp) (*(char **)(bp))
#define SUCC_PTR(bp) (*(char **)(((char *)(bp)) + WSIZE))


/* Global variables */
static void** segregated_free_lists;

/* Function prototypes */
static void* extend_heap(size_t words);
static void place(void* bp, size_t asize);
static void* find_fit(size_t asize);
static void* coalesce(void* bp);
static void add_to_free_list(void* bp);
static void remove_from_free_list(void* bp);
static int get_list_index(size_t size);

/*
 * mm_init - Initializes the malloc package.
 */
int mm_init(void)
{
    char* heap_listp;

    if ((segregated_free_lists = mem_sbrk(LIST_LIMIT * WSIZE)) == (void*)-1) {
        return -1;
    }

    for (int i = 0; i < LIST_LIMIT; i++) {
        segregated_free_lists[i] = NULL;
    }

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block.
 */
void* mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char* bp;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Free a block.
 */
void mm_free(void* ptr)
{
    if (ptr == NULL) return;

    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * coalesce - Merge adjacent free blocks.
 */
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

/*
 * mm_realloc - Reallocate a memory block.
 */
void* mm_realloc(void* ptr, size_t size)
{
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t new_size = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

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

    void* next_bp = NEXT_BLKP(ptr);
    size_t next_alloc = GET_ALLOC(HDRP(next_bp));
    size_t size_next = GET_SIZE(HDRP(next_bp));

    if (!next_alloc && (old_size + size_next) >= new_size) {
        remove_from_free_list(next_bp);
        PUT(HDRP(ptr), PACK(old_size + size_next, 1));
        PUT(FTRP(ptr), PACK(old_size + size_next, 1));
        place(ptr, new_size);
        return ptr;
    }

    if (!next_alloc && size_next == 0) {
        size_t extend_size = new_size - old_size;
        if (mem_sbrk(extend_size) == (void*)-1) return NULL;
        PUT(HDRP(ptr), PACK(old_size + extend_size, 1));
        PUT(FTRP(ptr), PACK(old_size + extend_size, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));
        return ptr;
    }

    void* prev_bp = PREV_BLKP(ptr);
    size_t prev_alloc = GET_ALLOC(FTRP(prev_bp));

    if (!prev_alloc) {
        size_t size_prev = GET_SIZE(HDRP(prev_bp));
        if ((size_prev + old_size) >= new_size) {
            remove_from_free_list(prev_bp);
            PUT(HDRP(prev_bp), PACK(size_prev + old_size, 1));
            memmove(prev_bp, ptr, old_size - DSIZE);
            PUT(FTRP(prev_bp), PACK(size_prev + old_size, 1));
            place(prev_bp, new_size);
            return prev_bp;
        }
    }

    void* new_ptr = mm_malloc(size);
    if (new_ptr == NULL) return NULL;
    memcpy(new_ptr, ptr, old_size - DSIZE);
    mm_free(ptr);
    return new_ptr;
}


/*
 * extend_heap - Extend heap with a new free block.
 */
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

/*
 * place - Place block and split if remainder is large enough.
 */
static void place(void* bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_from_free_list(bp);

    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        coalesce(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * find_fit - Find a fit using best-fit search.
 */
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
        // If we found a best-fit in the current list, we don't need to check larger lists
        // because they will, by definition, be worse fits.
        if (best_fit != NULL) {
            return best_fit;
        }
    }
    return NULL;
}

/*
 * add_to_free_list - Add a block to the front of the appropriate list (LIFO).
 */
static void add_to_free_list(void* bp)
{
    int index = get_list_index(GET_SIZE(HDRP(bp)));
    void* list_head = segregated_free_lists[index];

    SUCC_PTR(bp) = list_head;
    if (list_head != NULL) {
        PRED_PTR(list_head) = bp;
    }
    PRED_PTR(bp) = NULL;
    segregated_free_lists[index] = bp;
}

/*
 * remove_from_free_list - Remove a block from its list.
 */
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

/*
 * get_list_index - Get the index of the list for a given size.
 */
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
