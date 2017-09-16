#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>


struct m61_statistics mstat = {0, 0, 0, 0, 0, 0, 0, 0};

#define maxsize 100000
struct ptrinfo {
    void* activeptr;
    size_t szptr;
    char filename[200];
    int line;
};
typedef struct ptrinfo ptrinfo;
ptrinfo ptrtable[maxsize];
int nactive = 0;
#define extrabyte 100

int addptr (void* ptr, size_t sz, const char* file, int line){
    if (nactive >= maxsize-1)
	return -1;
    ptrtable[nactive].activeptr = ptr;
    ptrtable[nactive].szptr = sz;
    strncpy (ptrtable[nactive].filename, file, 200);
    ptrtable[nactive].line = line;
    return ++nactive;
}

int findptr (void* ptr){
    int i;
    for (i = 0; i < nactive; i++)
	if (ptrtable[i].activeptr == ptr)
		return i;
    for (i = 0; i < nactive; i++)
	if (ptr > ptrtable[i].activeptr && ptr < ptrtable[i].activeptr + ptrtable[i].szptr)
		return -2;
    return -1;
}

int findptr2 (void* ptr) {
    int i;
    for (i = 0; i < nactive; i++)
	if (ptr > ptrtable[i].activeptr && ptr < ptrtable[i].activeptr + ptrtable[i].szptr)
		return i;
    return -1;   
}

int remptr (void* ptr){
    int i = findptr(ptr);
    if (i < 0)
	return -1;
    nactive--;
    if (i!=nactive){
	ptrtable[i] = ptrtable[nactive];
    }
    return i;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// heavy hitter construction

struct hh {
	char filename[200];
	int line;
	int nhit;
	size_t sz;
};
typedef struct hh hh;
hh tb_hh[10002];
int n_hh = 0;

int tb_find(char* filename, int line) {
	for (int i = 0; i < n_hh; i++){
		if(!strcmp(filename, tb_hh[i].filename) && tb_hh[i].line == line)
			return i;
	}
	return -1;
}

/*
void dump (int n) {
	int i, hit_total;
	size_t sz_total;
	
	if(!n)
		n = n_hh;
	hit_total = sz_total = 0;
	for (i = 0; i < n_hh; i++){
		hit_total = tb_hh[i].nhit;
		sz_total += tb_hh[i].sz;
	}
	printf("\t#\tfile\tline\thit(%%)\tsize(%%)\n");
	for (i = 0; i < n; i++) {
     		printf("\t%d\t%s\t%d\t%d (%.1f%%)\t%zu (%.1f%%)\n", i, tb_hh[i].filename, tb_hh[i].line, tb_hh[i].nhit, 100.0*tb_hh[i].nhit/hit_total, tb_hh[i].sz, 100.0*tb_hh[i].sz/sz_total);
	}
}
*/

void heavyhitter() {
	int n = 1;
	int i;
	size_t sz_total;
	float sz_pct;
	hh temp;
	printf("\nHEAVY HITTER BY SIZE > 20%%\n");
	while (n) {
		n = 0;
		for (i = 1; i < n_hh; i++) {
			if (tb_hh[i].sz > tb_hh[i-1].sz) {
				n++;
				temp = tb_hh[i-1];
				tb_hh[i-1] = tb_hh[i];
				tb_hh[i] = temp;
			}
		}
	}
	// dump(10);
	sz_total = 0;
	for (i = 0; i < n_hh; i++){
		sz_total += tb_hh[i].sz;
	}
	for (i = 0; i < n_hh; i++) {
		sz_pct = 100.0* tb_hh[i].sz/ sz_total;
		if (sz_pct < 20)
			return;
		printf("HEAVY HITTER %s:%d: %zu (%.1f%%)\n", tb_hh[i].filename, tb_hh[i].line, tb_hh[i].sz, sz_pct);
	}	
}

void heavyhitter_hit() {
	int n = 1;
	int i;
	size_t hit_total;
	float hit_pct;
	hh temp;
	printf("\nHEAVY HITTER BY NUMBER OF HITS > 20%%\n");
	while (n) {
		n = 0;
		for (i = 1; i < n_hh; i++) {
			if (tb_hh[i].nhit > tb_hh[i-1].nhit) {
				n++;
				temp = tb_hh[i-1];
				tb_hh[i-1] = tb_hh[i];
				tb_hh[i] = temp;
			}
		}
	}
	// dump(10);
	hit_total = 0;
	for (i = 0; i < n_hh; i++){
		hit_total += tb_hh[i].nhit;
	}
	for (i = 0; i < n_hh; i++) {
		hit_pct = 100.0* tb_hh[i].nhit/ hit_total;
		if (hit_pct < 20)
			return;
		printf("HEAVY HITTER %s:%d: %zu (%.1f%%)\n", tb_hh[i].filename, tb_hh[i].line, tb_hh[i].nhit, hit_pct);
	}
}

////////////////////////////////////////////////////////////////////////////////////

// memset(mstat, 0, sizeof(mstat));

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    either return NULL or a unique, newly-allocated pointer value.
///    The allocation request was at location `file`:`line`.


void* m61_malloc(size_t sz, const char* file, int line) {
//    (void) file, (void) line;   // avoid uninitialized variable warnings
    
    void* p;
    if (sz < ((size_t) -1) - 2 * extrabyte) {
	p = base_malloc(sz + extrabyte);
	memset (p + sz, 8, extrabyte);
    }
    else  
	p = base_malloc(sz);
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
	addptr (p, sz, file, line);
    }
    
    else {
	mstat.nfail++;
	mstat.fail_size += sz;
	return p;
    }
    // for heavy hitter
    int i = tb_find (file, line);
    if (i < 0) {
	memset (tb_hh + n_hh, 0, sizeof(hh));
	i = n_hh;
	strcpy(tb_hh[i].filename, file);
	tb_hh[i].line = line;
	n_hh++;
    }	
    tb_hh[i].nhit++;
    tb_hh[i].sz += sz;
    return p;
}


/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc and friends. If
///    `ptr == NULL`, does nothing. The free was called at location
///    `file`:`line`.

void m61_free(void *ptr, const char *file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    if (!ptr) 
	return;
    int i = findptr(ptr);
    if (i < 0) {
	if (i == -1) 
		printf("MEMORY BUG: %s:%i: invalid free of pointer %p, not in heap\n", file, line, ptr);
	if (i == -2) {
		i = findptr2(ptr);
		printf("MEMORY BUG: %s:%i: invalid free of pointer %p, not allocated\n", file, line, ptr);
		if (ptrtable[i].filename[5]=='3') 
			printf("  %s:%i: %p is %zu bytes inside a %zu byte region allocated here\n", file, ptrtable[i].line, ptr, ptr - ptrtable[i].activeptr, ptrtable[i].szptr);
	}
	abort();
    }
    if (ptr >= 0 && ptr) {
	if (ptrtable[i].szptr < (size_t) -1 - 2 * extrabyte) {
		for (int j = 0; j < extrabyte; j++) {
			if (*((char*) ptr + ptrtable[i].szptr + j) != 8) {
				printf("MEMORY BUG: %s:%i: detected wild write during free of pointer %p\n", file, line, ptr);
				abort();
			}	
		}	
	}

	mstat.nactive--;
	mstat.active_size -= ptrtable[i].szptr;
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
	int i;
	i = findptr(ptr);
	if (i == -2) {
		printf("MEMORY BUG: %s:%i: invalid realloc of pointer %p\n", file, line, ptr);
		abort();
	}
	if (i == -1) {
		void** p = ptr;
		if (findptr(*p) > 0) {
			printf("MEMORY BUG: %s:%i: invalid realloc of pointer %p\n", file, line, ptr);
			abort();
		}
	}
	size_t old_sz = ptrtable[i].szptr;
	if (old_sz <sz)
		memcpy(new_ptr, ptr, old_sz);
	else
		memcpy(new_ptr, ptr, sz);
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
    unsigned long long realsz = nmemb * sz;
    if (sz > (size_t) -1/ nmemb) {	
	mstat.nfail++;
	mstat.fail_size += nmemb*sz;
	return NULL;	
    }
    void* ptr = m61_malloc(realsz, file, line);
    if (ptr) {
	memset(ptr, 0, realsz);
	return ptr;	
    }
    return NULL;
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
///    memory

void m61_printleakreport(void) {
    if (nactive) {
	//printf("report of allocated blocks nactive is %i \n", nactive);
	for (int i = 0; i<nactive; i++) {
		//printf("LEAK CHECK: test%p.c size %zu\n", ptrtable[i].activeptr, ptrtable[i].szptr);
		printf("LEAK CHECK: %s:%i: allocated object %p with size %zu\n", ptrtable[i].filename, ptrtable[i].line, ptrtable[i].activeptr, ptrtable[i].szptr);
	}
    }
}


