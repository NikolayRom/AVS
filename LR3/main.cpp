#define CL_TARGET_OPENCL_VERSION 120
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS 
#include <CL/cl.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <cmath>

using namespace std;

const char* kernel_source = R"CLC(
float factorial(int n) {
    if (n == 0 || n == 1) return 1.0f;
    float res = 1.0f;
    for (int i = 2; i <= n; ++i) res *= (float)i;
    return res;
}

__kernel void math_kernel(float a, float b, float h, float eps) {
    int id = get_global_id(0);
    float pi_4 = 0.78539816f;

    for (float x = a; x <= b + h/2.0f; x += h) {
        float y_val = exp(x * cos(pi_4)) * cos(x * sin(pi_4));
        float s_val = 0.0f;
        float current_term = 0.0f;
        int k = 0;

        do {
            float num = cos((float)k * pi_4) * pown(x, k); 
            float den = factorial(k);
            current_term = num / den;
            s_val += current_term;
            k++;
        } while (fabs(y_val - s_val) >= eps && k < 100);
    }
}
)CLC";

void checkError(cl_int err, const char* operation) {
    if (err != CL_SUCCESS) {
        cerr << "Ошибка OpenCL во время: " << operation << " (Код: " << err << ")\n";
        exit(1);
    }
}

int main() {
    float a = 0.1f, b = 1.0f, h = 0.1f, eps = 0.0001f;
    
    long long max_iterations = 1000000;
    int steps = 100;
    size_t iters_per_step = max_iterations / steps;

    cout << "--- Lab 3: GPU Acceleration (OpenCL / AMD) ---" << endl;

    cl_uint num_platforms;
    clGetPlatformIDs(0, NULL, &num_platforms);
    vector<cl_platform_id> platforms(num_platforms);
    clGetPlatformIDs(num_platforms, platforms.data(), NULL);

    cl_device_id device_id = NULL;
    for(size_t i = 0; i < platforms.size(); i++) {
        char plat_name[128];
        clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, sizeof(plat_name), plat_name, NULL);
        
        cl_uint num_devices = 0;
        if(clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 1, &device_id, &num_devices) == CL_SUCCESS) {
            break;
        }
    }

    if (!device_id) {
        cerr << "\nОШИБКА: Видеокарта (GPU) не найдена!\n";
        return 1;
    }

    char device_name[128];
    clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    cout << "Используется видеокарта: " << device_name << endl;
    cout << "Запуск вычислений (" << max_iterations << " итераций). Ждите...\n\n";

    cl_int err;
    cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &err);
    cl_command_queue queue = clCreateCommandQueue(context, device_id, 0, &err);

    cl_program program = clCreateProgramWithSource(context, 1, &kernel_source, NULL, &err);
    err = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        char build_log[4096];
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(build_log), build_log, NULL);
        cerr << "Ошибка компиляции ядра:\n" << build_log << endl;
        return 1;
    }

    cl_kernel kernel = clCreateKernel(program, "math_kernel", &err);

    clSetKernelArg(kernel, 0, sizeof(float), &a);
    clSetKernelArg(kernel, 1, sizeof(float), &b);
    clSetKernelArg(kernel, 2, sizeof(float), &h);
    clSetKernelArg(kernel, 3, sizeof(float), &eps);

    ofstream csv("data.csv");
    csv << "Time,Iterations\n";

    auto start_time = chrono::high_resolution_clock::now();
    long long current_iters = 0;

    for (int step = 1; step <= steps; ++step) {
        err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &iters_per_step, NULL, 0, NULL, NULL);
        checkError(err, "Запуск ядра");

        clFinish(queue);

        current_iters += iters_per_step;

        auto current_time = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed = current_time - start_time;
        
        csv << elapsed.count() << "," << current_iters << "\n";
    }

    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    csv.close();

    cout << "Успешно! Файл data.csv создан." << endl;
    return 0;
}