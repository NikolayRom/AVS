#include <iostream>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>

using namespace std;

inline double asm_add(double a, double b) {
    double result;
    __asm__ __volatile__(
        "fldl %1 \n\t"
        "fldl %2 \n\t"
        "faddp %%st, %%st(1) \n\t"
        "fstpl %0 \n\t"
        : "=m" (result)
        : "m" (b), "m" (a)
    );
    return result;
}

inline double asm_mul(double a, double b) {
    double result;
    __asm__ __volatile__(
        "fldl %1 \n\t"
        "fldl %2 \n\t"
        "fmulp %%st, %%st(1) \n\t"
        "fstpl %0 \n\t"
        : "=m" (result)
        : "m" (b), "m" (a)
    );
    return result;
}

inline double asm_factorial(int n) {
    if(n == 0 || n == 1) return 1.0;
    double result = 1.0;

    for(int i = 2; i <= n; ++i) {
        double d_i = (double)i;
        __asm__ __volatile__(
            "fldl %1 \n\t"
            "fldl %0 \n\t"
            "fmulp %%st, %%st(1) \n\t"
            "fstpl %0 \n\t"
            : "+m" (result)
            : "m" (d_i)
        );
    }
    return result;
}

std::atomic<long long> global_completed_iters(0);
std::atomic<bool> start_flag(false);

void worker_task(long long iters_for_this_thread, double a, double b, double h, double eps) {
    
    while(!start_flag.load(std::memory_order_relaxed)){}
    
    double pi_4 = M_PI / 4.0;

    for(long long iter = 0; iter < iters_for_this_thread; ++iter) {
        for(double x = a; x <= b; x += h) {
            double y_val = exp(x * cos(pi_4)) * cos(x * sin(pi_4));
            double s_val = 0.0;
            double current_term = 0.0;
            int k = 0;

            do {

                double numerator = asm_mul(cos(k * pi_4), pow(x, k));
                double denominator = asm_factorial(k);
                current_term = numerator / denominator;
                s_val = asm_add(s_val, current_term);
                ++k;

            } while(abs(y_val - s_val) >= eps && k < 100);
        }
    
        global_completed_iters.fetch_add(1, std::memory_order_relaxed);
    }
}

int main() {
    
    double a = 0.1, b = 1.0, h = 0.1, eps = 0.0001;
    long long max_iterations = 10000;

    unsigned int num_threads = std::thread::hardware_concurrency();

    if(num_threads == 0) {
        num_threads = 16;
    }

    cout << "Lab 2:\nDefault values:" << endl;
    cout << "a: " << a << endl;
    cout << "b: " << b << endl;
    cout << "h: " << h << endl;
    cout << "eps: " << eps << endl;
    cout << "max_iterations: " << max_iterations << endl;
    cout << "num_threads: " << num_threads << endl;

    ofstream csv("data.csv");
    csv << "Time,Iterations\n";

    cout << "Starting CPU test (SMT ON/OFF)..." << endl;

    global_completed_iters = 0;

    long long iters_per_thread = max_iterations / num_threads;

    vector<std::thread> threads;

    for(unsigned int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread(worker_task, iters_per_thread, a, b, h, eps));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    auto start_time = chrono::high_resolution_clock::now();

    start_flag.store(true, std::memory_order_relaxed);

    long long record_step = max_iterations / 100;
    if(record_step == 0) {
        record_step = 1;
    }

    long long next_target = record_step;

    while(true) {
        long long current_iters = global_completed_iters.load(std::memory_order_relaxed);
        if(current_iters >= next_target || current_iters >= max_iterations) {
            auto current_time = chrono::high_resolution_clock::now();
            chrono::duration<double> elapsed = current_time - start_time;
            csv << elapsed.count() << "," << current_iters << "\n";
            next_target = current_iters + record_step;
        }
        if(current_iters >= (num_threads * iters_per_thread)) {
            break;
        }
    }

    for(auto& t : threads) {
        t.join();
    }
    
    csv.close();

    cout << "Succesfull create data.csv" << endl;
    
    return 0;
}