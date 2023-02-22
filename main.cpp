
#include "lgc.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <memory>
#include <stdio.h>


std::atomic<bool> is_running;

struct HeavyObject : public lgc::GCObject
{
    int version = 0;
    int i = 0;
    double d = 0.0;
    std::string s;
    char buffer[4096];

    HeavyObject()
    {
        printf("call HeavyObject(): %p\n", this);
    }

    ~HeavyObject()
    {
        printf("call ~HeavyObject(): %p\n", this);
    }
};

#ifdef USE_STD_SHARED_PTR
std::shared_ptr<HeavyObject> global_obj_ptr;
thread_local std::shared_ptr<HeavyObject> local_obj_ptr;
#else
lgc::GCObjectPtr<HeavyObject> global_obj_ptr;
thread_local lgc::GCObjectPtr<HeavyObject> local_obj_ptr;
#endif

#ifdef USE_STD_SHARED_PTR
std::shared_ptr<HeavyObject> getLocalHeavyObject()
#else
lgc::GCObjectPtr<HeavyObject> getLocalHeavyObject()
#endif
{
    return local_obj_ptr;
}

void backgroundGCThread()
{
    while (is_running.load() == true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        int clear_num = lgc::GCMgr::getInstance()->runGC();
        if (clear_num > 0)
        {
            printf("gc clear in backgroundGCThread: %d\n", clear_num);
        }
    }
}

void workThread()
{
    lgc::GCMgr::getInstance()->setupThreadIndexForThisThread();

    while (is_running.load() == true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        local_obj_ptr = global_obj_ptr;

        for (int i = 0; i != 999; i++)
        {
            auto obj_ptr = getLocalHeavyObject();
            if (obj_ptr != nullptr && obj_ptr->i != 0)
            {
                // do something
                printf("%d\n", obj_ptr->version);
            }
        }
    }
    local_obj_ptr = nullptr;
}

int main()
{
    // init thread
    is_running.store(true);

    std::thread backgroup(backgroundGCThread);
    std::thread work1(workThread);
    std::thread work2(workThread);
    std::thread work3(workThread);
    std::thread work4(workThread);

    // create HeavyObject
    for (int i = 0; i < 10; i++)
    {
#ifdef USE_STD_SHARED_PTR
        auto obj_ptr = std::make_shared<HeavyObject>();
#else
        auto obj_ptr = lgc::GCMgr::getInstance()->newGCObject<HeavyObject>();
#endif
        obj_ptr->version = i;
        global_obj_ptr = obj_ptr; // setup global obj

        std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    }
    global_obj_ptr = nullptr;

    // join
    is_running.store(false);

    backgroup.join();
    work1.join();
    work2.join();
    work3.join();
    work4.join();

    // gc before stop
    while (lgc::GCMgr::getInstance()->getGCObjectNum() > 0)
    {
        lgc::GCMgr::getInstance()->runGC();
        int clear_num = lgc::GCMgr::getInstance()->runGC();
        if (clear_num > 0)
        {
            printf("gc clear in mainThread: %d\n", clear_num);
        }
    }
    
    return 0;
}