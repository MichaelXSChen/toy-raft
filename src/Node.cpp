//
// Created by xusheng on 6/18/19.
//

#include "Node.hpp"
#include <butil/logging.h>
#include <brpc/server.h>

void Node::AppendEntries(google::protobuf::RpcController* cntl_base,
                   const raft::AppendEntriesReq* request,
                   raft::AppendEntriesReply* response,
                   google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);


    //Seems not needed to touch the config for now.
    //brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);



    std::lock_guard<std::mutex> l(mu);
    if (my_role == RAFT_FOLLOWER){
        for (const auto &e : request->entries()){
            if (e.index() == max_received_index + 1){
                auto entry = std::make_unique<Entry> (e.index(), 0, RECEIVED);
                DLOG(INFO) << "Received request, index " << *entry;
                entries.push_back(std::move(entry));
                max_received_index++;
            }
        }
        response->set_success(true);
        response->set_term(my_term);

    }

}