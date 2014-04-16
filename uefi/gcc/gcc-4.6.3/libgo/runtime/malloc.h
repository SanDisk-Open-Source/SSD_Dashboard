// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Memory allocator, based on tcmalloc.
// http://goog-perftools.sourceforge.net/doc/tcmalloc.html

// The main allocator works in runs of pages.
// Small allocation sizes (up to and including 32 kB) are
// rounded to one of about 100 size classes, each of which
// has its own free list of objects of exactly that size.
// Any free page of memory can be split into a set of objects
// of one size class, which are then managed using free list
// allocators.
//
// The allocator's data structures are:
//
//	FixAlloc: a free-list allocator for fixed-size objects,
//		used to manage storage used by the allocator.
//	MHeap: the malloc heap, managed at page (4096-byte) granularity.
//	MSpan: a run of pages managed by the MHeap.
//	MHeapMap: a mapping from page IDs to MSpans.
//	MCentral: a shared free list for a given size class.
//	MCache: a per-thread (in Go, per-M) cache for small objects.
//	MStats: allocation statistics.
//
// Allocating a small object proceeds up a hierarchy of caches:
//
//	1. Round the size up to one of the small size classes
//	   and look in the corresponding MCache free list.
//	   If the list is not empty, allocate an object from it.
//	   This can all be done without acquiring a lock.
//
//	2. If the MCache free list is empty, replenish it by
//	   taking a bunch of objects from the MCentral free list.
//	   Moving a bunch amortizes the cost of acquiring the MCentral lock.
//
//	3. If the MCentral free list is empty, replenish it by
//	   allocating a run of pages from the MHeap and then
//	   chopping that memory into a objects of the given size.
//	   Allocating many objects amortizes the cost of locking
//	   the heap.
//
//	4. If the MHeap is empty or has no page runs large enough,
//	   allocate a new group of pages (at least 1MB) from the
//	   operating system.  Allocating a large run of pages
//	   amortizes the cost of talking to the operating system.
//
// Freeing a small object proceeds up the same hierarchy:
//
//	1. Look up the size class for the object and add it to
//	   the MCache free list.
//
//	2. If the MCache free list is too long or the MCache has
//	   too much memory, return some to the MCentral free lists.
//
//	3. If all the objects in a given span have returned to
//	   the MCentral list, return that span to the page heap.
//
//	4. If the heap has too much memory, return some to the
//	   operating system.
//
//	TODO(rsc): Step 4 is not implemented.
//
// Allocating and freeing a large object uses the page heap
// directly, bypassing the MCache and MCentral free lists.
//
// The small objects on the MCache and MCentral free lists
// may or may not be zeroed.  They are zeroed if and only if
// the second word of the object is zero.  The spans in the
// page heap are always zeroed.  When a span full of objects
// is returned to the page heap, the objects that need to be
// are zeroed first.  There are two main benefits to delaying the
// zeroing this way:
//
//	1. stack frames allocated from the small object lists
//	   can avoid zeroing altogether.
//	2. the cost of zeroing when reusing a small object is
//	   charged to the mutator, not the garbage collector.
//
// This C code was written with an eye toward translating to Go
// in the future.  Methods have the form Type_Method(Type *t, ...).

typedef struct FixAlloc	FixAlloc;
typedef struct MCentral	MCentral;
typedef struct MHeap	MHeap;
typedef struct MHeapMap	MHeapMap;
typedef struct MSpan	MSpan;
typedef struct MStats	MStats;
typedef struct MLink	MLink;

enum
{
	PageShift	= 12,
	PageSize	= 1<<PageShift,
	PageMask	= PageSize - 1,
};
typedef	uintptr	PageID;		// address >> PageShift

enum
{
	// Tunable constants.
	NumSizeClasses = 67,		// Number of size classes (must match msize.c)
	MaxSmallSize = 32<<10,

	FixAllocChunk = 128<<10,	// Chunk size for FixAlloc
	MaxMCacheListLen = 256,		// Maximum objects on MCacheList
	MaxMCacheSize = 2<<20,		// Maximum bytes in one MCache
	MaxMHeapList = 1<<(20 - PageShift),	// Maximum page length for fixed-size list in MHeap.
	HeapAllocChunk = 1<<20,		// Chunk size for heap growth
};

#if __SIZEOF_POINTER__ == 8
#include "mheapmap64.h"
#else
#include "mheapmap32.h"
#endif

// A generic linked list of blocks.  (Typically the block is bigger than sizeof(MLink).)
struct MLink
{
	MLink *next;
};

// SysAlloc obtains a large chunk of zeroed memory from the
// operating system, typically on the order of a hundred kilobytes
// or a megabyte.
//
// SysUnused notifies the operating system that the contents
// of the memory region are no longer needed and can be reused
// for other purposes.  The program reserves the right to start
// accessing those pages in the future.
//
// SysFree returns it unconditionally; this is only used if
// an out-of-memory error has been detected midway through
// an allocation.  It is okay if SysFree is a no-op.

void*	runtime_SysAlloc(uintptr nbytes);
void	runtime_SysFree(void *v, uintptr nbytes);
void	runtime_SysUnused(void *v, uintptr nbytes);
void	runtime_SysMemInit(void);

// FixAlloc is a simple free-list allocator for fixed size objects.
// Malloc uses a FixAlloc wrapped around SysAlloc to manages its
// MCache and MSpan objects.
//
// Memory returned by FixAlloc_Alloc is not zeroed.
// The caller is responsible for locking around FixAlloc calls.
// Callers can keep state in the object but the first word is
// smashed by freeing and reallocating.
struct FixAlloc
{
	uintptr size;
	void *(*alloc)(uintptr);
	void (*first)(void *arg, byte *p);	// called first time p is returned
	void *arg;
	MLink *list;
	byte *chunk;
	uint32 nchunk;
	uintptr inuse;	// in-use bytes now
	uintptr sys;	// bytes obtained from system
};

void	runtime_FixAlloc_Init(FixAlloc *f, uintptr size, void *(*alloc)(uintptr), void (*first)(void*, byte*), void *arg);
void*	runtime_FixAlloc_Alloc(FixAlloc *f);
void	runtime_FixAlloc_Free(FixAlloc *f, void *p);


// Statistics.
// Shared with Go: if you edit this structure, also edit extern.go.
struct MStats
{
	// General statistics.  No locking; approximate.
	uint64	alloc;		// bytes allocated and still in use
	uint64	total_alloc;	// bytes allocated (even if freed)
	uint64	sys;		// bytes obtained from system (should be sum of xxx_sys below)
	uint64	nlookup;	// number of pointer lookups
	uint64	nmalloc;	// number of mallocs
	uint64	nfree;  // number of frees
	
	// Statistics about malloc heap.
	// protected by mheap.Lock
	uint64	heap_alloc;	// bytes allocated and still in use
	uint64	heap_sys;	// bytes obtained from system
	uint64	heap_idle;	// bytes in idle spans
	uint64	heap_inuse;	// bytes in non-idle spans
	uint64	heap_objects;	// total number of allocated objects

	// Statistics about allocation of low-level fixed-size structures.
	// Protected by FixAlloc locks.
	uint64	stacks_inuse;	// bootstrap stacks
	uint64	stacks_sys;
	uint64	mspan_inuse;	// MSpan structures
	uint64	mspan_sys;
	uint64	mcache_inuse;	// MCache structures
	uint64	mcache_sys;
	uint64	heapmap_sys;	// heap map
	uint64	buckhash_sys;	// profiling bucket hash table
	
	// Statistics about garbage collector.
	// Protected by stopping the world during GC.
	uint64	next_gc;	// next GC (in heap_alloc time)
	uint64	pause_total_ns;
	uint64	pause_ns[256];
	uint32	numgc;
	bool	enablegc;
	bool	debuggc;
	
	// Statistics about allocation size classes.
	// No locking; approximate.
	struct {
		uint32 size;
		uint64 nmalloc;
		uint64 nfree;
	} by_size[NumSizeClasses];
};

extern MStats mstats
  __asm__ ("libgo_runtime.runtime.MemStats");


// Size classes.  Computed and initialized by InitSizes.
//
// SizeToClass(0 <= n <= MaxSmallSize) returns the size class,
//	1 <= sizeclass < NumSizeClasses, for n.
//	Size class 0 is reserved to mean "not small".
//
// class_to_size[i] = largest size in class i
// class_to_allocnpages[i] = number of pages to allocate when
// 	making new objects in class i
// class_to_transfercount[i] = number of objects to move when
//	taking a bunch of objects out of the central lists
//	and putting them in the thread free list.

int32	runtime_SizeToClass(int32);
extern	int32	runtime_class_to_size[NumSizeClasses];
extern	int32	runtime_class_to_allocnpages[NumSizeClasses];
extern	int32	runtime_class_to_transfercount[NumSizeClasses];
extern	void	runtime_InitSizes(void);


// Per-thread (in Go, per-M) cache for small objects.
// No locking needed because it is per-thread (per-M).
typedef struct MCacheList MCacheList;
struct MCacheList
{
	MLink *list;
	uint32 nlist;
	uint32 nlistmin;
};

struct MCache
{
	MCacheList list[NumSizeClasses];
	uint64 size;
	int64 local_alloc;	// bytes allocated (or freed) since last lock of heap
	int64 local_objects;	// objects allocated (or freed) since last lock of heap
	int32 next_sample;	// trigger heap sample after allocating this many bytes
};

void*	runtime_MCache_Alloc(MCache *c, int32 sizeclass, uintptr size, int32 zeroed);
void	runtime_MCache_Free(MCache *c, void *p, int32 sizeclass, uintptr size);
void	runtime_MCache_ReleaseAll(MCache *c);

// An MSpan is a run of pages.
enum
{
	MSpanInUse = 0,
	MSpanFree,
	MSpanListHead,
	MSpanDead,
};
struct MSpan
{
	MSpan	*next;		// in a span linked list
	MSpan	*prev;		// in a span linked list
	MSpan	*allnext;		// in the list of all spans
	PageID	start;		// starting page number
	uintptr	npages;		// number of pages in span
	MLink	*freelist;	// list of free objects
	uint32	ref;		// number of allocated objects in this span
	uint32	sizeclass;	// size class
	uint32	state;		// MSpanInUse etc
	union {
		uint32	*gcref;	// sizeclass > 0
		uint32	gcref0;	// sizeclass == 0
	};
};

void	runtime_MSpan_Init(MSpan *span, PageID start, uintptr npages);

// Every MSpan is in one doubly-linked list,
// either one of the MHeap's free lists or one of the
// MCentral's span lists.  We use empty MSpan structures as list heads.
void	runtime_MSpanList_Init(MSpan *list);
bool	runtime_MSpanList_IsEmpty(MSpan *list);
void	runtime_MSpanList_Insert(MSpan *list, MSpan *span);
void	runtime_MSpanList_Remove(MSpan *span);	// from whatever list it is in


// Central list of free objects of a given size.
struct MCentral
{
	Lock;
	int32 sizeclass;
	MSpan nonempty;
	MSpan empty;
	int32 nfree;
};

void	runtime_MCentral_Init(MCentral *c, int32 sizeclass);
int32	runtime_MCentral_AllocList(MCentral *c, int32 n, MLink **first);
void	runtime_MCentral_FreeList(MCentral *c, int32 n, MLink *first);

// Main malloc heap.
// The heap itself is the "free[]" and "large" arrays,
// but all the other global data is here too.
struct MHeap
{
	Lock;
	MSpan free[MaxMHeapList];	// free lists of given length
	MSpan large;			// free lists length >= MaxMHeapList
	MSpan *allspans;

	// span lookup
	MHeapMap map;

	// range of addresses we might see in the heap
	byte *min;
	byte *max;
	
	// central free lists for small size classes.
	// the union makes sure that the MCentrals are
	// spaced 64 bytes apart, so that each MCentral.Lock
	// gets its own cache line.
	union {
		MCentral;
		byte pad[64];
	} central[NumSizeClasses];

	FixAlloc spanalloc;	// allocator for Span*
	FixAlloc cachealloc;	// allocator for MCache*
};
extern MHeap runtime_mheap;

void	runtime_MHeap_Init(MHeap *h, void *(*allocator)(uintptr));
MSpan*	runtime_MHeap_Alloc(MHeap *h, uintptr npage, int32 sizeclass, int32 acct);
void	runtime_MHeap_Free(MHeap *h, MSpan *s, int32 acct);
MSpan*	runtime_MHeap_Lookup(MHeap *h, PageID p);
MSpan*	runtime_MHeap_LookupMaybe(MHeap *h, PageID p);
void	runtime_MGetSizeClassInfo(int32 sizeclass, int32 *size, int32 *npages, int32 *nobj);

void*	runtime_mallocgc(uintptr size, uint32 flag, int32 dogc, int32 zeroed);
int32	runtime_mlookup(void *v, byte **base, uintptr *size, MSpan **s, uint32 **ref);
void	runtime_gc(int32 force);

void*	runtime_SysAlloc(uintptr);
void	runtime_SysUnused(void*, uintptr);
void	runtime_SysFree(void*, uintptr);

enum
{
	RefcountOverhead = 4,	// one uint32 per object

	RefFree = 0,	// must be zero
	RefStack,		// stack segment - don't free and don't scan for pointers
	RefNone,		// no references
	RefSome,		// some references
	RefNoPointers = 0x80000000U,	// flag - no pointers here
	RefHasFinalizer = 0x40000000U,	// flag - has finalizer
	RefProfiled = 0x20000000U,	// flag - is in profiling table
	RefNoProfiling = 0x10000000U,	// flag - must not profile
	RefFlags = 0xFFFF0000U,
};

void	runtime_Mprof_Init(void);
void	runtime_MProf_Malloc(void*, uintptr);
void	runtime_MProf_Free(void*, uintptr);
void	runtime_MProf_Mark(void (*scan)(byte *, int64));

// Malloc profiling settings.
// Must match definition in extern.go.
enum {
	MProf_None = 0,
	MProf_Sample = 1,
	MProf_All = 2,
};
extern int32 runtime_malloc_profile;

typedef struct Finalizer Finalizer;
struct Finalizer
{
	Finalizer *next;	// for use by caller of getfinalizer
	void (*fn)(void*);
	void *arg;
	const struct __go_func_type *ft;
};

Finalizer*	runtime_getfinalizer(void*, bool);
