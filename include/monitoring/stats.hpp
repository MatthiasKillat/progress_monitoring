#pragma once

// TODO: refactor dependencies
#include "source_location.hpp"
#include "time.hpp"

#include "stack/entry.hpp"

#include <atomic>
#include <cmath>
#include <limits>
#include <map>
#include <mutex>

// TODO: optimization,
// can do this thread local and merge the results later namespace monitor {

namespace monitor {

struct stats {

  stats(checkpoint_id_t id) : id(id) {}
  source_location location;
  checkpoint_id_t id{0};

  uint64_t count{0};
  uint64_t violations{0};
  uint64_t min{std::numeric_limits<uint64_t>::max()};
  uint64_t max{0};
  double mean{0};
  double meanOfSquares{0};

  void print() {
    std::cout << "checkpoint id " << id << std::endl;
    std::cout << "count : " << count << std::endl;
    std::cout << "violations : " << violations << std::endl;
    std::cout << "min : " << min << std::endl;
    std::cout << "max : " << max << std::endl;
    std::cout << "mean : " << mean << std::endl;
    std::cout << "standard deviation : " << std::sqrt(variance()) << std::endl;
  }

  double variance() {
    if (count < 2) {
      return 0;
    }
    // estimate based on VarX = E[X*X] - E[X]*E[X]
    // we keep the totals Tk = Sum X_i ^ k, hence
    // n = count = T0,
    // mean = T1 / n
    // meanOfSquares = T2/n
    // and can then estimate the variance as
    // C * (n * T2 - T1*T1) / (n*n)
    // with correction factor for bias
    // C = n/(n-1)
    double n = double(count);
    double m1 = mean;
    double m2 = meanOfSquares;
    return (n / (n - 1)) * (m2 - m1 * m1);
  }
};

class stats_monitor {
public:
  static void update(checkpoint_id_t id, time_t runtime,
                     bool violation = false) {

    // TODO: avoid these blocking between unrelated threads
    // and gather stats in local maps and merge them when the threads
    // end
    auto &inst = instance();
    std::lock_guard<std::mutex> g(inst.m_mutex);
    auto &stats = inst.get(id);

    if (violation) {
      ++stats.violations;
    }
    ++stats.count;

    if (runtime < stats.min) {
      stats.min = runtime;
    }
    if (runtime > stats.max) {
      stats.max = runtime;
    }

    // incremental computation of mean and variance

    double t = double(runtime);
    double n = stats.count;
    auto m1 = stats.mean;
    stats.mean = (t + (n - 1) * m1) / n;

    auto m2 = stats.meanOfSquares;
    stats.meanOfSquares = (t * t + (n - 1) * m2) / n;
  }

  static stats_monitor &instance() {
    static stats_monitor instance;
    return instance;
  }

  static void print() {
    auto &inst = instance();
    std::lock_guard<std::mutex> g(inst.m_mutex);
    for (auto &pair : inst.m_stats) {
      pair.second.print();
      std::cout << std::endl;
    }
  }

private:
  stats_monitor() = default;
  stats_monitor(const stats_monitor &) = delete;

  std::mutex m_mutex;
  std::map<checkpoint_id_t, stats> m_stats;

  stats &get(checkpoint_id_t id) {

    auto iter = m_stats.find(id);
    if (iter != m_stats.end()) {
      return iter->second;
    }
    // TODO: can actually fail, need to handle this
    auto pair = m_stats.emplace(std::make_pair(id, stats(id)));
    return (pair.first)->second;
  }
};

} // namespace monitor