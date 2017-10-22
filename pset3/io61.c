#include "io61.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

// io61.c
//    YOUR CODE HERE!


// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

// defines block size, number, starting pos, and offset
#define BLOCK_SIZE 0x1000
//#define BLOCK_SIZE 4096

static inline off_t find_block(off_t off) { 
    return off >> 0xC;
    //return off/BLOCK_SIZE; 
}

static inline off_t find_block_pos(off_t off) {
    return off & (off_t)~0x0FFF;
    //return find_block(off) * BLOCK_SIZE; 
}

static inline int find_block_ofs(off_t off) { 
    return off & (off_t)0x0FFF;
    //return off % BLOCK_SIZE; 
}

// defines slot size, assignment
#define NSLOTS 0x100
//#define NSLOTS 64
static inline int find_slot(off_t off) { 
    return find_block(off) & 0x0FF; 
    //return find_block(off) % NSLOTS;
}


typedef struct io61_slot {
    unsigned char buf[BLOCK_SIZE];
    off_t block; // block number
    off_t pos;   // block pos
    ssize_t sz;  // size of cached data
} io61_slot;

typedef struct io61_file {
    int fd;
    int mode;
    off_t pos; // logical file pos, maybe cached
    io61_slot slots [NSLOTS];
    //int allowseek; // is file seekable
    off_t f_pos; // actual file pos, 
    off_t f_size; // only used in read mode
    int seq_mode;  // sequential mode: 
                   //       -1:unknown, initially default to -1 (if not allowseek, default to 0)
                   //        0: sequential read, after 10 sequential readc, change to 0
                   //        1: not sequential: any io61_seek(), change to not sequencial
    
} io61_file;


// read_slot()
//     read one block data into cached slot 
int read_slot(io61_file* f, int slot_num, off_t block_num) {
    assert(f->mode == O_RDONLY); 
    
    // replace slot with new_block data
    off_t new_block_pos = block_num * BLOCK_SIZE;
    io61_slot* s = &f->slots[slot_num];
    
    switch (f->seq_mode) {
        case -1:
        case 0: // no seek needed, verify position is correct
            //if (new_block_pos != f->f_pos) printf ("%ld %ld \n", new_block_pos, f->f_pos); 
            assert(new_block_pos == f->f_pos);
            break;
        case 1: // io61_seek() requested at least once
            if (f->f_size != -1 && f->pos >= f->f_size)  // file ended
                return EOF;
            if (new_block_pos != f->f_pos) {
                int new_pos = lseek(f->fd, new_block_pos, SEEK_SET);
                assert(new_pos != -1); 
                f->f_pos = new_block_pos;
            }
    }
    ssize_t r = read(f->fd, s->buf, BLOCK_SIZE);
//char x = s->buf[5];  s->buf[5]= 0;printf("wrbuffer %d, %d, [%s] \n", s->pos, s->sz, s->buf);s->buf[5]= x;
//printf ("read buffer(%ld %ld) \n", f->f_pos, r);
//s->buf[r] = 0; printf ("read buffer(%ld %ld) [%s]\n", f->f_pos, r, s->buf);
    if (r == -1 || r == 0)
        return EOF;
    f->f_pos += r; // keep track of real file position
    s->block = block_num;
    s->pos = new_block_pos;
    s->sz = r;
    return r;
}

// flush_slot()
//    flush one cached slot to disk/device
int flush_slot(io61_file* f, int slot_num) {
    assert(f->mode == O_WRONLY); 
        
    io61_slot *s = &f->slots[slot_num];
    if (s->sz > 0 && s->pos >= 0) {
        if (f->seq_mode != 1) {
            //if (s->pos != f->f_pos) printf("%ld %ld\n", s->pos, f->f_pos);
            assert(s->pos == f->f_pos);
        } else {
            if (f->f_pos != s->pos) { 
                int new_pos = lseek(f->fd, s->pos, SEEK_SET);
                assert(new_pos != -1);
                f->f_pos = new_pos;
            }
        }
        ssize_t r = write(f->fd, s->buf, s->sz);
//printf("wrbuffer %ld, %ld \n", s->pos, s->sz);
        if (r <= 0)
            return -1;
        f->f_pos+=r;
        s->block = s->pos = -1;
        s->sz=0;
    }
}



// io61_fdopen(fd, mode)
//    Return a new io61_file for file descriptor `fd`. `mode` is
//    either O_RDONLY for a read-only file or O_WRONLY for a
//    write-only file. You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = (io61_file*) malloc(sizeof(io61_file));
    //printf ("fdopen malloc %p\n", f);
    f->fd = fd;
    f->mode = mode;
    assert(mode == O_RDONLY || mode == O_WRONLY);
    f->pos = 0;
    f->f_pos = 0;
    f->f_size = io61_filesize(f);
    int allowseek = !(lseek(f->fd, 0, SEEK_CUR) == (off_t) -1);
    f->seq_mode = allowseek? -1 : 0;
    for (int i=0; i < NSLOTS; ++i)
        f->slots[i].block = f->slots[i].pos = -1;
    return f;
}


// io61_close(f)
//    Close the io61_file `f` and release all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    //printf ("fdclose free %p\n", f);
    free(f);
    return r;
}


// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.

int io61_readc(io61_file* f) {
    /*unsigned char buf[1];
    if (read(f->fd, buf, 1) == 1) {
        return buf[0];
    } else {
        return EOF;
    }*/
    off_t new_block = find_block(f->pos);
    off_t new_block_pos = find_block_pos(f->pos);
    int new_block_ofs = find_block_ofs(f->pos);
    int slotindex = find_slot(f->pos);
    io61_slot* s = &f->slots[slotindex];

    if (new_block != s->block)
        // replace slot with new_block data
        if (read_slot(f, slotindex, new_block)==EOF)
            return EOF;

    if (new_block_ofs >= s->sz) 
        return EOF;
    ++f->pos;
    return s->buf[new_block_ofs];
}


// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count, which might be zero, if the file ended before `sz` characters
//    could be read. Returns -1 if an error occurred before any characters
//    were read.

ssize_t io61_read(io61_file* f, char* buf, size_t sz) {
    size_t nread;
    //printf ("%zd\n", sz);
    if (f->seq_mode != 1) {
	nread = read(f->fd, buf, sz);
	//printf("loop1 \n");
        return nread;
    }
    else if (sz > BLOCK_SIZE) {
	if (f->seq_mode != 0) {
	    int new_pos = lseek(f->fd, f->pos, SEEK_SET);
            assert(new_pos != -1); 
            f->f_pos = new_pos;
	}
	nread = read(f->fd, buf, sz);
	f->f_pos += nread;
	//printf("loop2\n");
	return nread;
    } else {
        for (nread = 0; nread != sz; ++nread) {
            int ch = io61_readc(f);
            if (ch == EOF)
                break;
            buf[nread] = ch;
        }
	//printf("loop3\n");
        if (nread != 0 || sz == 0 || io61_eof(f)) {
            return nread;
        } 
        else 
            return -1;
    }
/*
    size_t nread = 0;
    while (nread != sz) {
        int ch = io61_readc(f);
        if (ch == EOF) {
            break;
        }
        buf[nread] = ch;
        ++nread;
    }
*/

}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    /*
    unsigned char buf[1];
    buf[0] = ch;
    if (write(f->fd, buf, 1) == 1) {
        return 0;
    }
    else {
        return -1;
    } */
    int slotindex = find_slot(f->pos);
    io61_slot* s = &f->slots[slotindex];
    //s->buf[BLOCK_SIZE+1]=0;printf("[%s]\n",s->buf);
    off_t new_block = find_block(f->pos);
    off_t new_block_pos = find_block_pos(f->pos);
    int new_block_ofs = find_block_ofs(f->pos);
    if (new_block_ofs == 0)
        io61_flush (f);

    if (s->pos == -1) { // empty slot, initialize
        s->block = find_block(f->pos);
        s->pos = find_block_pos(f->pos);
        s->sz = 0;
    }
    //if (f->pos < s->pos || f->pos >= s->pos + s->sz) {
    if (new_block != s->block) {
        // flush cached data before reuse
        if (s->sz && s->pos >= 0) {
            //if (f->allowseek && f->f_pos != s->pos) { // if not allowseek, file only allows seq writing
            if (f->seq_mode != 1)
                assert(s->pos == f->f_pos);
            else 
                if (f->f_pos != s->pos) { 
                    int new_pos = lseek(f->fd, s->pos, SEEK_SET);
                    //if (s->pos == 0)
                        //printf ("write():lseek %d %d\n", s->pos, new_pos);
                    assert(new_pos != -1);
                    f->f_pos = new_pos;
                }
            ssize_t r = write(f->fd, s->buf, s->sz);
//printf("wrbuffer %ld, %ld \n", s->pos, s->sz);
            if (r <= 0)
                return -1;
            f->f_pos += r; // keep track of real file position
            s->block = new_block;
            s->pos = new_block_pos;
            s->sz = 0;
            memset(s->buf, 0, BLOCK_SIZE); //reset content to be safe for random write
        }
    }
    s->buf[new_block_ofs] = (unsigned char) ch;
    if (new_block_ofs+1 > s->sz)
        s->sz = new_block_ofs+1;
    ++f->pos;
    return 0;
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {
    size_t nwritten;
    if (f->seq_mode != 1) {
    	nwritten = write(f->fd, buf, sz);
    	return nwritten;
    } else {
        if (!sz)
            return 0;
        for (nwritten = 0; nwritten != sz; ++nwritten)
            if (io61_writec(f, buf[nwritten]) == -1) 
                return -1;
        return nwritten;
    }
}

// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {
    //(void)f;
    if (f->mode == O_WRONLY)
        for (int i = 0; i < NSLOTS; i++)
            flush_slot(f, i);

    return 0;
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.
//    any call means no longer sequencial r/w, change cached f->pos only. lseek() delayed ro real r/w of cache

int io61_seek(io61_file* f, off_t pos) {
    if (f->mode==O_RDONLY && f->f_size != -1 && pos > f->f_size)
        return -1;
	else {
        f->pos = pos;
        if (f->seq_mode != 1)
            f->seq_mode = 1;
        return 0;
    } 
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `filename == NULL`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != NULL` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    //printf("io61_open_check [%s] [%d]\n", filename, mode);
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_filesize(f)
//    Return the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}


// io61_eof(f)
//    Test if readable file `f` is at end-of-file. Should only be called
//    immediately after a `read` call that returned 0 or -1.

int io61_eof(io61_file* f) {
    char x;
    ssize_t nread = read(f->fd, &x, 1);
    if (nread == 1) {
        fprintf(stderr, "Error: io61_eof called improperly\n\
  (Only call immediately after a read() that returned 0 or -1.)\n");
        abort();
    }
    return nread == 0;
}

