//
// Created by xusheng on 6/18/19.
//

#include "Node.hpp"
#include <butil/logging.h>
#include <brpc/server.h>
#include <ctime>
#include <cstdlib>
#include <chrono>

#define N_nodes 3

void Node::AppendEntries(google::protobuf::RpcController* cntl_base,
                   const raft::AppendEntriesReq* request,
                   raft::AppendEntriesReply* response,
                   google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);


    //Seems not needed to touch the config for now.
    //brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    DLOG(INFO) << "[Append Follower] Append entries called, leader_commit = " << request->leadercommit();

    std::lock_guard<std::mutex> l(mu);

    if (my_role == RAFT_CANDIDATE){
        if (request->term() >= my_term){
            my_term = request->term();
            my_role = RAFT_FOLLOWER;
            DLOG(INFO) << "[Election] received leader message from a higher term, changed to be follower, term " << my_term;
        }
    }


    if (my_role == RAFT_FOLLOWER){
        if (request->term() < my_term) {
            response->set_success(false);
            response->set_term(my_term);
            DLOG(WARNING) << "[Append Follower] received leader message from lower term, reject proposal" << my_term;
            return;
        }



        if (request->prevlogindex() >= 1 &&
                entries[request->prevlogindex()-1]->term() != request->prevlogterm()){
            response->set_success(false);
            response->set_term(my_term);
            DLOG(WARNING) << "[Append Follower] received leader with unmatched prevLogTerm, reject proposal" << my_term;
            return;
        }


        if (request->entries().size() != 0){
            while (request->entries(0).index() < max_received_index + 1){
                DLOG(INFO) << "Going to pop back, " << request->entries(0).index() << "is smaller than "  << max_received_index + 1;
                entries.pop_back();
                max_received_index--;
                DLOG(WARNING) << "[Append Follower] Entry conflict, Replacing local entry with remote one, new max_received_index = "
                    << max_received_index << ", proposed entry start from " << request->entries(0).index();
            }
        }


        for (const auto &e : request->entries()){
            DLOG(INFO) << "Received request, index " << e.index();


            if (e.index() == max_received_index + 1){
                auto entry = std::make_unique<Entry> (e.data(), e.index(), 0, RECEIVED, request->term());



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


        std::unique_lock<std::mutex> tl(timer_mu);
        received_msg = true;
        timer_cond.notify_all();
        tl.unlock();
    }


}

void Node::start() {
    t = std::thread(&Node::serveRPCs, this);
    srand(time(0));
    sleep(3); //for normal workload.


    while(true) {
        if (brpc::IsAskedToQuit()){
            return;
        }

        int sleep_ms = rand() % 1500 + 1500;
        std::unique_lock<std::mutex> l(timer_mu);

        timer_cond.wait_for(l, std::chrono_literals::operator ""ms(sleep_ms));

        if (received_msg){
            received_msg = false;
            timer_mu.unlock();
            continue;
        }
        else{
            timer_mu.unlock();
            std::unique_lock<std::mutex> global_lk(mu);
            my_role = RAFT_CANDIDATE;
            global_lk.unlock();

            LOG(WARNING) << "[TIMEOUT] going to elect for leader";
            while(true){
                std::unique_lock<std::mutex> global_lk_loop(mu);

                if (my_role != RAFT_CANDIDATE){
                    break;
                }

                global_lk_loop.unlock();

                next_term();

                elect_for_leader();

                if (brpc::IsAskedToQuit()){
                    return;
                }
            }


            if (my_role == RAFT_LEADER){

                int count = 0;
                while(!brpc::IsAskedToQuit()){
                    append(std::to_string(count));
                    sleep(1);
                }
                if (brpc::IsAskedToQuit()){
                    return;
                }
            }
        }

    }








}

void Node::wait(){
    t.join();
}

void Node::serveRPCs() {
    if (server.AddService(this, brpc::SERVER_DOESNT_OWN_SERVICE) != 0){
        LOG(FATAL) << "Fail to add service";
        exit(-1);
    }
    if (server.Start(my_id + 10000, &options) != 0) {
        LOG(FATAL) << "Fail to start EchoServer";
        exit(-1);
    }

    DLOG(INFO) << "RPC service started";

    server.RunUntilAskedToQuit();
}

void Node::append(const std::string& data) {
    std::lock_guard<std::mutex> l(mu);

    if (my_role != RAFT_LEADER){
        LOG(WARNING) << " I am not leader, giving up on proposing";
        return;
    }
    max_received_index++;
    auto e = std::make_unique<Entry>(data, max_received_index, my_id, PROPOSED, my_term);
    DLOG(INFO) << "Proposing entry" << *e;
    entries.push_back(std::move(e));


    if (max_committed_index >= 1){
        DLOG(INFO) << "[Append] ask for consensus"
                   << ", max committed index: " << max_committed_index
                   << ", proposed entries count: " << entries.size() - max_committed_index
                   << ", prevLogTerm " << entries[max_committed_index-1]->term();
    }else{
        DLOG(INFO) << "[Append] ask for consensus"
                   << ", max committed index: " << max_committed_index
                   << ", proposed entries count: " << entries.size() - max_committed_index
                   << ", prevLogTerm " << 0;
    }


    for (const auto & s: stubs){

        auto call_data = std::make_shared<AppendEntriesCallData>();

        call_data->node = shared_from_this();

        call_data->remote_addr = s.first;

        call_data->req.set_term(my_term);
        call_data->req.set_sender_id(my_id);
        call_data->req.set_leadercommit(max_committed_index);

        call_data->req.set_prevlogindex(max_committed_index);

        if (max_committed_index >= 1){
            call_data->req.set_prevlogterm(entries[max_committed_index-1]->term());
        }else{
            call_data->req.set_prevlogterm(0);
        }

        for (uint i = max_committed_index; i < entries.size(); i++){
            auto entry = call_data->req.add_entries();
            entry->set_data(entries[i]->data());
            entry->set_index(i+1);
        }





        s.second->AppendEntries(&call_data->cntl, &call_data->req, &call_data->reply, google::protobuf::NewCallback(Node::onAppendEntriesComplete, call_data));
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
    brpc::ClosureGuard done_guard(done);
    DLOG(INFO) << "[election] Received RequestVote request from " << request->sender_id() << " for term " << request->term();

    std::lock_guard<std::mutex> l(mu);
    response->set_sender_id(my_id);
    response->set_term(my_term);
    if (request->term() < my_term){
        response->set_votegranted(false);
        DLOG(INFO) << "[election] request from " << request->sender_id() << ": smaller than my term, not voting for it";
        return;
    }

    if (request->term() > my_term) {
        //Revert to follower state.
        my_role = RAFT_FOLLOWER;
        my_term = request->term();
        voted_for = -1;
        DLOG(INFO) << "Found a higher term, move to that term and revert as follower, term = " << my_term;
    }

    //Already on the same term if executed to this line
    if (my_role == RAFT_LEADER){
        DLOG(INFO) << "[election] request from " << request->sender_id() << ": I am a leader for the term " << request->term();
        response->set_votegranted(false);
    }
    else{
        if (voted_for != -1){
            //already voted for this term
            DLOG(INFO) << "[election] request from " << request->sender_id() << ": already voted for " << voted_for <<", not voting for it";

            response->set_votegranted(false);
        }
        else{
            if (request->lastlogterm() > local_last_log_term()){
                LOG(INFO) << "[Vote] voting for id " << request->sender_id()
                    << ", because it has a more upto date log, election term =  " << request->term()
                    << ", remote log last log term = " << request->lastlogterm()
                    << ", local log last log term = " << local_last_log_term();
                response->set_votegranted(true);
                voted_for = request->sender_id();
            }
            /*
             * Can be combined together with previous branch, but separated for better log (and understandability).
             */
            else if (request->lastlogterm() == local_last_log_term() && request->lastlogindex() >= local_last_log_index()){
                LOG(INFO) << "[Vote] voting for id " << request->sender_id()
                          << ", because it has a more upto date log, election term =  " << request->term()
                          << ", remote log last log term == local log last log term =  " << request->lastlogterm()
                          << ", remote log last log index = " << request->lastlogindex()
                          << ", local log last log index = " << local_last_log_index();
                response->set_votegranted(true);
                voted_for = request->sender_id();
            }
        }
    }
}


/*
 * This method will send out requestVote RPC to all other (live) nodes.
 */
void Node::elect_for_leader() {


    std::unique_lock<std::mutex> l(mu);

    //vote for myself.
    voted_for = my_id;


    for (const auto & s: stubs){
        auto call_data = std::make_shared<RequestVoteCallData>();

        call_data->node = shared_from_this();
        call_data->req.set_term(my_term);
        call_data->req.set_sender_id(my_id);
        call_data->req.set_lastlogindex(local_last_log_index());
        call_data->req.set_lastlogterm(local_last_log_term());

        call_data->remote_addr = s.first;



        s.second->RequestVote(&call_data->cntl, &call_data->req, &call_data->reply, google::protobuf::NewCallback(Node::onRequestVoteComplete, call_data));
    }

    int rand_sleep = rand() % 300 + 150;
    DLOG(INFO) << "Electing for leader for term :"<<my_term << "the time out time is " << rand_sleep << "ms";


    role_cond.wait_for( l, std::chrono_literals::operator""ms(rand_sleep));


}

void Node::onRequestVoteComplete(std::shared_ptr<RequestVoteCallData> call_data) {

    if (call_data->cntl.Failed()){
        LOG(WARNING) << "Vote request failed, message: " << call_data->cntl.ErrorText() <<", remote addr" << call_data->remote_addr;
        // exit(-1);
        std::lock_guard<std::mutex> l(call_data->node->mu);
        call_data->node->stubs.erase(call_data->remote_addr);
        DLOG(INFO) << "Removed stubs to failed node: " << call_data->remote_addr;



    }
    else{
        if (call_data->reply.votegranted()){
            DLOG(INFO) << "[Election] got vote from " << call_data->reply.sender_id() << "trying to get the lock";

            std::unique_lock<std::mutex> l(call_data->node->mu);


            if (std::count(call_data->node->votes.begin(), call_data->node->votes.end(), call_data->reply.sender_id()) == 0){
                call_data->node->votes.push_back(call_data->reply.sender_id());
                DLOG(INFO) << "Got vote from " << call_data->reply.sender_id();
                if (call_data->node->votes.size() + 1  >= (N_nodes + 1)/2){
                    DLOG(INFO) << "Elected as leader for term " << call_data->node->my_term;\
                    call_data->node->my_role = RAFT_LEADER;
                    call_data->node->role_cond.notify_all();

                }
            }
        }
        else{
            DLOG(WARNING) << "[election] Vote not granted, from :" << call_data->reply.sender_id();
        }
    }
}


void Node::onAppendEntriesComplete(std::shared_ptr<AppendEntriesCallData> call_data) {

    if (call_data->cntl.Failed()){
        //This line is triggered because I am (or, was) a leader. Remove the failed stub.

        LOG(WARNING) << "Append entries failed, message: " << call_data->cntl.ErrorText() << ", remote" << call_data->cntl.remote_side();
        std::lock_guard<std::mutex> l(call_data->node->mu);
        call_data->node->stubs.erase(call_data->remote_addr);
        DLOG(INFO) << "Removed stubs to failed node: " << call_data->remote_addr;
    }
    else{
        std::lock_guard<std::mutex> l(call_data->node->mu);
        if (call_data->node->entries.size() < call_data->reply.max_index()){
            LOG(FATAL) << "error handling ack, size: " << call_data->node->entries.size() << ", index = " << call_data->reply.max_index();
            exit(-1);
        }
        bool committed = call_data->node->entries[call_data->reply.max_index()-1]->receive_ack(call_data->reply.sender_id());
        if (committed){
            call_data->node->commit(call_data->reply.max_index());
        }
    }
}

void Node::next_term() {
    my_term++;
    DLOG(INFO) << "Moved to next term: " << my_term;
    //clear the votes for last term.
    voted_for = -1;
}

uint32_t Node::local_last_log_term(){
    if (entries.size() == 0){
        return 0;
    }else {
        return entries.back()->term();
    }

};

