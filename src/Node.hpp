//
// Created by xusheng on 6/18/19.
//

#ifndef TRYRAFT_NODE_HPP
#define TRYRAFT_NODE_HPP

#include "Entry.hpp"
#include <thread>
#include <brpc/server.h>
#include <brpc/channel.h>
#include <butil/logging.h>
#include "raft.pb.h"
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <map>
#include <condition_variable>



enum roles{
    RAFT_LEADER = 1,
    RAFT_FOLLOWER = 2,
    RAFT_CANDIDATE = 0,
};



//The class for the
class Node: public raft::RaftServer, public std::enable_shared_from_this<Node>{
public:
    Node(uint32_t _node_id);
    void start();
    void wait();

    void AppendEntries(google::protobuf::RpcController* controller,
                       const raft::AppendEntriesReq* request,
                       raft::AppendEntriesReply* response,
                       google::protobuf::Closure* done) override;

    void RequestVote(google::protobuf::RpcController* controller,
                     const raft::RequestVoteReq* request,
                     raft::RequestVoteReply* response,
                     google::protobuf::Closure* done) override;

    void append(const std::string& data);


private:

    class AppendEntriesCallData{
    public:
        raft::AppendEntriesReq req;
        raft::AppendEntriesReply reply;
        brpc::Controller cntl;
        std::shared_ptr<Node> node;
        std::string remote_addr;
    };

    class RequestVoteCallData{
    public:
        raft::RequestVoteReq req;
        raft::RequestVoteReply reply;
        brpc::Controller cntl;
        std::shared_ptr<Node> node;
        std::string remote_addr;
    };

    std::mutex mu;

    std::condition_variable role_cond;

    roles my_role;
    uint32_t my_term;
    uint32_t my_id;

    std::vector<std::unique_ptr<Entry>> entries;
    uint32_t max_received_index;
    uint32_t max_committed_index;
    //rpc server
    brpc::Server server;
    std::thread t;
    brpc::ServerOptions options;

    //Leader election related datas
    int32_t voted_for;
    std::vector<uint32_t>  votes;

    void serveRPCs();

    std::map<std::string, std::unique_ptr<raft::RaftServer_Stub>> stubs;
//    std::vector<> stubs;std::unique_ptr<raft::RaftServer_Stub>

    //Keep it in case it is needed.
    std::vector<std::shared_ptr<brpc::Channel>> channels;

    static void onAppendEntriesComplete(std::shared_ptr<AppendEntriesCallData>);
    static void onRequestVoteComplete(std::shared_ptr<RequestVoteCallData> call_data);
    void commit(uint32_t up_to_index);

    void next_term();

    void do_view_change();

    void elect_for_leader();


    std::mutex timer_mu;
    std::condition_variable timer_cond;
    bool received_msg;

    inline uint32_t local_last_log_term();


    //Note: entries.size == local_last_log_index because local log start from index 1 (not 0).
    inline uint32_t local_last_log_index(){
        return entries.size();
    }

};




#endif //TRYRAFT_NODE_HPP
