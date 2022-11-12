#pragma once

// #define MONITORING_MODE_PASSIVE
#define MONITORING_MODE_ACTIVE

// we can turn the output of (for e.g. measurement),
// the handler will still be called(if any)

// #define DEADLINE_VIOLATION_OUTPUT_ON

// activation will kill performance due to map access under mutex!
// statistic tracking must be refactored to work mostly local
// #define MONITORING_STATS
