#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <ranges>
#include <syncstream>

using namespace std::literals;

void background_work(size_t id, const std::string& text, std::chrono::milliseconds delay)
{
    std::cout << "bw#" << id << " has started..." << std::endl;

    for (const auto& c : text)
    {
        std::cout << "bw#" << id << ": " << c << std::endl;

        std::this_thread::sleep_for(delay);
    }

    std::cout << "bw#" << id << " is finished..." << std::endl;
}

void background_work_stopable(std::stop_token stp_token, size_t id, const std::string& text, std::chrono::milliseconds delay )
{
    std::cout << "bw_stopable#" << id << " has started..." << std::endl;

    std::thread::id thd_id = std::this_thread::get_id();

    std::cout << "this_thread::get_id() - " << thd_id << "\n";

    for (const auto& c : text)
    {
        if (stp_token.stop_requested())
        {
            std::cout << "Stop has been requested for THD#" << id << std::endl;
            return;
        }
        std::cout << "bw#" << id << ": " << c << std::endl;

        std::this_thread::sleep_for(delay);
    }

    std::cout << "bw_stopable#" << id << " is finished..." << std::endl;
}

class BackgroundWork 
{
    const int id_;
    const std::string text_;

public:
    BackgroundWork(int id, std::string text)
        : id_{id}
        , text_{std::move(text)}
    {
    }

    void operator()(std::chrono::milliseconds delay) const
    {
        std::cout << "BW#" << id_ << " has started..." << std::endl;

        for (const auto& c : text_)
        {
            std::cout << "BW#" << id_ << ": " << c << std::endl;

            std::this_thread::sleep_for(delay);
        }

        std::cout << "BW#" << id_ << " is finished..." << std::endl;
    }
};

std::jthread create_thread()
{
    static int id_gen = 100;
    const int id = ++id_gen;

    return std::jthread{&background_work, id, "THREAD#" + std::to_string(id), 250ms}; // Copy-Ellision = RVO
}

void may_throw()
{
    //throw std::runtime_error("Error#13");
}

std::osyncstream synced_cout()
{
    return std::osyncstream{std::cout};
}

void thread_basics()
{
    synced_cout() << "Main thread starts..." << std::endl;
    const std::string text = "Hello Threads";

    std::thread empty_thd;
    synced_cout() << "empty_thd: " << empty_thd.get_id() << "\n";

    std::thread thd_1{&background_work, 1, std::cref(text), 500ms};
    std::jthread thd_2 = create_thread();

    BackgroundWork bw{3, "BackgroundWork#3"};
    std::thread thd_3{std::cref(bw), 750ms};

    std::jthread thd_4{[&text] {
        background_work(4, text, 100ms);
    }};

    {
        std::vector<std::jthread> vec_thds;
        vec_thds.push_back(create_thread());
        vec_thds.push_back(create_thread());
        vec_thds.push_back(create_thread());

        thd_1.join();
        thd_2.join();
        thd_3.detach();
    }

    assert(thd_4.joinable());

    synced_cout() << "Main thread ends..." << std::endl;
}

void is_thread_safe_question()
{
    std::vector<int> vec_source = {1, 2, 3, 4, 5, 6, 7};

    std::vector<int> target_1;
    std::vector<int> target_2;

    {
        std::jthread thd_1{[&] { std::ranges::copy(vec_source, std::back_inserter(target_1)); }};
        std::jthread thd_2{[&] { std::ranges::copy(vec_source, std::back_inserter(target_2)); }};

        may_throw();
    } // implicit join

    std::cout << "target_1: ";
    for(const auto& item : target_1)
    {
        std::cout << item << " ";
    }
    std::cout << std::endl;
}

void jthread_with_stop_tokens()
{
    std::stop_source stop_source;

    {
        std::jthread thd_1{&background_work_stopable, stop_source.get_token(), 1, "THREAD#1", 500ms};
        std::jthread thd_2{&background_work_stopable, 2, "THREAD#2", 300ms};

        std::this_thread::sleep_for(1s);
        stop_source.request_stop();

        std::this_thread::sleep_for(1s);
        thd_2.request_stop();

    }

    std::cout << "END..." << std::endl;
} 

int main()
{
    std::cout << "NO OF CORES: " << std::max(1u, std::thread::hardware_concurrency()) << "\n";

    jthread_with_stop_tokens();
}
