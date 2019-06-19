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

    DLOG(INFO) << "Append entries called";

    std::lock_guard<std::mutex> l(mu);
    if (my_role == RAFT_FOLLOWER){
        for (const auto &e : request->entries()){
            DLOG(INFO) << "Received request, index " << e.index();

            if (e.index() == max_received_index + 1){
                auto entry = std::make_unique<Entry> (e.data(), e.index(), 0, RECEIVED);
                DLOG(INFO) << "Pushing entry" << *entry;

                entries.push_back(std::move(entry));

                max_received_index++;
            }
        }
        response->set_success(true);
        response->set_term(my_term);
        response->set_max_index(max_received_index);
        response->set_sender_id(id);
    }
}

void Node::start() {
    t = std::thread(&Node::serve, this);
    DLOG(INFO) << "service started";


    if (my_role == RAFT_LEADER){

        int count = 0;
        while(!brpc::IsAskedToQuit()){
            append(std::to_string(count));
            sleep(1);
        }
    }
}

void Node::wait(){
    t.join();
}

void Node::serve() {
    if (server.AddService(this, brpc::SERVER_DOESNT_OWN_SERVICE) != 0){
        LOG(FATAL) << "Fail to add service";
    }
    if (server.Start(id + 10000, &options) != 0) {
        LOG(FATAL) << "Fail to start EchoServer";
    }
    server.RunUntilAskedToQuit();
}

void Node::append(const std::string& data) {
    std::lock_guard<std::mutex> l(mu);

    if (my_role != RAFT_LEADER){
        LOG(WARNING) << " I am not leader, giving up on proposing";
        return;
    }
    max_received_index++;
    auto e = std::make_unique<Entry>(data, max_received_index, id, PROPOSED);
    DLOG(INFO) << "Proposing entry" << *e;
    entries.push_back(std::move(e));



    auto call_data = std::make_shared<AppendEntriesCallData>();

    call_data->node = shared_from_this();

    call_data->req.set_term(my_term);
    call_data->req.set_sender_id(id);
    auto entry = call_data->req.add_entries();
    entry->set_data(data);
    entry->set_index(max_received_index);


    for (const auto & stub: follower_stubs){



        stub->AppendEntries(&call_data->cntl, &call_data->req, &call_data->reply, google::protobuf::NewCallback(Node::onAppendEntriesComplete, call_data));
        if (!call_data->cntl.Failed()) {
            DLOG(INFO) << "follower response";
        }else{
            LOG(FATAL) << "rpc failed: " << call_data->cntl.ErrorText() ;
        }
    }
}

void Node::onAppendEntriesComplete(std::shared_ptr<AppendEntriesCallData> call_data) {

    if (call_data->cntl.Failed()){
        LOG(FATAL) << "Append entries failed, not implemented, message: " << call_data->cntl.ErrorText();
    }
    else{
        std::lock_guard<std::mutex> l(call_data->node->mu);
        if (call_data->node->entries.size() < call_data->reply.max_index()){
            LOG(FATAL) << "error handling ack, size: " << call_data->node->entries.size() << ", index = " << call_data->reply.max_index();
        }
        bool committed = call_data->node->entries[call_data->reply.max_index()-1]->receive_ack(call_data->reply.sender_id());
        if (committed){
            call_data->node->commit(call_data->reply.max_index());
        }
    }
}

/*
 * IMPORTANT: this function must be called when the global lock is held.
 */
void Node::commit(uint32_t up_to_index) {
    for (uint32_t i = max_committed_index; i < up_to_index; i++){
        entries[i]->commit();
    }
}

