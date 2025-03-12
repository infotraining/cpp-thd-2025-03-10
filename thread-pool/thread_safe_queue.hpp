#ifndef THREAD_SAFE_QUEUE_HPP
#define THREAD_SAFE_QUEUE_HPP

#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class ThreadSafeQueue
{
    std::queue<T> q_;
    mutable std::mutex mtx_q_;
    std::condition_variable cv_q_not_empty_;

public:
    ThreadSafeQueue() = default;

    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    bool empty() const
    {
        std::lock_guard lk{mtx_q_};
        return q_.empty();
    }

    void push(const T& item)
    {
        {
            std::lock_guard lk{mtx_q_};
            q_.push(item);
        }
        cv_q_not_empty_.notify_one();
    }

    void push(std::initializer_list<T> lst)
    {
        {
            std::lock_guard lk{mtx_q_};
            for (const auto& item : lst)
                q_.push(item);
        }
        cv_q_not_empty_.notify_all();
    }

    bool try_pop(T& item)
    {
        std::unique_lock lk{mtx_q_, std::try_to_lock};
        if (!lk.owns_lock() || q_.empty())
            return false;
        item = q_.front();
        q_.pop();
        return true;
    }

    void pop(T& item)
    {
        std::unique_lock lk{mtx_q_};
        cv_q_not_empty_.wait(lk, [this] { return !q_.empty(); });

        item = q_.front();
        q_.pop();
    }
};

#endif // THREAD_SAFE_QUEUE_HPP
