#include "thread_safe_queue.hpp"

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <future>
#include <syncstream>
#include <random>

using namespace std::literals;

std::osyncstream sync_cout()
{
    return std::osyncstream{std::cout};
}

int calculate_square(int x)
{
    sync_cout() << "Starting calculation for " << x << " in " << std::this_thread::get_id() << std::endl;

    std::random_device rd;
    std::uniform_int_distribution<> distr(100, 5000);

    std::this_thread::sleep_for(std::chrono::milliseconds(distr(rd)));

    if (x % 3 == 0)
        throw std::runtime_error("Error#3");

    return x * x;
}

void background_work(size_t id, const std::string& text, std::chrono::milliseconds delay)
{
    std::cout << "bw#" << id << " has started..." << std::endl;

    for (const auto& c : text)
    {
        sync_cout()
            << "bw#" << id << ": " << c << " - this_thread::id: "
            << std::this_thread::get_id() << std::endl;

        std::this_thread::sleep_for(delay);
    }

    sync_cout() << "bw#" << id << " is finished..." << std::endl;
}

using Task = std::move_only_function<void()>;

inline namespace ver_1
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
                auto kill_task = [this] {
                    is_done_ = true;
                };
                tasks_.push(std::move(kill_task));
            }
        }

        template <typename TTask>
        auto submit(TTask&& task)
        {
            using TResult = decltype(task());

            // work-around for std::function as Task
            // auto pt = std::make_shared<std::packaged_task<TResult()>>(std::forward<TTask>(task));
            // std::future<TResult> f_result = pt->get_future();
            // tasks_.push([pt] { (*pt)(); });

            std::packaged_task<TResult()> pt(std::forward<TTask>(task));
            std::future<TResult> f_result = pt.get_future();
            tasks_.push(std::move(pt));

            return f_result;
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

namespace ver_2
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
                Task STOP;
                tasks_.push(std::move(STOP)); // poisoning pill
            }
        }

        void submit(Task task)
        {
            if (!task)
                throw std::invalid_argument("Empty task not supported");

            tasks_.push(std::move(task));
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
    sync_cout() << "Main thread starts..." << std::endl;
    const std::string text = "Hello Threads";

    {
        ThreadPool thd_pool(std::thread::hardware_concurrency());

        thd_pool.submit([text] { background_work(1, text, 250ms); });

        std::vector<std::tuple<int, std::future<int>>> f_squares;

        for (int i = 1; i < 20; ++i)
        {
            std::future<int> f_square = thd_pool.submit([i] { return calculate_square(i); });
            f_squares.emplace_back(i, std::move(f_square));
        }

        for (auto& [n, f] : f_squares)
        {
            try
            {
                sync_cout() << n << '*' << n << " = " << f.get() << std::endl;
            }
            catch(const std::exception& e)
            {
                std::cout << e.what() << std::endl;
            }                    
        }
    }

    sync_cout() << "Main thread ends..." << std::endl;
}
