
#include "lgc.h"

namespace lgc
{

// ===============================================
// RefCntObject
// ===============================================

thread_local uint64_t RefCntObject::ThreadIndex = 0;
std::atomic<uint64_t> RefCntObject::ThreadIndexMax = {0};

void RefCntObject::allocThreadIndexForThisThread()
{
    ThreadIndex = ThreadIndexMax.fetch_add(1);
}

RefCntObject::RefCntObject()
{
    for (int i = 0; i < REF_CNT_SLOTS_NUM; i++)
    {
        ref_cnt_slots_[i].store(nullptr);
    }
}

RefCntObject::~RefCntObject()
{
    // release all thread local ref_cnt
    for (int i = 0; i < REF_CNT_SLOTS_NUM; i++)
    {
        ThreadLocalRefCnt* thread_local_ref_cnt_ptr = ref_cnt_slots_[i].load();
        if (thread_local_ref_cnt_ptr != nullptr)
        {
            delete thread_local_ref_cnt_ptr;
        }
    }
}

RefCntObject::ThreadLocalRefCnt* RefCntObject::initThreadLocalRefCnt()
{
    uint64_t i = ThreadIndex % REF_CNT_SLOTS_NUM;

    // double check
    ThreadLocalRefCnt* thread_local_ref_cnt_ptr = ref_cnt_slots_[i].load();
    if (thread_local_ref_cnt_ptr != nullptr)
    {
        return thread_local_ref_cnt_ptr;
    }

    // allocate memory for ref_cnt
    // allocated from thread memory poll if use TCmalloc
    thread_local_ref_cnt_ptr = (ThreadLocalRefCnt*)new(ThreadLocalRefCnt)(0);

    // cas
    static ThreadLocalRefCnt* empty;
    if (!ref_cnt_slots_[i].compare_exchange_strong(empty, thread_local_ref_cnt_ptr))
    {
        delete thread_local_ref_cnt_ptr; // cas fail, release memory
    }

    return ref_cnt_slots_[i].load();
}

int64_t RefCntObject::getRefCntForAllThreadOnce()
{
    int64_t ref_cnt = 0;
    for (int i = 0; i < REF_CNT_SLOTS_NUM; i++)
    {
        ThreadLocalRefCnt* thread_local_ref_cnt_ptr = ref_cnt_slots_[i].load();
        if (thread_local_ref_cnt_ptr != nullptr)
        {
            ref_cnt += thread_local_ref_cnt_ptr->load();
        }
    }
    return ref_cnt;
}

int64_t RefCntObject::getRefCntForAllThreadLockFree()
{
    int64_t ref_cnt_last = getRefCntForAllThreadOnce();
    int64_t ref_cnt_curr = getRefCntForAllThreadOnce();

    while (ref_cnt_last != ref_cnt_curr)
    {
        ref_cnt_last = ref_cnt_curr;
        ref_cnt_curr = getRefCntForAllThreadOnce();
    }
    
    return ref_cnt_curr;
}

int64_t RefCntObject::getRefCntForAllThreadWaitFree(bool& is_succ)
{
    int64_t ref_cnt_last = getRefCntForAllThreadOnce();
    int64_t ref_cnt_curr = getRefCntForAllThreadOnce();
    is_succ = (ref_cnt_last == ref_cnt_curr);
    return ref_cnt_curr;
}

// ===============================================
// GCMgr
// ===============================================

void GCMgr::setupThreadIndexForThisThread()
{
    RefCntObject::allocThreadIndexForThisThread();
}

int GCMgr::runGC()
{
    std::lock_guard<std::mutex> guard(gcobject_set_mutex_);
    int clear_num = 0;
    for(auto it = gcobject_set_.begin(); it != gcobject_set_.end();)
    {
        GCObject* gcobject_ptr = *it;
        if (gcobject_ptr != nullptr)
        {
            bool is_succ = false;
            int64_t ref_cnt = gcobject_ptr->getRefCntForAllThreadWaitFree(is_succ);
            if (is_succ && ref_cnt == 0)
            {
                clear_num++;
                // release gcobject
                delete gcobject_ptr;
                // remove from gcobject_set_
                it = gcobject_set_.erase(it); // <= attention! erase from loop here
                continue;
            }
        }
        it++;
    }
    return clear_num;
}

} // namespace lgc