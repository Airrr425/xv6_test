// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void hinit(void *heapEnd, int size);


extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *head;
} kmem;


struct Memblock {       
    int size;             
    int free;             
    struct Memblock *next;  
};

struct {                 
    struct spinlock lock; 
    struct Memblock *head;  
} heap;
/**
 * 初始化内存 同时初始化自旋锁 
 */
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);

  initlock(&heap.lock, "heap"); 
  heap.head = (struct Memblock*)(PHYSTOP - 8*1024*1024);   
  heap.head->size = 8*1024*1024 - sizeof(struct Memblock);  //初始化堆大小
  heap.head->next = 0; 
  heap.head->free = 1;
}

/**
 * 此处进行初始化内存空间的时候堆内存要单独处理 不然就会也被加入到固定分配的链表中
*/
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.head; // 头插法将当前块插入到空闲块链表中
  kmem.head = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.head;
  if(r)
    kmem.head = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}



void printheap() {
    acquire(&heap.lock);
    
    for (struct Memblock *pre = heap.head; pre != 0; pre = pre->next) {
      if(pre->free==1)
        printf("该块是空闲块, 块的地址为%p,块的大小为%d\n", pre,pre->size);
      else
        printf("该块正被占用, 块的地址为%p,块的大小为%d\n", pre,pre->size);
    }
    release(&heap.lock);
}



void *
malloc(uint size){
    struct Memblock *pre;
    size = size + sizeof(struct Memblock);
    acquire(&heap.lock); 
    for (pre = heap.head; pre != 0; pre = pre->next) {    //遍历链表寻找空闲块
        if (pre->free && pre->size > size) {
            struct Memblock *newBlock = pre + size;
            newBlock->size = pre->size - size;
            newBlock->next = pre->next;
            newBlock->free = 1;
            pre->size = size;
            pre->next = newBlock;
            pre->free = 0;
            break;
        }
    }
    release(&heap.lock); 
    return (void*)(pre);
}

void
free(void* p){
    if (!p) return;

    struct Memblock *block = (struct Memblock*)((char*)p - sizeof(struct Memblock)); // 计算块头部地址
    struct Memblock *curr, *prev;

    acquire(&heap.lock);

    block->free = 1; // 将该块标记为空闲

    // 合并相邻的空闲块
    curr = heap.head;
    prev = 0;
    while (curr != 0) {
        if (curr->free && curr->next && curr->next->free) {
            curr->size += curr->next->size + sizeof(struct Memblock);
            curr->next = curr->next->next;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }

    // 如果要释放的块和前一个块相邻且前一个块是空闲的，则合并
    if (prev && prev->free && (char*)prev + prev->size + sizeof(struct Memblock) == (char*)block) {
        prev->size += block->size + sizeof(struct Memblock);
        prev->next = block->next;
    } else {
        // 否则插入到链表中合适的位置
        if (prev) {
            block->next = prev->next;
            prev->next = block;
        } else {
            block->next = heap.head;
            heap.head = block;
        }
    }

}



void *best_fit_alloc(uint size) {
    struct Memblock *curr;
    struct Memblock *least = 0;      //最小的空闲块
    int minDif = HEAP;      
    void *result = 0;
    acquire(&heap.lock);
    for (curr = heap.head; curr != 0; curr = curr->next) {      
        if (curr->free && curr->size >= size + sizeof(struct Memblock)) {
            int sizeDif = curr->size - size - sizeof(struct Memblock);

            if (sizeDif < minDif) {    //寻找最小的块更新链表
                least = curr;
                minDif = sizeDif;
            }
        }
    }
    if (least) {
        if (least->size == size + sizeof(struct Memblock)) {
            least->free = 0;
        } else {
            struct Memblock *newBlock = (struct Memblock*)((char*)least + sizeof(struct Memblock) + size);
            newBlock->size = least->size - size - sizeof(struct Memblock);
            newBlock->next = least->next;
            newBlock->free = 1;

            least->size = size + sizeof(struct Memblock);
            least->next = newBlock;
            least->free = 0;

        }
        result = (void*)((char*)least);
    }else{
      printf("当前没有可分配内存块\n");
     }
    release(&heap.lock);
    return result;
}



void *worst_fit_alloc(uint size) {
    struct Memblock *curr;
    struct Memblock *largest = 0;   
    int maxDif = -1;             // 初始化为-1，标识未找到合适的块
    void *result = 0;

    acquire(&heap.lock);
    
    for (curr = heap.head; curr != 0; curr = curr->next) {
        if (curr->free && curr->size >= size + sizeof(struct Memblock)) {
            int sizeDif = curr->size - size - sizeof(struct Memblock);  //遍历寻找满足条件的块

            if (sizeDif > maxDif) {   //当出现更大的块时更新链表
                largest = curr;
                maxDif = sizeDif;
            }
        }
    }
    if (largest) {
        if (largest->size == size + sizeof(struct Memblock)) {
            largest->free = 0;
        } else {
            struct Memblock *newBlock = (struct Memblock*)((char*)largest + sizeof(struct Memblock) + size);
            newBlock->size = largest->size - size - sizeof(struct Memblock);
            newBlock->next = largest->next;
            newBlock->free = 1;

            largest->size = size + sizeof(struct Memblock);
            largest->next = newBlock;
            largest->free = 0;
        }
        result = (void*)((char*)largest);
    } else {
        printf("当前没有可分配内存块");
    }

    release(&heap.lock);
    return result;
}


// void quick_fit_init() {
//   for (int i = 0; i < NUM_LISTS; i++) {
//     quick_fit_heap.lists[i] = 0;
//   }
// }


// void *quick_fit_alloc(uint size) {
//   int idx = get_list_index(size);
//   if (idx == -1) return 0;  // too large

//   struct node *prev = 0, *t = quick_fit_heap.lists[idx];
//   while (t && t->size < size) {
//     prev = t;
//     t = t->next;
//   }

//   if (!t) return 0;

//   if (prev) {
//     prev->next = t->next;
//   } else {
//     quick_fit_heap.lists[idx] = t->next;
//   }
//   t->size = size - sizeof(struct node);
//   return (void *)(t + 1);
// }



// void quick_fit_free(void *p, uint size) {
//   int idx = get_list_index(size);
//   if (idx == -1) return;  // too large

//   struct node *t = (struct node *)p - 1;
//   t->next = quick_fit_heap.lists[idx];
//   quick_fit_heap.lists[idx] = t;
// }
