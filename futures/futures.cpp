#include <cassert>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <syncstream>
#include <latch>

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

void save_to_file(const std::string& filename)
{
    sync_cout() << "Saving to file: " << filename << std::endl;

    std::this_thread::sleep_for(3s);

    sync_cout() << "File saved: " << filename << std::endl;
}

void future_demo()
{
    std::future<int> f_square_13 = std::async(std::launch::async, calculate_square, 13);
    std::future<int> f_square_9 = std::async(std::launch::deferred, [] { return calculate_square(9); });
    std::future<void> f_save = std::async(std::launch::async, [] { save_to_file("data.txt"); });

    sync_cout() << "Main thread: " << std::this_thread::get_id() << std::endl;

    while (f_save.wait_for(100ms) != std::future_status::ready)
    {
        sync_cout() << ".";
        std::cout.flush();
    }

    try
    {
        sync_cout() << "square(13) = " << f_square_13.get() << std::endl;
        sync_cout() << "square(9) = " << f_square_9.get() << std::endl;
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
        sync_cout() << n << '*' << n << " = " << f.get() << "\n";
    }

    sync_cout() << "\n------------------------------------\n";

    std::future<int> f_square_97 = std::async(std::launch::async, [] { return calculate_square(97); });

    std::shared_future<int> sf = f_square_97.share();    

    std::jthread thd1{[sf] { 
        sync_cout() << "Start#" << std::this_thread::get_id() << "\n";
        sync_cout() << "result1: " << sf.get() << std::endl; } 
    };
    
    auto f = std::async(std::launch::async, [sf] { 
        sync_cout() << "Start#" << std::this_thread::get_id() << "\n";
        sync_cout() << "result2: " << sf.get() << std::endl;
    });

    sync_cout() << "\n------------------------------------\n";

    auto fs1 = std::async(std::launch::async, save_to_file, "f1.dat");
    auto fs2 = std::async(std::launch::async, save_to_file, "f2.dat");
    auto fs3 = std::async(std::launch::async, save_to_file, "f3.dat");
    auto fs4 = std::async(std::launch::async, save_to_file, "f4.dat");
}

class SquareCalulator
{
    std::promise<int> promise_result_;
public:
    std::future<int> get_future()
    {
        return promise_result_.get_future();
    }

    void calculate(int n)
    {
        try
        {
            int result = calculate_square(n);
            promise_result_.set_value(result);
        }
        catch(...)
        {
            promise_result_.set_exception(std::current_exception());
        }

    }
};

void promise_demo()
{
    SquareCalulator calc;
    auto f = calc.get_future();

    std::jthread thd{[&calc] {
        calc.calculate(13); 
    }};

    std::jthread thd_consumer{[f = std::move(f)] mutable { sync_cout() << "result: " << f.get() << "\n";}};
}

template <typename TTask>
auto spawn_task(TTask&& task)
{
    using TResult = decltype(task());
    std::packaged_task<TResult()> pt(std::forward<TTask>(task));
    std::future<TResult> f = pt.get_future();

    std::jthread thd{std::move(pt)};
    thd.detach();

    return f;
}

void packaged_task_demo()
{
    std::packaged_task<int(int)> pt_square(calculate_square);
    auto f_result = pt_square.get_future();

    std::jthread thd{std::move(pt_square), 13};
    //pt_square(13);

    std::cout << "result: " << f_result.get() << "\n";

    auto f = spawn_task([] { return calculate_square(97); });

    std::latch all_done{4};

    spawn_task([&all_done] { save_to_file("f1.dat"); all_done.count_down(); });
    spawn_task([&all_done] { save_to_file("f2.dat"); all_done.count_down();});
    spawn_task([&all_done] { save_to_file("f3.dat"); all_done.count_down();});
    spawn_task([&all_done] { save_to_file("f4.dat"); std::this_thread::sleep_for(10s); all_done.count_down();});

    sync_cout() << "r: " << f.get() << "\n";

    all_done.wait();

    sync_cout() << "END" << std::endl;
}

int main()
{
    // promise_demo();

    packaged_task_demo();
}


