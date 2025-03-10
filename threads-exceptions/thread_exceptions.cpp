#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <variant>
#include <vector>

using namespace std::literals;

template <typename... Ts>
struct overloads : Ts...
{
    using Ts::operator()...;
};

template <typename T>
struct Result
{
    std::variant<std::monostate, T, std::exception_ptr> value_;

public:
    void set_exception(std::exception_ptr e)
    {
        value_ = e;
    }

    template <typename TValue>
    void set_value(TValue&& value)
    {
        value_ = std::forward<TValue>(value);
    }

    T get()
    {        
        return std::visit(overloads{
            [](const T& v) -> T {
                return v;
            },
            [](std::exception_ptr e) -> T {
                std::rethrow_exception(e);
            },
            [](std::monostate e) -> T {
                throw std::logic_error("Result is not set");
            }
        }, value_);
    }
};

void background_work(size_t id, const std::string& text, Result<char>& result)
{
    try
    {
        std::cout << "bw#" << id << " has started..." << std::endl;

        for (const auto& c : text)
        {
            std::cout << "bw#" << id << ": " << c << std::endl;

            std::this_thread::sleep_for(100ms);
        }

        result.set_value(text.at(5)); // potential exception

        std::cout << "bw#" << id << " is finished..." << std::endl;
    }
    catch (...)
    {
        result.set_exception(std::current_exception());
    }
}

struct PrintVisitor
{
    void operator()(int n)
    {
        std::cout << "int: " << n << "\n";
    }

    void operator()(double n)
    {
        std::cout << "double: " << n << "\n";
    }

    void operator()(const std::string& n)
    {
        std::cout << "std::string: " << n << "\n";
    }
};

void visit_demo()
{
    std::variant<int, double, std::string> v{4};

    // if (std::holds_alternative<double>(v))
    // {
    //     std::cout << std::get<double>(v) << "\n";
    // }

    std::visit(PrintVisitor{}, v);
}

int main()
{
    std::cout << "Main thread starts..." << std::endl;
    const std::string text = "Hello Threads";

    // char result1{};
    // std::exception_ptr eptr1;
    Result<char> result1;
    Result<char> result2;

    {
        std::jthread thd_1{&background_work, 1, "THREAD#1", std::ref(result1)};
        std::jthread thd_2{&background_work, 2, "T#2", std::ref(result2)};
    }

    try
    {
        std::cout << "result1: " << result1.get() << "\n";
        std::cout << "result2: " << result2.get() << "\n";
    }
    catch (const std::out_of_range& e)
    {
        std::cout << "Caught an exception: " << e.what() << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    // if (eptr1)
    // {
    //     try
    //     {
    //         std::rethrow_exception(eptr1);
    //     }
    //     catch(const std::out_of_range& e)
    //     {
    //         std::cout << "Caught an exception: " << e.what() << "\n";
    //     }
    //     catch(const std::exception& e)
    //     {
    //         std::cout << "Caught a general exception..." << "\n";
    //     }
    // }
    // else
    // {
    //     std::cout << "result1: " << result1 << "\n";
    // }

    // if (eptr2)
    // {
    //     try
    //     {
    //         std::rethrow_exception(eptr2);
    //     }
    //     catch(const std::out_of_range& e)
    //     {
    //         std::cout << "Caught an exception: " << e.what() << "\n";
    //     }
    //     catch(const std::exception& e)
    //     {
    //         std::cout << "Caught a general exception..." << "\n";
    //     }
    // }
    // else
    // {
    //     std::cout << "result2: " << result2 << "\n";
    // }

    std::cout << "Main thread ends..." << std::endl;

    visit_demo();
}

