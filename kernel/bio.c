// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define NBUCKET 13

struct bucket {
  struct spinlock lock;   // 保护本桶的成员表和所有槽的元数据
  int n;                  // 本桶当前有几个槽
  int mem[NBUF];          // 本桶包含的槽下标，mem[0..n-1] 有效
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket bucket[NBUCKET];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
 // struct buf head;
} bcache;


/*void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}*/

void
binit(void)
{

  initlock(&bcache.lock, "bcache");
  for(int i=0;i<NBUF;i++){
    initsleeplock(&bcache.buf[i].lock,"bcache");
    bcache.buf[i].ticks=0;
    bcache.buf[i].refcnt=0;
    int idx=i%NBUCKET;
    bcache.buf[i].bucket=idx;
    bcache.bucket[idx].mem[bcache.bucket[idx].n]=i;
    bcache.bucket[idx].n++;
  }
  for(int i=0;i<NBUCKET;i++){
    initlock(&bcache.bucket[i].lock,"bcache");
  }
}
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
/*static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}*/

static struct buf*
bget(uint dev, uint blockno)
{
  int key=blockno%NBUCKET;
  acquire(&bcache.bucket[key].lock);
  for(int i=0;i<bcache.bucket[key].n;i++){
    int idx=bcache.bucket[key].mem[i];
    if(bcache.buf[idx].dev==dev&&bcache.buf[idx].blockno==blockno){
      bcache.buf[idx].refcnt++;
      release(&bcache.bucket[key].lock);
      acquiresleep(&bcache.buf[idx].lock);
      return &bcache.buf[idx];
    }
  }
  int index=-1;
  int min=0xFFFFFFFF;
  for(int i=0;i<bcache.bucket[key].n;i++){
    int idx=bcache.bucket[key].mem[i];
    if(bcache.buf[idx].refcnt==0&&bcache.buf[idx].ticks<min){
      index=idx;
      min=ticks;
    }
  }
  if(index!=-1){
    bcache.buf[index].blockno=blockno;
    bcache.buf[index].dev=dev;
    bcache.buf[index].refcnt=1;
    bcache.buf[index].valid = 0;
    release(&bcache.bucket[key].lock);
    acquiresleep(&bcache.buf[index].lock);
    return &bcache.buf[index];
  }
  release(&bcache.bucket[key].lock);
  for(int i=0;i<NBUCKET;i++){
    acquire(&bcache.bucket[i].lock);
  }
  int offset;
  for(int i=0;i<NBUCKET;i++){
    for(int j=0;j<bcache.bucket[i].n;j++){
      int idx=bcache.bucket[i].mem[j];
      if(bcache.buf[idx].refcnt==0&&bcache.buf[idx].ticks<min){
        index=idx;
        min=bcache.buf[idx].ticks;
        offset=j;
      }
    }
  }
  if(index!=-1){
    int skey=bcache.buf[index].bucket;
    bcache.bucket[skey].mem[offset]=bcache.bucket[skey].mem[bcache.bucket[skey].n-1];
    bcache.bucket[skey].n--;
    bcache.buf[index].bucket=key;
    bcache.bucket[key].mem[bcache.bucket[key].n]=index;
    bcache.bucket[key].n++;
    bcache.buf[index].blockno=blockno;
    bcache.buf[index].dev=dev;
    bcache.buf[index].refcnt=1;
    bcache.buf[index].valid = 0;
    for(int i=0;i<NBUCKET;i++){
      release(&bcache.bucket[i].lock);
    }
    acquiresleep(&bcache.buf[index].lock);
    return &bcache.buf[index];
  }
  panic("bget: no buffers");
}
// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
/*void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}*/

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);
  acquire(&bcache.bucket[b->bucket].lock);
  b->refcnt--;
  if(b->refcnt==0){
    b->ticks=ticks;  
  }
  release(&bcache.bucket[b->bucket].lock);
}


void
bpin(struct buf *b) {
  acquire(&bcache.bucket[b->bucket].lock);
  b->refcnt++;
  release(&bcache.bucket[b->bucket].lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.bucket[b->bucket].lock);
  b->refcnt--;
  release(&bcache.bucket[b->bucket].lock);
}


