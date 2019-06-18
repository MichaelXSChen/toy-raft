//
// Created by xusheng on 6/18/19.
//

#include "Entry.hpp"

std::ostream & operator << (std::ostream &out, const Entry &c)
{
    out << "[ index: " << c.index;
    out << ", leader id: " << c.leader_id;
    out << ", content: " << c.data;
    out << ", status: " << c.status << " ]" ;
    return out;
}