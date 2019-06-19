//
// Created by xusheng on 6/18/19.
//

#include "Node.hpp"
#include <butil/logging.h>
#include <brpc/server.h>

#define N_nodes 3


void Node::AppendEntries(google::protobuf::RpcController* cntl_base,
                   const raft::AppendEntriesReq* request,
                   raft::AppendEntriesReply* response,
                   google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);


    //Seems not needed to touch the config for now.
    //brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);

    DLOG(INFO) << "Append entries called, leader_commit = " << request->leadercommit();

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
        response->set_sender_id(my_id);

        this->commit(request->leadercommit());
    }
    else if (my_role == RAFT_CANDIDATE){
        if (request->term() >= my_term){
            my_term = request->term();
            my_role = RAFT_FOLLOWER;
            DLOG(INFO) << "[election] received leader message, changed to be follower, term " << my_term;
        }


    }
}

void Node::start() {
    t = std::thread(&Node::serveRPCs, this);
    DLOG(INFO) << "service started";


    while(true){
        std::unique_lock<std::mutex> l(mu);

        if (my_role != RAFT_CANDIDATE){
            break;
        }

        l.unlock();

        next_term();

        elect_for_leader();

        sleep(1);
    }


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

void Node::serveRPCs() {
    if (server.AddService(this, brpc::SERVER_DOESNT_OWN_SERVICE) != 0){
        LOG(FATAL) << "Fail to add service";
    }
    if (server.Start(my_id + 10000, &options) != 0) {
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
    auto e = std::make_unique<Entry>(data, max_received_index, my_id, PROPOSED);
    DLOG(INFO) << "Proposing entry" << *e;
    entries.push_back(std::move(e));



    auto call_data = std::make_shared<AppendEntriesCallData>();

    call_data->node = shared_from_this();

    call_data->req.set_term(my_term);
    call_data->req.set_sender_id(my_id);
    call_data->req.set_leadercommit(max_committed_index);

    auto entry = call_data->req.add_entries();
    entry->set_data(data);
    entry->set_index(max_received_index);


    for (const auto & stub: stubs){
        stub->AppendEntries(&call_data->cntl, &call_data->req, &call_data->reply, google::protobuf::NewCallback(Node::onAppendEntriesComplete, call_data));
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
    //Note that entry index start from 1, but vector start from 0.
    for (uint32_t i = max_committed_index; i < up_to_index; i++){
        entries[i]->commit();
    }
    max_committed_index = up_to_index;
}

void Node::RequestVote(google::protobuf::RpcController *controller, const raft::RequestVoteReq *request,
                       raft::RequestVoteReply *response, google::protobuf::Closure *done) {
    response->set_sender_id(my_id);
    response->set_term(my_term);
    if (request->term() < my_term){
        response->set_votegranted(false);
    }
    else{
        //not for a smaller term.
        if (voted_for != -1){
            //already voted for this term
            response->set_votegranted(false);
        }
        else{
            //TODO: Compare logs.
            DLOG(INFO) << "[vote] voting for id " << request->sender_id() << ", term " << request->term();
            response->set_votegranted(true);
            voted_for = request->sender_id();
        }
    }


}


/*
 * This method will send out requestVote RPC to all other (live) nodes.
 */
void Node::elect_for_leader() {
    DLOG(INFO) << "Electing for leader for term :"<<my_term;
    std::unique_lock<std::mutex> l(mu);

    //vote for myself.
    voted_for = my_id;
    auto call_data = std::make_shared<RequestVoteCallData>();

    call_data->node = shared_from_this();
    call_data->req.set_term(my_term);
    call_data->req.set_sender_id(my_id);


    for (const auto & stub: stubs){
        stub->RequestVote(&call_data->cntl, &call_data->req, &call_data->reply, google::protobuf::NewCallback(Node::onRequestVoteComplete, call_data));
    }

}

void Node::onRequestVoteComplete(std::shared_ptr<RequestVoteCallData> call_data) {

    if (call_data->cntl.Failed()){
        LOG(FATAL) << "Request failed, not implemented, message: " << call_data->cntl.ErrorText();
    }
    else{
        if (call_data->reply.votegranted()){
            std::lock_guard<std::mutex> l(call_data->node->mu);


            if (std::count(call_data->node->votes.begin(), call_data->node->votes.end(), call_data->reply.sender_id()) == 0){
                call_data->node->votes.push_back(call_data->reply.sender_id());
                DLOG(INFO) << "Got vote from " << call_data->reply.sender_id();
                if (call_data->node->votes.size() + 1  >= (N_nodes + 1)/2){
                    DLOG(INFO) << "Elected as leader for term " << call_data->node->my_term;
                }
            }
        }
        else{
            DLOG(WARNING) << "[election] Vote not granted, from :" << call_data->reply.sender_id();
        }
    }
}

void Node::next_term() {
    my_term++;
    DLOG(INFO) << "Moved to next term: " << my_term;
    //clear the votes for last term.
    voted_for = -1;
}
