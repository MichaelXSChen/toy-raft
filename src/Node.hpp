//
// Created by xusheng on 6/18/19.
//

#ifndef TRYRAFT_NODE_HPP
#define TRYRAFT_NODE_HPP

#include <vector>

#include <string>
#include <mutex>

#include "Entry.hpp"
#include "raft.pb.h"

enum roles{
    RAFT_LEADER = 1,
    RAFT_FOLLOWER = 2,
    RAFT_CANDIDATE = 0,
};

//The class for the
class Node: public raft::follower{
public:

private:
    std::mutex mu;

    roles my_role;
    uint32_t my_term;


    std::vector<std::unique_ptr<Entry>> entries;
    uint32_t max_received_index;





    void AppendEntries(google::protobuf::RpcController* controller,
                               const raft::AppendEntriesReq* request,
                               raft::AppendEntriesReply* response,
                               google::protobuf::Closure* done) override;





};




#endif //TRYRAFT_NODE_HPP
