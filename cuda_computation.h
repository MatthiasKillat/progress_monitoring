#pragma once

extern bool cuda_computation1(float *host, size_t n);

extern void *get_device_buffer(size_t n);

extern void free_device_buffer(void *a);

// TODO: second computation that reuses device buffer
// extern bool cuda_computation2(float *host, float *device, size_t n);