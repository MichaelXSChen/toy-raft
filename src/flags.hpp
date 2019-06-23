//
// Created by xusheng on 6/23/19.
//

#ifndef TRYRAFT_FLAGS_HPP
#define TRYRAFT_FLAGS_HPP


#include <gflags/gflags.h>
#include "butil/logging.h"


DECLARE_uint64(id);
DECLARE_uint64(n_nodes);
DECLARE_string(ips);
DECLARE_string(ports);

// trim from start (in place)
inline void ltrim(std::string &s);

// trim from end (in place)
inline void rtrim(std::string &s);

// trim from both ends (in place)
inline void trim(std::string &s);

std::vector<std::string> parse_ips();

std::vector<std::string> parse_ports();




#endif //TRYRAFT_FLAGS_HPP
