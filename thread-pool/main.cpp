#include "thread_safe_queue.hpp"

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::literals;

void background_work(size_t id, const std::string& text, std::chrono::milliseconds delay)
{
    std::cout << "bw#" << id << " has started..." << std::endl;

    for (const auto& c : text)
    {
        std::cout
            << "bw#" << id << ": " << c << " - this_thread::id: "
            << std::this_thread::get_id() << std::endl;

        std::this_thread::sleep_for(delay);
    }

    std::cout << "bw#" << id << " is finished..." << std::endl;
}

using Task = std::function<void()>;

namespace ver_1
{
    class ThreadPool
    {
    public:
        ThreadPool(size_t size)
        {
            threads_.reserve(size);
            for (size_t i = 0; i < size; ++i)
                threads_.push_back(std::jthread{[this] {
                    run();
                }});
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        ~ThreadPool()
        {
            auto kill_task = [this] {
                is_done_ = true;
            };

            for (size_t i = 0; i < threads_.size(); ++i)
            {
                tasks_.push(kill_task);
            }
        }

        template <typename TTask>
        void submit(TTask&& task)
        {
            tasks_.push(std::forward<TTask>(task));
        }

    private:
        ThreadSafeQueue<Task> tasks_;
        std::vector<std::jthread> threads_;
        std::atomic<bool> is_done_ = false;

        void run()
        {
            while (!is_done_)
            {
                Task task;
                tasks_.pop(task);

                task(); // running task in this thread
            }
        }
    };
} // namespace ver_1

inline namespace ver_2
{
    class ThreadPool
    {
    public:
        ThreadPool(size_t size)
        {
            threads_.reserve(size);
            for (size_t i = 0; i < size; ++i)
                threads_.push_back(std::jthread{[this] {
                    run();
                }});
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        ~ThreadPool()
        {            
            for (size_t i = 0; i < threads_.size(); ++i)
            {
                tasks_.push(STOP); // poisoning pill
            }
        }

        void submit(Task task)
        {
            if (!task)
                throw std::invalid_argument("Empty task not supported");
                
            tasks_.push(task);
        }

    private:
        ThreadSafeQueue<Task> tasks_;
        std::vector<std::jthread> threads_;
        inline static const Task STOP;

        void run()
        {
            while (true)
            {
                Task task;
                tasks_.pop(task);

                if (!task)
                    break; // if poisonning pill

                task(); // running task in this thread
            }
        }
    };
} // namespace ver_1


int main()
{
    std::cout << "Main thread starts..." << std::endl;
    const std::string text = "Hello Threads";

    {
        ThreadPool thd_pool(std::thread::hardware_concurrency());

        thd_pool.submit([text] { background_work(1, text, 250ms); });

        for (int i = 2; i <= 20; ++i)
            thd_pool.submit([text, i] { background_work(i, text + std::to_string(i), std::chrono::milliseconds(100 + 10 * i)); });
    }

    std::cout << "Main thread ends..." << std::endl;
}
