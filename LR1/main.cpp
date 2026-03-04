#include <iostream>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <chrono>

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

int main() {
    
    double a, b, h, eps;
    long long max_iterations = 10000;

    cout << "Input a (0.1): ";
    cin >> a;
    cout << "Input b (1.0): ";
    cin >> b;
    cout << "Input h (0.1): ";
    cin >> h;
    cout << "Input eps (0.0001): ";
    cin >> eps;

    ofstream csv("data.csv");
    csv << "Time,Iterations\n";

    cout << "Starting CPU test (SMT ON/OFF)...";

    auto start_time = chrono::high_resolution_clock::now();

    for(long long i = 1; i <= max_iterations; ++i) {
        
        bool is_last_iteration = i == max_iterations;

        if(is_last_iteration) {
            cout << string(65, '-') << endl;
            cout << "|" << setw(8) << "x" << " |" << setw(15) << "Y(x)" 
                 << " |" << setw(15) << "S(x)" << " |" << setw(15) << "Итераций (n)" << " |" << endl;
            cout << string(65, '-') << endl;
        }

        for(double x = a; x <= b; x += h) {
            
            double pi_4 = M_PI / 4;
            double y_val = exp(x * cos(pi_4)) * cos(x * sin(pi_4));
            double s_val = 0.0;
            double current_term = 0.0;
            int k = 0;

            do{

                double numerator = asm_mul(cos(k * pi_4), pow(x, k));
                double denominator = asm_factorial(k);
                current_term = numerator / denominator;
                s_val = asm_add(s_val, current_term);
                ++k;

            } while(abs(y_val - s_val) >= eps && k < 100);

            if(is_last_iteration) {
                cout << "|" << fixed << setprecision(2) << setw(8) << x 
                     << " |" << fixed << setprecision(8) << setw(15) << y_val 
                     << " |" << fixed << setprecision(8) << setw(15) << s_val 
                     << " |" << setw(15) << (k - 1) << " |" << endl;
            }
        }

        if (is_last_iteration) {
            cout << string(65, '-') << endl;
        }

        if(i % 100 == 0) {
            auto current_time = chrono::high_resolution_clock::now();
            chrono::duration<double> elapsed = current_time - start_time;
            csv << elapsed.count() << "," << i << endl;
        }
    }

    csv.close();

    cout << "Succesfull create data.csv" << endl;
    
    return 0;
}