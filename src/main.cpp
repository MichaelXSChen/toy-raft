//
// Created by xusheng on 6/18/19.
//


#define STRIP_FLAG_HELP 1

#include "Node.hpp"
#include "flags.hpp"



int main(int argc, char* argv[]){

    gflags::SetUsageMessage("./raftServer --ips 127.0.0.1,127.0.0.1,127.0.0.1 --ports 10000,10001,10002 --n_nodes 3 --id 0");

    gflags::ParseCommandLineFlags(&argc, &argv, true);

    LOG(INFO) << "Flags node id: " << FLAGS_id;
    auto n = std::make_shared<Node>(FLAGS_id);

    n->start();


    n->wait();




    return 0;
}