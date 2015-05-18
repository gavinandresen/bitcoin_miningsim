// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCHEDULER_H
#define BITCOIN_SCHEDULER_H

//
// NOTE:
// boost::thread / boost::function / boost::chrono should be ported to
// std::thread / std::function / std::chrono when we support C++11.
//
#include <boost/function.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>
#include <map>

class CScheduler
{
public:
    CScheduler();
    ~CScheduler();

    typedef boost::function<void(void)> Function;

    // Call func at/after time t
    void schedule(Function f, boost::chrono::system_clock::time_point t);

    void serviceQueue();

private:
    std::multimap<boost::chrono::system_clock::time_point, Function> taskQueue;
};

#endif
