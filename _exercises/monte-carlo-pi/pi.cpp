#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <thread>
#include <future>

using namespace std;

constexpr uintmax_t N = 100'000'000;

void calc_hits_per_thread(uintmax_t count, uintmax_t& hits)
{
    const auto thd_id = std::this_thread::get_id();
    const auto seed = std::hash<std::thread::id>{}(thd_id);
    std::mt19937_64 rnd_gen(seed);
    std::uniform_real_distribution<double> rnd_distr(0.0, 1.0);

    for (long n = 0; n < count; ++n)
    {
        double x = rnd_distr(rnd_gen);
        double y = rnd_distr(rnd_gen);
        if (x * x + y * y < 1)
            ++hits; // hot loop 
    }
}

void calc_hits_per_thread_with_local_hits(uintmax_t count, uintmax_t& hits)
{
    const auto thd_id = std::this_thread::get_id();
    const auto seed = std::hash<std::thread::id>{}(thd_id);
    std::mt19937_64 rnd_gen(seed);
    std::uniform_real_distribution<double> rnd_distr(0.0, 1.0);

    uintmax_t local_hits = 0;
    for (long n = 0; n < count; ++n)
    {
        double x = rnd_distr(rnd_gen);
        double y = rnd_distr(rnd_gen);
        if (x * x + y * y < 1)
            ++local_hits; // incrementing local variable
    }

    hits += local_hits;
}

void calc_hits_per_thread_with_mutex(uintmax_t count, uintmax_t& hits, std::mutex& mtx)
{
    const auto thd_id = std::this_thread::get_id();
    const auto seed = std::hash<std::thread::id>{}(thd_id);
    std::mt19937_64 rnd_gen(seed);
    std::uniform_real_distribution<double> rnd_distr(0.0, 1.0);

    for (long n = 0; n < count; ++n)
    {
        double x = rnd_distr(rnd_gen);
        double y = rnd_distr(rnd_gen);
        if (x * x + y * y < 1)
        {
            std::lock_guard lk{mtx};
            ++hits;
        }
    }
}

struct Hits
{
    alignas(std::hardware_destructive_interference_size) uintmax_t value; // aligned to cache line
};

void calc_hits_per_thread_with_aligned_hits(uintmax_t count, Hits& hits)
{
    const auto thd_id = std::this_thread::get_id();
    const auto seed = std::hash<std::thread::id>{}(thd_id);
    std::mt19937_64 rnd_gen(seed);
    std::uniform_real_distribution<double> rnd_distr(0.0, 1.0);

    for (long n = 0; n < count; ++n)
    {
        double x = rnd_distr(rnd_gen);
        double y = rnd_distr(rnd_gen);
        if (x * x + y * y < 1)
            ++hits.value;
    }
}

void calc_hits_per_thread_with_atomic(uintmax_t count, std::atomic<uintmax_t>& hits)
{
    const auto thd_id = std::this_thread::get_id();
    const auto seed = std::hash<std::thread::id>{}(thd_id);
    std::mt19937_64 rnd_gen(seed);
    std::uniform_real_distribution<double> rnd_distr(0.0, 1.0);

    for (long n = 0; n < count; ++n)
    {
        double x = rnd_distr(rnd_gen);
        double y = rnd_distr(rnd_gen);
        if (x * x + y * y < 1)
            //++hits; // hot loop 
            hits.fetch_add(1, std::memory_order_relaxed);
    }
}

uintmax_t calc_hits_per_thread_with_future(uintmax_t count)
{
    const auto thd_id = std::this_thread::get_id();
    const auto seed = std::hash<std::thread::id>{}(thd_id);
    std::mt19937_64 rnd_gen(seed);
    std::uniform_real_distribution<double> rnd_distr(0.0, 1.0);

    uintmax_t hits = 0;
    for (long n = 0; n < count; ++n)
    {
        double x = rnd_distr(rnd_gen);
        double y = rnd_distr(rnd_gen);
        if (x * x + y * y < 1)
            ++hits; // hot loop 
    }
    return hits;
}

void mc_pi_one_thread()
{
    cout << "Pi calculation started! One thread!" << endl;
    const auto start = chrono::high_resolution_clock::now();

    uintmax_t hits = 0;

    calc_hits_per_thread(N, hits);

    const double pi = static_cast<double>(hits) / N * 4;

    const auto end = chrono::high_resolution_clock::now();
    const auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "Pi = " << pi << endl;
    cout << "Elapsed = " << elapsed_time << "ms" << endl;
}

void mc_pi_many_threads()
{
    std::cout << "\n------------------------------------\n";
    std::cout << "Pi calculation started! Many threads" << endl;
    const auto start = chrono::high_resolution_clock::now();

    int num_threads = std::max(std::thread::hardware_concurrency(), 1u);
    std::vector<uintmax_t> hits_from_thread(num_threads);

    {
        std::vector<std::jthread> threads;

        for (int i = 0; i < num_threads; i++)
        {
            threads.push_back(std::jthread{calc_hits_per_thread, int(N / num_threads), std::ref(hits_from_thread[i])});
        }
    } // join

    auto hits = std::reduce(hits_from_thread.begin(), hits_from_thread.end());

    const double pi = static_cast<double>(hits) / N * 4;

    const auto end = chrono::high_resolution_clock::now();
    const auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    std::cout << "Pi = " << pi << endl;
    std::cout << "Elapsed = " << elapsed_time << "ms" << endl;
}

void mc_pi_many_threads_with_local_counter()
{
    std::cout << "\n------------------------------------\n";
    std::cout << "Pi calculation started! Many threads - local counter" << endl;
    const auto start = chrono::high_resolution_clock::now();

    int num_threads = std::max(std::thread::hardware_concurrency(), 1u);
    std::vector<uintmax_t> hits_from_thread(num_threads);

    {
        std::vector<std::jthread> threads;

        for (int i = 0; i < num_threads; i++)
        {
            threads.push_back(std::jthread{calc_hits_per_thread_with_local_hits, int(N / num_threads), std::ref(hits_from_thread[i])});
        }
    } // join

    auto hits = std::reduce(hits_from_thread.begin(), hits_from_thread.end());

    const double pi = static_cast<double>(hits) / N * 4;

    const auto end = chrono::high_resolution_clock::now();
    const auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    std::cout << "Pi = " << pi << endl;
    std::cout << "Elapsed = " << elapsed_time << "ms" << endl;
}

void mc_pi_many_threads_with_aligned_hits()
{
    std::cout << "\n------------------------------------\n";
    std::cout << "Pi calculation started! Many threads - hits aligned for cache line" << endl;
    const auto start = chrono::high_resolution_clock::now();

    int num_threads = std::max(std::thread::hardware_concurrency(), 1u);
    std::vector<Hits> hits_from_thread(num_threads);

    {
        std::vector<std::jthread> threads;

        for (int i = 0; i < num_threads; i++)
        {
            threads.push_back(std::jthread{calc_hits_per_thread_with_aligned_hits, int(N / num_threads), std::ref(hits_from_thread[i])});
        }
    } // join

    auto hits = std::accumulate(hits_from_thread.begin(), hits_from_thread.end(), 0ULL, [](auto red, auto arg) { return red + arg.value; });

    const double pi = static_cast<double>(hits) / N * 4;

    const auto end = chrono::high_resolution_clock::now();
    const auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    std::cout << "Pi = " << pi << endl;
    std::cout << "Elapsed = " << elapsed_time << "ms" << endl;
}

void mc_pi_with_mutex()
{
    std::cout << "\n------------------------------------\n";
    std::cout << "Pi calculation started! Many threads - with mutex" << endl;
    const auto start = chrono::high_resolution_clock::now();

    int num_threads = std::max(std::thread::hardware_concurrency(), 1u);
    uintmax_t hits = 0;
    std::mutex mtx;

    {
        std::vector<std::jthread> threads;

        for (int i = 0; i < num_threads; i++)
        {
            threads.push_back(std::jthread{calc_hits_per_thread_with_mutex, int(N / num_threads), std::ref(hits), std::ref(mtx)});
        }
    } // join

    const double pi = static_cast<double>(hits) / N * 4;

    const auto end = chrono::high_resolution_clock::now();
    const auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    std::cout << "Pi = " << pi << endl;
    std::cout << "Elapsed = " << elapsed_time << "ms" << endl;
}


void mc_pi_with_atomic()
{
    std::cout << "\n------------------------------------\n";
    cout << "Pi calculation started! Atomic!" << endl;
    const auto start = chrono::high_resolution_clock::now();

    std::atomic<uintmax_t> hits = 0;

    int num_threads = std::max(std::thread::hardware_concurrency(), 1u);
    std::vector<uintmax_t> hits_from_thread(num_threads);

    {
        std::vector<std::jthread> threads;

        for (int i = 0; i < num_threads; i++)
        {
            threads.push_back(std::jthread{calc_hits_per_thread_with_atomic, int(N / num_threads), std::ref(hits)});
        }
    } // join

    const double pi = static_cast<double>(hits) / N * 4;

    const auto end = chrono::high_resolution_clock::now();
    const auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "Pi = " << pi << endl;
    cout << "Elapsed = " << elapsed_time << "ms" << endl;
}


void mc_pi_with_futures()
{
    std::cout << "\n------------------------------------\n";
    cout << "Pi calculation started! Future is now!" << endl;
    const auto start = chrono::high_resolution_clock::now();

    int num_threads = std::max(std::thread::hardware_concurrency(), 1u);

    std::vector<std::future<uintmax_t>> futures;
    futures.reserve(num_threads);
    
    for (int i = 0; i < num_threads; i++)
    {
        futures.push_back(std::async(std::launch::async, calc_hits_per_thread_with_future, int(N / num_threads)));
    }

    uintmax_t hits = 0;
    for (auto& f: futures)
    {
        hits += f.get();
    }

    const double pi = static_cast<double>(hits) / N * 4;

    const auto end = chrono::high_resolution_clock::now();
    const auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "Pi = " << pi << endl;
    cout << "Elapsed = " << elapsed_time << "ms" << endl;
}

int main()
{
    mc_pi_one_thread();

    mc_pi_many_threads();

    mc_pi_many_threads_with_local_counter();

    mc_pi_many_threads_with_aligned_hits();

    mc_pi_with_atomic();

    mc_pi_with_mutex();

    mc_pi_with_futures();
}
