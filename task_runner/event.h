#ifndef _EVENT_H_
#define _EVENT_H_

#include <cassert>
#include <condition_variable>
#include <mutex>

namespace base {

// Simple thread synchronization primitive.
// Usual case: we are waiting on one thread for a confirmation from other
// thread. Notice: if notify() would be called before wait(), event would be
// triggered and waiting thread wouldn't block at all. To be able to use event
// one more time, you need to call reset manually.
class manual_event {
  public:
    // Wait until notify() call on other thread.
    // If notify() call happened before call to wait(), call to this method
    // wouldn't block anything.
    void wait() {
        std::unique_lock guard{mtx_};
        while (!triggered_) {
            cv_.wait(guard);
        }
    }

    // Awake waiting thread. Marks event as triggered.
    void notify() {
        std::unique_lock guard{mtx_};
        triggered_ = true;
        cv_.notify_all();
    }

    // Mark event as not triggered. It will allow you to use this event one more
    // time.
    void reset() {
        std::unique_lock guard{mtx_};
        triggered_ = false;
    }

  private:
    std::mutex mtx_;
    std::condition_variable cv_;
    bool triggered_ = false;
};

// Simple thread synchronization primitive.
// Usual case: we are waiting on one thread for a confirmation from other
// thread. Notice: if notify() would be called before wait(), event would be
// triggered and waiting thread wouldn't block at all. To be able to use event
// one more time, you need to call reset manually.
class auto_event {
  public:
    // Wait until notify() call on other thread. Implicitly set event as not
    // triggered. If notify() call happened before call to wait(), call to this
    // method wouldn't block anything.
    void wait() {
        std::unique_lock guard{mtx_};
        while (!triggered_) {
            cv_.wait(guard);
        }
        triggered_ = false;
    }

    // Awake waiting thread. Marks event as triggered.
    void notify() {
        std::unique_lock guard{mtx_};
        triggered_ = true;
        cv_.notify_all();
    }

    // You probably don't want to call it. Use manual_event instead.
    void reset() {
        std::unique_lock guard{mtx_};
        triggered_ = false;
    }

  private:
    std::mutex mtx_;
    std::condition_variable cv_;
    bool triggered_ = false;
};

}  // namespace base

#endif  // _EVENT_H_
