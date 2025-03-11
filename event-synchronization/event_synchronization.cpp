#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <syncstream>
#include <thread>
#include <vector>

std::osyncstream synced_cout()
{
    return std::osyncstream{std::cout};
}

using namespace std::literals;

namespace BusyWait
{
    namespace WithMutex
    {
        class Data
        {
            std::vector<int> data_;
            bool is_data_ready_ = false;
            std::mutex mtx_is_data_ready_;

        public:
            void produce()
            {
                synced_cout() << "Start reading..." << std::endl;
                data_.resize(100);

                std::random_device rnd;
                std::generate(begin(data_), end(data_), [&rnd] { return rnd() % 1000; });
                std::this_thread::sleep_for(2s);
                synced_cout() << "End reading..." << std::endl;

                {
                    std::lock_guard lk{mtx_is_data_ready_};
                    is_data_ready_ = true;
                }
            } // unlock

            void consume(int id)
            {
                bool data_ready;

                do
                {
                    std::lock_guard lk{mtx_is_data_ready_};
                    data_ready = is_data_ready_;
                } while (!data_ready);

                long sum = std::accumulate(begin(data_), end(data_), 0L);
                synced_cout() << "Id: " << id << "; Sum: " << sum << std::endl;
            }
        };
    } // namespace WithMutex

    class Data
    {
        std::vector<int> data_;
        std::atomic<bool> is_data_ready_ = false;

    public:
        void produce()
        {
            synced_cout() << "Start reading..." << std::endl;
            data_.resize(100);

            std::random_device rnd;
            std::generate(begin(data_), end(data_), [&rnd] { return rnd() % 1000; });
            std::this_thread::sleep_for(2s);
            synced_cout() << "End reading..." << std::endl;

            is_data_ready_.store(true, std::memory_order_release);

        } // unlock

        void consume(int id)
        {
            while (!is_data_ready_.load(std::memory_order_acquire))
            { }

            long sum = std::accumulate(begin(data_), end(data_), 0L);
            synced_cout() << "Id: " << id << "; Sum: " << sum << std::endl;
        }
    };
} // namespace BusyWait

namespace IdleWaits
{
    namespace WithCV
    {
        class Data
        {
            std::vector<int> data_;
            bool is_data_ready_ = false;
            std::mutex mtx_is_data_ready_;
            std::condition_variable cv_data_ready_;

        public:
            void produce()
            {
                synced_cout() << "Start reading..." << std::endl;
                data_.resize(100);

                std::random_device rnd;
                std::generate(begin(data_), end(data_), [&rnd] { return rnd() % 1000; });
                std::this_thread::sleep_for(2s);
                synced_cout() << "End reading..." << std::endl;

                {
                    std::lock_guard lk{mtx_is_data_ready_};
                    is_data_ready_ = true;
                }
                cv_data_ready_.notify_all();
            } // unlock

            void consume(int id)
            {
                {
                    std::unique_lock lk{mtx_is_data_ready_};
                    cv_data_ready_.wait(lk, [this] { return is_data_ready_; });
                }

                long sum = std::accumulate(begin(data_), end(data_), 0L);
                synced_cout() << "Id: " << id << "; Sum: " << sum << std::endl;
            }
        };
    } // namespace WithCV
    
    inline namespace WithAtomics
    {
        class Data
        {
            std::vector<int> data_;
            std::atomic<bool> is_data_ready_ = false;            

        public:
            void produce()
            {
                synced_cout() << "Start reading..." << std::endl;
                data_.resize(100);

                std::random_device rnd;
                std::generate(begin(data_), end(data_), [&rnd] { return rnd() % 1000; });
                std::this_thread::sleep_for(2s);
                synced_cout() << "End reading..." << std::endl;

                is_data_ready_ = true;
                is_data_ready_.notify_all();
            } // unlock

            void consume(int id)
            {
                is_data_ready_.wait(false);

                long sum = std::accumulate(begin(data_), end(data_), 0L);
                synced_cout() << "Id: " << id << "; Sum: " << sum << std::endl;
            }
        };
    } // namespace WithAtomics
} // namespace IdleWaits

int main()
{
    static_assert(std::atomic<double>::is_always_lock_free);

    using namespace IdleWaits;

    {
        Data data;
        std::jthread thd_producer{[&data] {
            data.produce();
        }};

        std::jthread thd_consumer_1{[&data] {
            data.consume(1);
        }};
        std::jthread thd_consumer_2{[&data] {
            data.consume(2);
        }};
    }

    synced_cout() << "END of main..." << std::endl;
}
