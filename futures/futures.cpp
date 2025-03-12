#include <cassert>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace std::literals;

int calculate_square(int x)
{
    std::cout << "Starting calculation for " << x << " in " << std::this_thread::get_id() << std::endl;

    std::random_device rd;
    std::uniform_int_distribution<> distr(100, 5000);

    std::this_thread::sleep_for(std::chrono::milliseconds(distr(rd)));

    if (x % 3 == 0)
        throw std::runtime_error("Error#3");

    return x * x;
}

void save_to_file(const std::string& filename)
{
    std::cout << "Saving to file: " << filename << std::endl;

    std::this_thread::sleep_for(3s);

    std::cout << "File saved: " << filename << std::endl;
}

int main()
{
    std::future<int> f_square_13 = std::async(std::launch::async, calculate_square, 13);
    std::future<int> f_square_9 = std::async(std::launch::deferred, [] { return calculate_square(9); });
    std::future<void> f_save = std::async(std::launch::async, [] { save_to_file("data.txt"); });

    std::cout << "Main thread: " << std::this_thread::get_id() << std::endl;

    while (f_save.wait_for(100ms) != std::future_status::ready)
    {
        std::cout << ".";
        std::cout.flush();
    }

    try
    {
        std::cout << "square(13) = " << f_square_13.get() << std::endl;
        std::cout << "square(9) = " << f_square_9.get() << std::endl;
        f_save.wait();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }    

    std::vector<std::tuple<int, std::future<int>>> f_squares;

    for(const auto& n : {7, 13, 26, 97})
    {
        f_squares.emplace_back(n, std::async(std::launch::async, [n] { return calculate_square(n); }));
    }

    for(auto& [n, f] : f_squares)
    {
        std::cout << n << '*' << n << " = " << f.get() << "\n";
    }
}


