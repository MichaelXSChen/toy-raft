//
// Created by xusheng on 6/18/19.
//

#ifndef TRYRAFT_ENTRY_HPP
#define TRYRAFT_ENTRY_HPP

#include <string>
#include <iostream>
#include <vector>
#include <mutex>

enum entry_status_t{
    EMPTY = 0,
    PROPOSED = 1,
    RECEIVED = 2,
    COMMITTED = 3,
};


class Entry {
public:
    Entry(const std::string & _data, uint32_t _index, uint32_t _leader_id, entry_status_t _status, uint32_t _term):
        data_(_data), index(_index), leader_id(_leader_id), status(_status), term_(_term), ack_count(0)
    {}
    friend std::ostream &operator << (std::ostream &out, const Entry& e);

    //Return true if committed,
    //Return false if not getting the threshold
    bool receive_ack(uint32_t id);

    void commit();

    uint32_t  term(){
        return term_;
    }

    std::string data(){
        return data_;
    }
private:



    std::mutex mu;
    std::string data_;
    uint32_t index;
    uint32_t leader_id;
    entry_status_t  status;

    uint32_t term_;

    uint32_t ack_count;


    std::vector<uint32_t> acked_nodes;



};





#endif //TRYRAFT_ENTRY_HPP
