//
// Created by xusheng on 6/23/19.
//
#include "flags.hpp"


DEFINE_uint64(id, 0, "the id of the node, 0 as the leader");
DEFINE_uint64(n_nodes, 3, "the total number of nodes, 3 by default");
DEFINE_string(ips, "127.0.0.1, 127.0.0.1, 127.0.0.1", "the ips of the nodes");
DEFINE_string(ports, "10000,10001,10002", "the ports of the nodes");




// trim from start (in place)
void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

std::vector<std::string> parse_ips(){

    std::vector<std::string> ips;
    std::string s = FLAGS_ips;
    char delimiter = ',';

    size_t pos = 0;
    size_t prev = 0;
    do{
        pos = s.find(delimiter, prev);
        std::string ip = s.substr(prev, pos-prev);
        trim(ip);
        DLOG(INFO) << "IP: ["<<ip<<"]";
        prev = pos+1;
        ips.push_back(ip);
    }while(pos != std::string::npos);
    return ips;
}


std::vector<std::string> parse_ports(){
    std::vector<std::string> ports;
    std::string s = FLAGS_ports;
    char delimiter = ',';

    size_t pos = 0;
    size_t prev = 0;
    do{
        pos = s.find(delimiter, prev);
        std::string port = s.substr(prev, pos-prev);
        trim(port);
        DLOG(INFO) << "Port: ["<<port<<"]";
        prev = pos+1;
        ports.push_back(port);
    }while(pos != std::string::npos);

    return ports;
};
