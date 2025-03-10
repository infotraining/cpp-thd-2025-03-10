#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

using namespace std::literals;

void run(int& value, std::mutex& mtx_value)
{
    for(int i = 0; i < 10'000'000; ++i)
    {
        mtx_value.lock(); // begin of critical section
        ++value;        
        mtx_value.unlock(); // end of critical section
    }
}

int main()
{
    std::cout << "Main thread starts..." << std::endl;

    int counter = 0;
    std::mutex mtx_counter;
    
    {
        std::jthread thd_1{[&counter, &mtx_counter] { run(counter, mtx_counter); }};
        std::jthread thd_2{[&counter, &mtx_counter] { run(counter, mtx_counter); }};
    }

    std::cout << "counter: " << counter << "\n";

    std::cout << "Main thread ends..." << std::endl;
}
