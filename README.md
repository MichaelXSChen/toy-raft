# toy-raft
A toy implementation of the Raft consensus protocol using apache brpc. 

This implementation is only for helping to understand the protocol, not (yet) optimized for performance. 

## Usage:

    ./raftServer --ips 127.0.0.1,127.0.0.1,127.0.0.1 --ports 10000,10001,10002 --n_nodes 3 --id 0
