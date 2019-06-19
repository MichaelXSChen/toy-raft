//
// Created by xusheng on 6/18/19.
//



#include <gflags/gflags.h>
#include <brpc/server.h>
#include "Node.hpp"

DEFINE_uint64(id, 0, "the id of the node, 0 as the leader");

int main(int argc, char* argv[]){


    gflags::ParseCommandLineFlags(&argc, &argv, true);

    LOG(INFO) << "Flags node id: " << FLAGS_id;
    auto n = std::make_shared<Node>(FLAGS_id);

    n->start();


    n->wait();




    return 0;
}