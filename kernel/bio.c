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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;
struct buf bucket[NBUKET];
struct spinlock bucketlock[NBUKET];
void printbucket(void){
  struct buf *b;
  for(int i = 0;i<NBUKET;i++){
  b = bucket[i].next;
  printf("bucket : %d \n",i);
  while(b!=0){
  printf("order %d , refcnt : %d ",b-bcache.buf,b->refcnt);
  if(b->prev!=0)
  printf("order %d 's prev pointer is : %d \n",b-bcache.buf,b->prev-bcache.buf);
  b = b->next;
  }
  }
}
void 
bucketinit(void){
for(int i = 0;i<NBUKET;i++){
bucket[i].blockno = 0;
bucket[i].next=0;
bucket[i].prev=0;
bucket[i].timestamp=0;
bucket[i].dev=0;
}
//mount all the buffers into bucket 0
for(int i = 0;i<NBUF;i++){
  bcache.buf[i].next = bucket[0].next;
  bcache.buf[i].prev = &bucket[0];
  if(bcache.buf[i].next!=0)
  bcache.buf[i].next->prev= &bcache.buf[i];
  bucket[0].next = &bcache.buf[i];

}
//init the locks of bucket
for(int i =0;i<NBUKET;i++)
initlock(&bucketlock[i],"bcache : bucket"); 

}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  
 // bcache.head.prev = &bcache.head;
  //bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
   // b->next = bcache.head.next;
    //b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    //bcache.head.next->prev = b;
    //bcache.head.next = b;
    b->refcnt = 0;
  }
  
  bucketinit();

}
struct buf *
search_move_buf(uint dev, uint blockno){
  uint i = blockno%NBUKET;
  struct buf* minbuf = 0;
  uint mintime = 0xffffffff;
  //uint minbucket = 0;
  acquire(&bucketlock[i]);
  //stage 1 
  for(struct buf *b = bucket[i].next;b!=0;b=b->next){
    if(b->blockno==blockno&&b->dev==dev){
      b->refcnt ++;
      b->timestamp = getticks();
      release(&bucketlock[i]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucketlock[i]);
  //stage 2
  acquire(&bcache.lock);
  acquire(&bucketlock[i]);
  for(struct buf *b = bucket[i].next;b!=0;b=b->next){
    if(b->blockno==blockno&&b->dev==dev){
      b->refcnt ++;
      b->timestamp = getticks();
      release(&bucketlock[i]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucketlock[i]);
  uint beforebucket = -1;
  uint iffind = 0;
  for(int j = 0;j<NBUKET;j++){
      acquire(&bucketlock[j]);
      iffind = 0;
  for(struct buf* b = bucket[j].next;b!=0;b = b->next){
      //lock the bucket
      if(b->timestamp<mintime&&(b->refcnt==0)){
        //find a free buffer cache 
        mintime = b->timestamp;
        minbuf = b;  
        iffind = 1;
        break;
      }
    }
    if(!iffind)
    release(&bucketlock[j]);
    else{
      if(beforebucket!=-1)
      release(&bucketlock[beforebucket]);
      beforebucket = j;
    }
  }
  if(minbuf!=0){
    //minbucket = minbuf->blockno%NBUKET;
    //remove minbuf from bucket
    minbuf->prev->next = minbuf->next;
    if(minbuf->next!=0)
    minbuf->next->prev = minbuf->prev;
   
    minbuf->next = bucket[i].next;
    minbuf->prev = &bucket[i];
    if(minbuf->next!=0)
    minbuf->next->prev = minbuf;
    bucket[i].next = minbuf;

    minbuf->blockno = blockno;
    minbuf->dev = dev;
    minbuf->refcnt = 1;
    minbuf->valid = 0;
    minbuf->timestamp = getticks();
    
    release(&bucketlock[beforebucket]);
    release(&bcache.lock);
    acquiresleep(&minbuf->lock);
    //printbucket();
    return minbuf; 
  }
    panic("bet:search and move");
}
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf * b = search_move_buf(dev,blockno);
  return b;
  /*
  struct buf *b;

  acquire(&bcache.lock);
  //acquire the lock in each bucket when searching bucket i
  //uint i = blockno%NBUKET;
  //acquire(&bucketlock[i]);

  
  
  
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
  */
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

  //acquire(&bcache.lock);
  uint i = b->blockno % NBUKET;
  acquire(&bucketlock[i]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    
    if(b->next!=0)
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bucket[i].next;
    b->prev = &bucket[i];
    if(b->next!=0)
    b->next->prev = b;
    b->prev->next = b;
    b->timestamp = getticks();
  }
  release(&bucketlock[i]);
  //release(&bcache.lock);
}

void
bpin(struct buf *b) {
  uint i = b->blockno%NBUKET;
  acquire(&bucketlock[i]);
  b->refcnt++;
  release(&bucketlock[i]);
}

void
bunpin(struct buf *b) {
  uint i = b->blockno%NBUKET;
  acquire(&bucketlock[i]);
  b->refcnt--;
  release(&bucketlock[i]);
}


