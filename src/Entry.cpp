//
// Created by xusheng on 6/18/19.
//

#include "Entry.hpp"
#include <algorithm>
#include <butil/logging.h>

#define N_nodes 3

std::ostream & operator << (std::ostream &out, const Entry &c)
{
    out << "[ index: " << c.index;
    out << ", leader id: " << c.leader_id;
    out << ", content: " << c.data;
    out << ", term: " << c.term_;
    out << ", status: " << c.status << " ]" ;
    return out;
}

bool Entry::receive_ack(uint32_t id) {
    DLOG(INFO) << "received ack, index = " << index << ", node_id:" << id;

    std::lock_guard<std::mutex> l(mu);
    if (std::count(acked_nodes.begin(), acked_nodes.end(), id) == 0){
        ack_count++;
        if (ack_count + 1 >= (N_nodes + 1)/2){
            DLOG(INFO) << "[recv] committed entry, index = " << index;
            this->status = COMMITTED;
            return true;
        }
    }
    return false;
}

void Entry::commit() {
    std::lock_guard<std::mutex> l(mu);
    if (status != COMMITTED){
        status = COMMITTED;
        DLOG(INFO) << "[induction] committed entry, index = " << index;
    }
}