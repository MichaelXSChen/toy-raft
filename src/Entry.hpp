//
// Created by xusheng on 6/18/19.
//

#ifndef TRYRAFT_ENTRY_HPP
#define TRYRAFT_ENTRY_HPP

#include <string>
#include <iostream>

enum entry_status_t{
    EMPTY = 0,
    PROPOSED = 1,
    RECEIVED = 2,
    COMMITTED = 3,
};


class Entry {
public:
    Entry(uint32_t _index, uint32_t _leader_id, entry_status_t _status):
        index(_index), leader_id(_leader_id), status(_status)
        {}
    friend std::ostream &operator << (std::ostream &out, const Entry& e);

private:


    std::string data;
    uint32_t index;
    uint32_t leader_id;
    entry_status_t  status;
};





#endif //TRYRAFT_ENTRY_HPP
