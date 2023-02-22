// light gc system base ref_cnt

#pragma once

#include <stdint.h>
#include <atomic>
#include <mutex>
#include <set>

namespace lgc
{

#define REF_CNT_SLOTS_NUM 8

struct RefCntObject
{
public:
    typedef std::atomic<int64_t> ThreadLocalRefCnt;

    static thread_local uint64_t ThreadIndex;
    static std::atomic<uint64_t> ThreadIndexMax;
    static void allocThreadIndexForThisThread();

private:
    std::atomic<ThreadLocalRefCnt*> ref_cnt_slots_[REF_CNT_SLOTS_NUM];

public:
    RefCntObject();
    ~RefCntObject();

    inline void incRefCntForThisThread(int64_t inc_val)
    {
        uint64_t i = ThreadIndex % REF_CNT_SLOTS_NUM;
        ThreadLocalRefCnt* thread_local_ref_cnt_ptr = ref_cnt_slots_[i].load();
        if (thread_local_ref_cnt_ptr == nullptr)
        {       
            thread_local_ref_cnt_ptr = initThreadLocalRefCnt(); // slow path
        }
        thread_local_ref_cnt_ptr->fetch_add(inc_val);
    }

    ThreadLocalRefCnt* initThreadLocalRefCnt();
    
    int64_t getRefCntForAllThreadOnce();
    int64_t getRefCntForAllThreadLockFree();
    int64_t getRefCntForAllThreadWaitFree(bool& is_succ);
};

struct GCObject : public RefCntObject
{
public:
    GCObject() {}
    virtual ~GCObject() {}
};

template <typename GCObjectT>
struct GCObjectPtr
{
private:
    GCObjectT* gc_object_ = nullptr;

public:
    GCObjectPtr() {}

    GCObjectPtr(GCObjectT* gc_object)
    : gc_object_(gc_object)
    {
        if (gc_object_ != nullptr)
        {
            gc_object_->incRefCntForThisThread(1);
        }
    }

    GCObjectPtr(const GCObjectPtr<GCObjectT>& other)
    {
        gc_object_ = other.gc_object_;
        if (gc_object_ != nullptr)
        {
            gc_object_->incRefCntForThisThread(1);
        }
    }

    GCObjectPtr<GCObjectT>& operator=(const GCObjectPtr<GCObjectT>& other)
    {
        if (this != &other)
        {
            if (gc_object_ != nullptr)
            {
                gc_object_->incRefCntForThisThread(-1);
            }
            
            gc_object_ = other.gc_object_;
            
            if (gc_object_ != nullptr)
            {
                gc_object_->incRefCntForThisThread(1);
            }
        }
        return *this;
    }
    
    ~GCObjectPtr()
    {
        if (gc_object_ != nullptr)
        {
            gc_object_->incRefCntForThisThread(-1);
        }
        gc_object_ = nullptr;
    }

    // operator overload for pointer
    GCObjectT& operator*() { return *gc_object_; }
    const GCObjectT& operator*() const { return *gc_object_; }
    
    GCObjectT* operator->() { return gc_object_; }
    const GCObjectT* operator->() const { return gc_object_; }

    bool operator==(const GCObjectPtr<GCObjectT>& other) const { return gc_object_ == other.gc_object_; }
    bool operator!=(const GCObjectPtr<GCObjectT>& other) const { return gc_object_ != other.gc_object_; }
    bool operator==(const GCObjectT* gcobject) const { return gc_object_ == gcobject; }
    bool operator!=(const GCObjectT* gcobject) const { return gc_object_ != gcobject; }

    operator bool () const { return gc_object_ != nullptr; }
    operator GCObjectT *() { return gc_object_; }
    operator const GCObjectT *() { return gc_object_; }
};


struct GCMgr
{
public:
    static GCMgr* getInstance()
    {
        static GCMgr inst;
        return &inst;
    }

private:
    std::set<GCObject*> gcobject_set_;
    std::mutex gcobject_set_mutex_;

    GCMgr() = default;

public:
    void setupThreadIndexForThisThread();
    int runGC();

    int getGCObjectNum()
    {
        std::lock_guard<std::mutex> guard(gcobject_set_mutex_);
        return gcobject_set_.size();
    }

    template <typename T>
    GCObjectPtr<T> newGCObject()
    {
        // TODO: use aligned new
        T* gcobject = (T*)new T();
        {
            std::lock_guard<std::mutex> guard(gcobject_set_mutex_);
            gcobject_set_.insert(gcobject);
        }
        return GCObjectPtr<T>(gcobject);
    }
};

} // namespace lgc