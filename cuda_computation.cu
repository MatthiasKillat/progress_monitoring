#include "cuda_computation.h"

#include "cuda_runtime.h"
#include <stdio.h>

__global__ void kernel(float *a, size_t n)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
    {
        a[i] = a[i] + 1;
    }
}

constexpr auto threads = 8;

void *get_device_buffer(size_t s)
{
    void *device;
    auto rc = cudaMalloc(&device, s);
    if (rc == cudaSuccess)
    {
        return device;
    }
    return nullptr;
}

void free_device_buffer(void *device)
{
    cudaFree(device);
}

bool cuda_computation1(float *host, size_t n)
{
    auto s = n * sizeof(float);
    float *device = (float *)get_device_buffer(s);

    if (!device)
    {
        return false;
    }

    auto rc = cudaMemcpy(device, host, s, cudaMemcpyHostToDevice);
    if (rc != cudaSuccess)
    {
        free_device_buffer(device);
        return false;
    }

    const size_t blocks = (n + threads - 1) / threads;

    kernel<<<blocks, threads>>>(device, n);
    bool ret = cudaDeviceSynchronize() == cudaSuccess;
    ret &= cudaMemcpy(host, device, s, cudaMemcpyDeviceToHost) == cudaSuccess;

    free_device_buffer(device);

    return ret;
}
