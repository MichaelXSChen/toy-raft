//
// Created by xusheng on 6/18/19.
//




#include "Node.hpp"
#include "flags.hpp"



int main(int argc, char* argv[]){


    gflags::ParseCommandLineFlags(&argc, &argv, true);



    LOG(INFO) << "Flags node id: " << FLAGS_id;
    auto n = std::make_shared<Node>(FLAGS_id);

    n->start();


    n->wait();




    return 0;
}