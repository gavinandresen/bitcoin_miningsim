// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "scheduler.h"

#include <assert.h>
#include <boost/bind.hpp>
#include <utility>

CScheduler::CScheduler()
{
}

CScheduler::~CScheduler()
{
}


void CScheduler::serviceQueue()
{
    while (!taskQueue.empty()) {
        Function f = taskQueue.begin()->second;
        taskQueue.erase(taskQueue.begin());
        f();
    }
}

void CScheduler::schedule(CScheduler::Function f, boost::chrono::system_clock::time_point t)
{
    taskQueue.insert(std::make_pair(t, f));
}
