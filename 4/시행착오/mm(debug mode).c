/*
 * mm-explicit.c - A high-performance malloc package using an explicit free list,
 * boundary tag coalescing, and a first-fit placement policy. Realloc is
 * optimized to reuse existing blocks where possible, including shrinking blocks.
 *
 * Block Structure:
 *
 * Allocated Block:
 * | Header (4 bytes) | Payload & Padding... | Footer (4 bytes) |
 * Header/Footer: | 31-bit size | 1-bit alloc status |
 *
 * Free Block:
 * | Header (4 bytes) | Prev Free Ptr (4) | Next Free Ptr (4) | ... | Footer (4 bytes) |
 * Free blocks use their payload area to store pointers to the previous and next
 * blocks in the explicit free list.
 *
 * Free List Management:
 * The explicit free list is managed as a LIFO (Last-In-First-Out) doubly-linked
 * list. New free blocks are inserted at the front of the list.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

 /*********************************************************
  * NOTE TO STUDENTS: Before you do anything else, please
  * provide your information in the following struct.
  ********************************************************/
team_t team = {
    /* Your student ID */
    "20211523",
    /* Your full name*/
    "JongWon Kim",
    /* Your email address */
    "kimjong1@sogang.ac.kr",
};

/* Enable for debugging */
#define DEBUG

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */

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

/* Pointers for explicit free list (in the payload of free blocks) */
#define PRED_PTR(bp) (*(char **)(bp))
#define SUCC_PTR(bp) (*(char **)(((char *)(bp)) + WSIZE))


/* Global variables */
static char* heap_listp = 0;  /* Pointer to first block */
static char* free_listp = 0; /* Pointer to first free block */

/* Function prototypes for internal helper routines */
static void* extend_heap(size_t words);
static void place(void* bp, size_t asize);
static void* find_fit(size_t asize);
static void* coalesce(void* bp);
static void add_to_free_list(void* bp);
static void remove_from_free_list(void* bp);
int mm_check(void);

/*
 * mm_init - Initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1)
        return -1;

    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2 * WSIZE);
    free_listp = NULL; /* Initialize free list */

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

#ifdef DEBUG
    mm_check();
#endif
    return 0;
}

/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
void* mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char* bp;

    if (heap_listp == 0) {
        mm_init();
    }

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
#ifdef DEBUG
        mm_check();
#endif
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
#ifdef DEBUG
    mm_check();
#endif
    return bp;
}

/*
 * mm_free - Free a block
 */
void mm_free(void* ptr)
{
    if (ptr == NULL) return;

    size_t size = GET_SIZE(HDRP(ptr));
    if (heap_listp == 0) {
        mm_init();
    }

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);

#ifdef DEBUG
    mm_check();
#endif
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void* coalesce(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
        add_to_free_list(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        remove_from_free_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        remove_from_free_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                     /* Case 4 */
        remove_from_free_list(PREV_BLKP(bp));
        remove_from_free_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    add_to_free_list(bp);
    return bp;
}

/*
 * mm_realloc - Reallocate a block with optimized shrinking and expanding
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

    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t new_size = size <= DSIZE ? 2 * DSIZE : DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

    /* Case 1: Shrinking the block */
    if (new_size <= old_size) {
        size_t diff = old_size - new_size;
        if (diff >= (2 * DSIZE)) { // If remainder is large enough, split it
            PUT(HDRP(ptr), PACK(new_size, 1));
            PUT(FTRP(ptr), PACK(new_size, 1));
            void* new_free_bp = NEXT_BLKP(ptr);
            PUT(HDRP(new_free_bp), PACK(diff, 0));
            PUT(FTRP(new_free_bp), PACK(diff, 0));
            coalesce(new_free_bp);
        }
        // If remainder is too small, do nothing and return original block
        return ptr;
    }
    /* Case 2: Expanding the block */
    else {
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
        size_t available_size = old_size + GET_SIZE(HDRP(NEXT_BLKP(ptr)));

        // Subcase 2.1: Expand in-place if next block is free and large enough
        if (!next_alloc && available_size >= new_size) {
            remove_from_free_list(NEXT_BLKP(ptr));

            size_t remainder = available_size - new_size;
            if (remainder >= (2 * DSIZE)) { // Split if possible
                PUT(HDRP(ptr), PACK(new_size, 1));
                PUT(FTRP(ptr), PACK(new_size, 1));
                void* new_free_bp = NEXT_BLKP(ptr);
                PUT(HDRP(new_free_bp), PACK(remainder, 0));
                PUT(FTRP(new_free_bp), PACK(remainder, 0));
                coalesce(new_free_bp);
            }
            else { // Use the whole coalesced block
                PUT(HDRP(ptr), PACK(available_size, 1));
                PUT(FTRP(ptr), PACK(available_size, 1));
            }
            return ptr;
        }
        // Subcase 2.2: Malloc new block, copy data, and free old block
        else {
            void* new_ptr = mm_malloc(size); // request original size, not aligned
            if (new_ptr == NULL) {
                return NULL;
            }
            memcpy(new_ptr, ptr, old_size - DSIZE); // copy only payload
            mm_free(ptr);
            return new_ptr;
        }
    }
}


/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * place - Place block of asize bytes at start of free block bp
 * and split if remainder would be at least minimum block size
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
 * find_fit - Find a fit for a block with asize bytes
 */
static void* find_fit(size_t asize)
{
    /* First-fit search */
    void* bp;

    for (bp = free_listp; bp != NULL; bp = SUCC_PTR(bp)) {
        if (asize <= GET_SIZE(HDRP(bp))) {
            return bp;
        }
    }
    return NULL; /* No fit */
}

/*
 * add_to_free_list - Adds block to the front of the free list.
 */
static void add_to_free_list(void* bp)
{
    if (free_listp == NULL) { // list is empty
        PRED_PTR(bp) = NULL;
        SUCC_PTR(bp) = NULL;
        free_listp = bp;
    }
    else {
        PRED_PTR(bp) = NULL;
        SUCC_PTR(bp) = free_listp;
        PRED_PTR(free_listp) = bp;
        free_listp = bp;
    }
}

/*
 * remove_from_free_list - Removes block from the free list.
 */
static void remove_from_free_list(void* bp)
{
    if (PRED_PTR(bp) == NULL) { // First item
        free_listp = SUCC_PTR(bp);
    }
    else {
        SUCC_PTR(PRED_PTR(bp)) = SUCC_PTR(bp);
    }

    if (SUCC_PTR(bp) != NULL) {
        PRED_PTR(SUCC_PTR(bp)) = PRED_PTR(bp);
    }
}


/*
 * mm_check - Heap consistency checker.
 * Checks the heap for correctness.
 */
int mm_check(void) {
    char* bp;
    int free_blocks_in_list = 0;
    int free_blocks_in_heap = 0;

    // 1. Is every block in the free list marked as free?
    for (bp = free_listp; bp != NULL; bp = SUCC_PTR(bp)) {
        free_blocks_in_list++;
        if (GET_ALLOC(HDRP(bp))) {
            printf("Error: Allocated block %p in free list.\n", bp);
            return 0;
        }
    }

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        // 2. Are there any contiguous free blocks that somehow escaped coalescing?
        if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
            printf("Error: Contiguous free blocks %p and %p not coalesced.\n", bp, NEXT_BLKP(bp));
            return 0;
        }

        // 3. Do pointers in a heap block point to valid heap addresses?
        if ((bp < (char*)mem_heap_lo()) || (bp > (char*)mem_heap_hi())) {
            printf("Error: block %p is outside of heap bounds.\n", bp);
            return 0;
        }

        // 4. Is every free block actually in the free list?
        if (!GET_ALLOC(HDRP(bp))) {
            free_blocks_in_heap++;
            char* temp;
            for (temp = free_listp; temp != NULL; temp = SUCC_PTR(temp)) {
                if (temp == bp) break;
            }
            if (temp == NULL) {
                printf("Error: Free block %p is not in the free list.\n", bp);
                return 0;
            }
        }
    }

    if (free_blocks_in_heap != free_blocks_in_list) {
        printf("Error: Mismatch between free blocks in heap (%d) and free list (%d).\n", free_blocks_in_heap, free_blocks_in_list);
        return 0;
    }

    return 1;
}
