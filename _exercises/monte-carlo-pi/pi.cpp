#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <cstdlib>
#include <ctime>

using namespace std;

void calc_hits_one_thread(uintmax_t count, uintmax_t& hits)
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
            hits++;
    }
}

int main()
{
    const long N = 100'000'000;

    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    //////////////////////////////////////////////////////////////////////////////
    // single thread

    cout << "Pi calculation started!" << endl;
    const auto start = chrono::high_resolution_clock::now();
        
    uintmax_t hits = 0;

    calc_hits_one_thread(N, hits);

    const double pi = static_cast<double>(hits) / N * 4;

    const auto end = chrono::high_resolution_clock::now();
    const auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "Pi = " << pi << endl;
    cout << "Elapsed = " << elapsed_time << "ms" << endl;

    //////////////////////////////////////////////////////////////////////////////
}
