#pragma once

#include <iostream>

// lightweight, contains only pointers to strings in the data segment
// (however, the strings increase the binary size so we may not want to use
// locations after all)
struct source_location {
  const char *file;
  unsigned line;
  const char *function;
};

inline std::ostream &operator<<(std::ostream &s,
                                const source_location &location) {
  s << "file " << location.file << " line " << location.line << " function "
    << location.function;
  return s;
}

#define THIS_SOURCE_LOCATION                                                   \
  source_location { __FILE__, __LINE__, __func__ }
