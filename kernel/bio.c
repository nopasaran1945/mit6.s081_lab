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
 
extern uint ticks;
struct entry{
uint timestamp;
struct buf *value;
struct entry *next;
struct entry *prev;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct entry bucket[NBUKET];
  struct buf head;
} bcache;
struct spinlock bucketlock[NBUKET];
struct entry bufentry[NBUF];
void 
bucketinit(void){
  struct entry *e;
  for(e = bcache.bucket;e<bcache.bucket+NBUKET;e++){
    e->timestamp = 0;
    e->value = 0;
    e->next = 0;  
    e->prev = 0;
  }
  for(int i = 0;i<NBUF;i++){
    bufentry[i].value = &bcache.buf[i];
  }
  for(int i = 0;i<NBUKET;i++){
    initlock(&bucketlock[i],"bcache.bucket");
  }
  //put all the entry into the 0 bucket
  acquire(&bucketlock[0]);
  for(int i = 0;i<NBUF;i++){
    bufentry[i].next = bcache.bucket[0].next;
    bufentry[i].prev = &bcache.bucket[0];
    bcache.bucket[0].next = &bufentry[i];
    if(bufentry[i].next!=0)
    bufentry[i].next->prev = &bufentry[i];
  }
  release(&bucketlock[0]);
}
void 
printbucket(void){
  struct entry *e;
  printf("bucket 0 : \n");
  for(e = bcache.bucket[0].next;e!=0;e = e->next){
    printf("%d order \n",e->value-bcache.buf);
  }
}
void
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
  bucketinit();
  printbucket();
}

struct entry *
search_mov_rubuf(uint dev,uint blockno){
  uint i = blockno%NBUKET;
  struct entry * e = 0;
  struct entry *mine = 0;
  uint mintime = -1;
  uint iffind = 0;
  uint beforemin;
  //acquire(&bcache.lock);

  for(int j = 0;j<NBUKET;j++){
    if(j!=i){
      acquire(&bucketlock[j]);
    }
      for(e = bcache.bucket[j].next;e!=0;e=e->next){
        if(e!=0&&e->timestamp<mintime&&e->value->refcnt==0){ //time stamp < mintime(have searched) and it is also a free block

          mine = e;
          mintime = mine->timestamp;
          iffind = 1;
        }
      }
  }
  if(mine!=0){
    mine->prev->next = mine->next;
    if(mine->next!=0)
    mine->next->prev = mine->prev;
    mine->next = bcache.bucket[i].next;
    mine->prev = &bcache.bucket[i];
    mine->prev->next = e;
    if(mine->next!=0)
    mine->next->prev = e;
    
  }
  return mine;
}
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  acquire(&bcache.lock);
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // Is the block already cached?
  /*
  uint i = blockno % NBUKET;
  acquire(&bucketlock[i]);
  for(struct entry *e = &bcache.bucket[i];e!=0;e=e->next){
    if(e->value!=0&&e->value->blockno==blockno&&e->value->dev==dev)
    {
      e->value->refcnt++;
      release(&bucketlock[i]);
      acquiresleep(&e->value->lock);
      return e->value;
    }
  }
  release(&bucketlock[i]);
  */
  

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  /* 
  
  struct entry *se = search_mov_rubuf(i);
  struct buf *res = se->value;
  

  */

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
void
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
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


