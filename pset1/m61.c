#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

struct m61_statistics mstat = {0, 0, 0, 0, 0, 0, 0, 0};

#define maxsize 100000
void* activeptr[maxsize];
size_t szptr[maxsize];
int nactive = 0;

int addptr (void* ptr, size_t sz){
    if (nactive >= maxsize-1)
	return -1;
    activeptr[nactive] = ptr;
    szptr[nactive] = sz;
    return ++nactive;
}

int findptr (void* ptr){
    int i;
    for (i=0; i<nactive; i++)
	if (activeptr[i] == ptr)
		return i;
    return -1;
}

int remptr (void* ptr){
    int i = findptr(ptr);
    if (i < 0)
	return -1;
        nactive--;
    if (i!=nactive){
	activeptr[i] = activeptr[nactive];
	szptr[i] = szptr[nactive];
    }
    return i;
}
inline size_t tbl_sz_of_ptr (int idx) { return szptr[idx]; }

// memset(mstat, 0, sizeof(mstat));

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    either return NULL or a unique, newly-allocated pointer value.
///    The allocation request was at location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    void* p = base_malloc(sz);
    if (p) {
	mstat.nactive++;
	mstat.active_size += sz;
	mstat.ntotal++;
	mstat.total_size = mstat.total_size + sz;
        if (!mstat.heap_min || mstat.heap_min > p) {
            mstat.heap_min = p;
        }
        if (!mstat.heap_max || mstat.heap_max < p + sz) {
            mstat.heap_max = p + sz;
        }
	addptr (p, sz);
    }
    
    else {
	mstat.nfail++;
	mstat.fail_size += sz;
	return p;
    }
    return p;
}


/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc and friends. If
///    `ptr == NULL`, does nothing. The free was called at location
///    `file`:`line`.

void m61_free(void *ptr, const char *file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    int i;
    i = findptr(ptr);
    if (ptr >= 0 && ptr) {
	mstat.nactive--;
	mstat.active_size -= szptr[i];
	base_free(ptr);
	remptr(ptr);
    }
}


/// m61_realloc(ptr, sz, file, line)
///    Reallocate the dynamic memory pointed to by `ptr` to hold at least
///    `sz` bytes, returning a pointer to the new block. If `ptr` is NULL,
///    behaves like `m61_malloc(sz, file, line)`. If `sz` is 0, behaves
///    like `m61_free(ptr, file, line)`. The allocation request was at
///    location `file`:`line`.

void* m61_realloc(void* ptr, size_t sz, const char* file, int line) {
    void* new_ptr = NULL;
    if (sz) {
        new_ptr = m61_malloc(sz, file, line);
    }
    if (ptr && new_ptr) {
        // Copy the data from `ptr` into `new_ptr`.
        // To do that, we must figure out the size of allocation `ptr`.
        // Your code here (to fix test014).
    }
    m61_free(ptr, file, line);
    return new_ptr;
}


/// m61_calloc(nmemb, sz, file, line)
///    Return a pointer to newly-allocated dynamic memory big enough to
///    hold an array of `nmemb` elements of `sz` bytes each. The memory
///    is initialized to zero. If `sz == 0`, then m61_malloc may
///    either return NULL or a unique, newly-allocated pointer value.
///    The allocation request was at location `file`:`line`.

void* m61_calloc(size_t nmemb, size_t sz, const char* file, int line) {
    // Your code here (to fix test016).
    void* ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr) {
        memset(ptr, 0, nmemb * sz);
    }
    return ptr;
}


/// m61_getstatistics(stats)
///    Store the current memory statistics in `*stats`.

void m61_getstatistics(struct m61_statistics* stats) {
    *stats = mstat;
}


/// m61_printstatistics()
///    Print the current memory statistics.

void m61_printstatistics(void) {
    struct m61_statistics stats;
    m61_getstatistics(&stats);

    printf("malloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("malloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_printleakreport()
///    Print a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_printleakreport(void) {
    // Your code here.
}
