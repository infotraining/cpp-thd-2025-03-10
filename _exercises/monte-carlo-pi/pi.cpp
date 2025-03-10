#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <thread>

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
            ++hits;
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
            ++local_hits;
    }

    hits += local_hits;
}

struct Hits
{
    alignas(std::hardware_destructive_interference_size) uintmax_t value;
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

void mc_pi_many_threads_with_aligned_hits()
{
    std::cout << "\n------------------------------------\n";
    std::cout << "Pi calculation started! Many threads - aligned hits" << endl;
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

void mc_pi_with_atomic()
{
    const long N = 100'000'000;

    //////////////////////////////////////////////////////////////////////////////
    // single thread

    std::cout << "\n------------------------------------\n";
    cout << "Pi calculation started! Atomic!" << endl;
    const auto start = chrono::high_resolution_clock::now();

    std::atomic<long> hits = 0;

    std::vector<std::thread> threads;
    threads.reserve(std::thread::hardware_concurrency());

    for (unsigned i = 0; i < std::thread::hardware_concurrency(); i++)
    {
        threads.emplace_back([&hits](long const iters) {
            std::mt19937_64 rnd_gen(std::hash<std::thread::id>{}(std::this_thread::get_id()));
            std::uniform_real_distribution rnd_dist(0.0, 1.0);

            long hits_local = 0;

            for (long n = 0; n < iters; ++n)
            {
                double const x = rnd_dist(rnd_gen);
                double const y = rnd_dist(rnd_gen);

                if (x * x + y * y < 1)
                    hits_local++;
            }

            hits += hits_local;
        },
            N / std::thread::hardware_concurrency());
    }

    for (auto& thread : threads)
        thread.join();

    const double pi = static_cast<double>(hits.load()) / N * 4;

    const auto end = chrono::high_resolution_clock::now();
    const auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "Pi = " << pi << endl;
    cout << "Elapsed = " << elapsed_time << "ms" << endl;

    //////////////////////////////////////////////////////////////////////////////
}

int main()
{
    mc_pi_one_thread();

    mc_pi_many_threads();

    mc_pi_many_threads_with_local_counter();

    mc_pi_many_threads_with_aligned_hits();

    mc_pi_with_atomic();
}
