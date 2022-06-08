#pragma once

#include <iostream>

// lightweight, will contain only pointers to strings in the data segment
struct SourceLocation {
  const char *file;
  unsigned line;
  const char *function;
};

inline std::ostream &operator<<(std::ostream &s,
                                const SourceLocation &location) {
  s << "file " << location.file << " line " << location.line << " function "
    << location.function;
  return s;
}

#define CURRENT_SOURCE_LOCATION                                                \
  SourceLocation { __FILE__, __LINE__, __func__ }
