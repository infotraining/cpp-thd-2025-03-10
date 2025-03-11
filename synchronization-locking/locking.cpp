#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

using namespace std::literals;

template <typename T>
struct SynchronizedValue
{
    T value;
    std::mutex mtx_value;

    [[nodiscard("Must be assigned to start critical section")]] 
    std::lock_guard<std::mutex> with_lock()
    {
        return std::lock_guard{mtx_value};
    }

    [[nodiscard("Must be assigned to start critical section")]] 
    std::unique_lock<std::mutex> with_ulock()
    {
        return std::unique_lock{mtx_value};
    }

    template <typename F>
    void lock(F&& f)
    {
        std::lock_guard lk{mtx_value};
        f(value); 
    }
};

void run(int& value, std::mutex& mtx_value)
{
    for(int i = 0; i < 10'000'000; ++i)
    {
        // mtx_value.lock(); // begin of critical section
        // ++value;        
        // mtx_value.unlock(); // end of critical section

        // RAII - critical section
        {
            std::lock_guard<std::mutex> lock{mtx_value}; // locking mutex
            ++value;
        }
    }
}

void run(SynchronizedValue<int>& synced_counter)
{
    for(int i = 0; i < 10'000'000; ++i)
    {
        // RAII - critical section
        {
            auto lk = synced_counter.with_lock();
            ++synced_counter.value;
        }

        synced_counter.lock([](auto& value) { ++value; });
    }
}

int main()
{
    std::cout << "Main thread starts..." << std::endl;
    
    SynchronizedValue<int> counter{};
    
    {
        std::jthread thd_1{[&counter] { run(counter); }};
        std::jthread thd_2{[&counter] { run(counter); }};
    }

    std::cout << "counter: " << counter.value << "\n";

    std::cout << "Main thread ends..." << std::endl;
}
