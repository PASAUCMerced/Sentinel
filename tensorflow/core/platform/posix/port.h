/*#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

class Pool_c
{ // Basic type define
   typedef unsigned int uint;
   typedef unsigned char uchar;

   uint m_numOfBlocks; // Num of blocks
   uint m_sizeOfEachBlock; // Size of each block
   uint m_numFreeBlocks; // Num of remaining blocks
   uint m_numInitialized; // Num of initialized blocks
   uchar* m_memStart; // Beginning of memory pool
   uchar* m_next; // Num of next free block
   //const size_t pagesize = sysconf(_SC_PAGESIZE);

 public:
    Pool_c()
    {
        m_numOfBlocks = 0;
        m_sizeOfEachBlock = 0;
        m_numFreeBlocks = 0;
        m_numInitialized = 0;
        m_memStart = NULL;
        m_next = 0;
    }
    ~Pool_c() { DestroyPool(); }
    void CreatePool(size_t sizeOfEachBlock,uint numOfBlocks)
    {
        m_numOfBlocks = numOfBlocks;
        m_sizeOfEachBlock = sizeOfEachBlock;
        //m_memStart = new uchar[ m_sizeOfEachBlock * m_numOfBlocks ];
        void* ptr;
        int err = posix_memalign(&ptr, sysconf(_SC_PAGESIZE),  m_sizeOfEachBlock * m_numOfBlocks);
        if (err != 0) {
          ptr = NULL;
          m_numFreeBlocks = 0;
        }
        else{
          m_numFreeBlocks = numOfBlocks;
        }
        m_memStart = (uchar*)ptr;
        m_next = m_memStart;
    }
    void DestroyPool()
    {
        //delete[] m_memStart;
        free((void*)m_memStart);
        m_memStart = NULL;
    }
    uchar* AddrFromIndex(uint i) const
    {
        return m_memStart + ( i * m_sizeOfEachBlock );
    }
    uint IndexFromAddr(const uchar* p) const
    {
        return (((uint)(p - m_memStart)) / m_sizeOfEachBlock);
    }
    void* Allocate()
    {
        if (m_numInitialized < m_numOfBlocks )
        {
            uint* p = (uint*)AddrFromIndex( m_numInitialized );
            *p = m_numInitialized + 1;
            m_numInitialized++;
        }
        void* ret = NULL;
        if ( m_numFreeBlocks > 0 )
        {
            ret = (void*)m_next;
            --m_numFreeBlocks;
            if (m_numFreeBlocks!=0)
            {
                m_next = AddrFromIndex( *((uint*)m_next) );
            }
            else
            {
                m_next = NULL;
            }
        }
        return ret;
    }
    void DeAllocate(void* p)
     {
         if (m_next != NULL)
         {
             (*(uint*)p) = IndexFromAddr( m_next );
             m_next = (uchar*)p;
         }
         else
         {
             *((uint*)p) = m_numOfBlocks;
             m_next = (uchar*)p;
         }
         ++m_numFreeBlocks;
     }
}; // End pool class
*/
#include <sys/mman.h>
#include <pjlib.h>
#include <map>
#include <list>
#include <mutex>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#define KB(x)   ((size_t) (x) << 10)
#define MB(x)   ((size_t) (x) << 20)

/*
class Pool_c{
private:
    pj_caching_pool cp;
    pj_status_t status;
    int reserved_size;
    pj_pool_t *pool;
    static void my_perror(const char *title, pj_status_t status)
    {
        PJ_PERROR(1,(THIS_FILE, status, title));
    }

public:
    Pool_c(int _reserved_size = 1024)
    {
        reserved_size = _reserved_size;
        //init pjlib
        status = pj_init();
        if (status != PJ_SUCCESS) {
            my_perror("Error initializing PJLIB", status);
            return;
        }

        //create pool factory
        pj_caching_pool_init(&cp, NULL, reserved_size*MB(1) );
        pool = pj_pool_create(cp.pfactory, // the factory
                              "pool1", // pool's name
                              reserved_size*MB(1), // initial size
                              MB(1), // increment size
                              NULL); // use default callback.
        if (pool == NULL) {
          my_perror("Error creating pool", PJ_ENOMEM);
          return;
        }
    }

    void* allocate(size_t size)
    {
        return pj_pool_alloc(pool, size);
    }

    ~Pool_c(){
        // Done with silly demo, must free pool to release all memory.
        pj_pool_release(pool);
        // Done with demos, destroy caching pool before exiting app.
        pj_caching_pool_destroy(&cp);
    }


};
*/


class MyMemoryAllocator{
private:
    std::map<void*, int> m;          //record<ptr, size> pair for quick free
    std::map<int, std::list<void*> > l;    //record<size, list<ptr>> for quick allocation
    size_t pool_size;           //pool size in MB
    std::mutex l_alloc;
    std::mutex l_free;
    //Pool_c *p;
    const size_t pagesize = sysconf(_SC_PAGESIZE);
    const size_t linesize = sizeof(void*);


public:
    MyMemoryAllocator(size_t _pool_size = 1){
        pool_size = _pool_size;
        //p = Pool_c(pool_size);
    }

    void* MyAllocate(size_t size)
    {
       if(size <= linesize)
        {
            size = linesize;
        }
        else{
          size = (size/pagesize + 1)*pagesize;
        }
        //int page = size/pagesize + 1;

        void* ptr;
        if (!l[size].empty())
        {
            l_alloc.lock();
            ptr = l[size].front();
            m[ptr] = size;
            l[size].pop_front();
            l_alloc.unlock();
            return ptr;
        }
        else{
            //ptr = p.allocate(size);
            int err = posix_memalign(&ptr, EIGEN_MAX_ALIGN_BYTES, size);
            //ptr = mmap(0, page, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
            if (err != 0) {
              return nullptr;
            } else {
              l_alloc.lock();
              m[ptr] = size;
              l_alloc.unlock();
              return ptr;
            }
            //ptr = memalign(EIGEN_MAX_ALIGN_BYTES, page*pagesize);
            //if(ptr!=NULL)
            //  m[ptr] = page;
        }
        //return ptr;
    }

    void MyFree(void* ptr)
    {

        if(ptr == NULL)
          return;
        int size = m[ptr];
        if(size)
        {
            l_alloc.lock();
            l[size].push_back(ptr);
            //free(ptr);
            m[ptr] = 0;
            l_alloc.unlock();
        }
        /*else{
          free(ptr);
        }*/
    }

    ~MyMemoryAllocator(){

    }

};
