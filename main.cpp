#include "scheduler.h"

#include <assert.h>
#include <boost/bind.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/function.hpp>
#include <boost/random/discrete_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/poisson_distribution.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/thread.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>

// Simulated miner, assuming constant difficulty.
class Miner
{
public:
    Miner() {
    }
    
    void AddPeer(Miner* peer, int64_t latency) {
        peers.push_back(std::make_pair(peer, latency));
    }

    void FindBlock(CScheduler& s, int blockNumber) {
        // Create a new copy of best chain:
        best_chain.push_back(blockNumber);
        
        RelayChain(s);
    }

    void ConsiderChain(CScheduler& s, std::vector<int> chain) {
        if (chain.size() > best_chain.size()) {
            best_chain = chain;
            RelayChain(s);
        }
    }

    void RelayChain(CScheduler& s) {
        auto now = boost::chrono::system_clock::now();
        for (auto i : peers) {
            auto f = boost::bind(&Miner::ConsiderChain, i.first, boost::ref(s), best_chain);
            s.schedule(f, now + boost::chrono::nanoseconds(i.second));
        }
    }

    int GetChainTip() const {
        if (best_chain.empty()) return -1;
        return best_chain.back();
    }

private:
    std::vector<int> best_chain;
    std::list<std::pair<Miner*,int64_t> > peers;
};

static void
Connect(Miner* m1, Miner* m2, int64_t latency)
{
    m1->AddPeer(m2, latency);
    m2->AddPeer(m1, latency);
}

int main(int argc, char** argv)
{
    int n_blocks = 2016;
    int rng_seed = 0;

    if (argc > 1) {
        n_blocks = atoi(argv[1]);
    }
    if (argc > 2) {
        rng_seed = atoi(argv[2]);
    }

    std::cout << "Simulating " << n_blocks << " blocks, rng seed: " << rng_seed << "\n";

    // Create 7 miners
    Miner miner[7];
    double probabilities[7];

    probabilities[0] = 0.3;
    // miner[0] connected to miner1/2/3
    Connect(&miner[0], &miner[1], 1);
    Connect(&miner[0], &miner[2], 1);
    Connect(&miner[0], &miner[3], 1);

    // miners 1-6 connected to each other, directly
    for (int i = 1; i < 7; i++) {
        probabilities[i] = 0.1;
        for (int j = i+1; j < 7; j++) {
            Connect(&miner[i], &miner[j], 1);
        }
    }

    boost::random::discrete_distribution<> dist(probabilities);
    boost::random::mt19937 rng;
    rng.seed(rng_seed);
    boost::random::poisson_distribution<> p_dist(600.0);

    CScheduler simulator;

    auto t = boost::chrono::system_clock::now();
    for (int i = 0; i < n_blocks; i++) {
        int which_miner = dist(rng);
        auto f = boost::bind(&Miner::FindBlock, &miner[which_miner], boost::ref(simulator), i);
        auto t_delta = p_dist(rng);
        auto t_found = t + boost::chrono::nanoseconds((int)t_delta);
        simulator.schedule(f, t_found);
        t = t_found;
    }

    simulator.serviceQueue(true);

    for (int i = 0; i < 7; i++) {
        std::cout << "Miner " << i << " tip: " << miner[i].GetChainTip() << "\n";
    }

    return 0;
}
