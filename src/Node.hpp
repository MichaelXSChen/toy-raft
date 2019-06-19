//
// Created by xusheng on 6/18/19.
//

#ifndef TRYRAFT_NODE_HPP
#define TRYRAFT_NODE_HPP

#include <vector>

#include <string>
#include <mutex>
#include <brpc/server.h>
#include <brpc/channel.h>


#include "Entry.hpp"
#include "raft.pb.h"
#include <thread>

enum roles{
    RAFT_LEADER = 1,
    RAFT_FOLLOWER = 2,
    RAFT_CANDIDATE = 0,
};

//The class for the
class Node: public raft::RaftServer, public std::enable_shared_from_this<Node>{
public:
    Node(uint32_t _node_id):
        my_term(1), id(_node_id), max_received_index(0), max_committed_index(0){
        if (id == 0){
            my_role = RAFT_LEADER;
        }else{
            my_role = RAFT_FOLLOWER;
        }


        for (int i = 0; i<3; i++){
            if (uint(i) != id){
                //create a channel to it.
                auto channel = std::make_shared<brpc::Channel>();

                if ( channel->Init( ("127.0.0.1:" + std::to_string(10000+i)).c_str(), NULL) != 0){
                    LOG(FATAL) << "Failed to create channel";
                }
                channels.push_back(channel);
                auto stub = std::make_unique<raft::RaftServer_Stub>(channel.get(), STUB_DOESNT_OWN_CHANNEL);
                stubs.push_back(std::move(stub));
                DLOG(INFO) << "created a channel and stub to 1000" << i;
            }
        }

    }
    void start();
    void wait();

    void AppendEntries(google::protobuf::RpcController* controller,
                       const raft::AppendEntriesReq* request,
                       raft::AppendEntriesReply* response,
                       google::protobuf::Closure* done) override;

    void append(const std::string& data);


private:

    class AppendEntriesCallData{
    public:
        raft::AppendEntriesReq req;
        raft::AppendEntriesReply reply;
        brpc::Controller cntl;
        std::shared_ptr<Node> node;
    };

    std::mutex mu;

    roles my_role;
    uint32_t my_term;
    uint32_t id;

    std::vector<std::unique_ptr<Entry>> entries;
    uint32_t max_received_index;
    uint32_t max_committed_index;
    //rpc server
    brpc::Server server;
    std::thread t;
    brpc::ServerOptions options;

    void serve();


    std::vector<std::unique_ptr<raft::RaftServer_Stub>> stubs;
    std::vector<std::shared_ptr<brpc::Channel>> channels;

    static void onAppendEntriesComplete(std::shared_ptr<AppendEntriesCallData>);

    void commit(uint32_t up_to_index);



};




#endif //TRYRAFT_NODE_HPP
