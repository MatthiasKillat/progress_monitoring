# Progress Monitoring

Lightweight, mostly lock-free, monitoring of thread progress.

## Goals

1. Monitor progress of threads in critical sections (algorithms, lock acquisition)
1. Minimum overhead due to extra synchronization and monitoring logic
1. Can be disabled and causes no runtime overhead in this case
1. Monitored sections are explicitly marked and very visible in code

## Properties

1. Critical section has a time budget
    - budget implies deadline = section start + budget
    - deadline violation is detected either actively or passively
1. Passive monitoring
    - thread itself can detect deadline violations at the end of the critical section
    - detection may be too late for some use cases
1. Active monitoring
    - requires high priority thread (more overhead)
    - one thread per application
    - detects violation on a time grid (configurable)
    - can detect potential deadlocks (by timeout)
    - earlier detection by using e.g. priority queues (next deadline) would be much more expensive (and not lock-free)
    - accuracy depends on OS thread scheduling

1. Lock-free
    - initialization is currently not lock-free (can be changed)
    - actual monitoring is lock-free (TODO: verify synchronization logic)

1. Minimal thread synchronization
    - only required for initialization per thread and in active mode
    - uses thread local state to track progress

1. Limited number of threads to monitor
   - due to static memory model
   - number of threads can be configured

1. Robust time measurement
    - accuracy depends on clock (uses monotonic clock)
    - can deal with overflow of the clock (relative measurements)
    - max time budget is limited to half the 64 bit integer range time units (sufficient if time units are milliseconds or even microseconds)

1. Macro API allows zero overhead if not used
    - increases visibility in code
    - no checks (nullptr etc.) that are not necessary when the API is correctly used (to minimize overhead)

1. Configurable reaction on deadline violation
    - handler function can be installed (TODO: improve interface)
    - handler increases overhead (mainly in the violation case)

## Future Goals

1. Gather runtime statistics of critical sections
1. Support RT OS for accurate and reliable monitoring
1. Make completely lock-free by using a lock-free slab allocator build out of iceoryx building blocks
1. Make header-only for even easier integration (?)
