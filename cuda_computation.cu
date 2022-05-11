#include "cuda_computation.h"

#include "cuda_runtime.h"
#include <stdio.h>

__global__ void kernel(float *a, size_t n)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
    {
        a[i] = 2 * a[i];
    }
}

bool cuda_computation(float *a, size_t n)
{
    float *d_a;
    auto s = n * sizeof(float);

    auto rc = cudaMalloc(&d_a, s);
    if (rc != cudaSuccess)
    {
        return false;
    }

    rc = cudaMemcpy(d_a, a, s, cudaMemcpyHostToDevice);
    if (rc != cudaSuccess)
    {
        cudaFree(d_a);
        return false;
    }

    size_t constexpr BLOCKS = 8;
    const auto threads = (n + 1) / BLOCKS;

    kernel<<<BLOCKS, threads>>>(d_a, n);
    bool ret = cudaDeviceSynchronize() == cudaSuccess;
    ret &= cudaMemcpy(a, d_a, s, cudaMemcpyDeviceToHost) == cudaSuccess;

    cudaFree(d_a);

    return ret;
}