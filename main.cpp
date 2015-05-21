#include "scheduler.h"

#include <assert.h>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/random/discrete_distribution.hpp>
#include <boost/random/exponential_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/thread.hpp>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <memory>

// Simulated miner, assuming constant difficulty.
class Miner
{
public:
    Miner(double _hash_fraction) : hash_fraction(_hash_fraction) {
        best_chain = std::make_shared<std::vector<int>>();
    }
    
    void AddPeer(Miner* peer, int64_t latency) {
        peers.push_back(std::make_pair(peer, latency));
    }

    virtual void FindBlock(CScheduler& s, int blockNumber, double t) {
        // Extend the chain:
        auto chain_copy = std::make_shared<std::vector<int>>(best_chain->begin(), best_chain->end());
        chain_copy->push_back(blockNumber);
        best_chain = chain_copy;
        
        RelayChain(s, chain_copy, t);
    }

    virtual void ConsiderChain(CScheduler& s, std::shared_ptr<std::vector<int>> chain, double t) {
        if (chain->size() > best_chain->size()) {
            best_chain = chain;
            RelayChain(s, chain, t);
        }
    }

    virtual void RelayChain(CScheduler& s, std::shared_ptr<std::vector<int>> chain, double t) {
        for (auto i : peers) {
            auto f = boost::bind(&Miner::ConsiderChain, i.first, boost::ref(s), chain, t);
            s.schedule(f, t + i.second);
        }
    }

    virtual void ResetChain() {
        best_chain->clear();
    }

    const double GetHashFraction() const { return hash_fraction; }
    std::vector<int> GetBestChain() const { return *best_chain; }

protected:
    double hash_fraction;

    std::shared_ptr<std::vector<int>> best_chain;
    std::list<std::pair<Miner*,double>> peers;
};

static void
Connect(Miner* m1, Miner* m2, int64_t latency)
{
    m1->AddPeer(m2, latency);
    m2->AddPeer(m1, latency);
}

int
run_simulation(boost::random::mt19937& rng, int n_blocks, std::vector<Miner*>& miners, std::vector<int>& blocks_found)
{
    CScheduler simulator;

    std::vector<double> probabilities;
    for (auto miner : miners) probabilities.push_back(miner->GetHashFraction());
    boost::random::discrete_distribution<> dist(probabilities.begin(), probabilities.end());

    boost::random::variate_generator<boost::random::mt19937, boost::random::exponential_distribution<>>
        block_time_gen(rng, boost::random::exponential_distribution<>(1.0));

    std::map<int, int> block_owners; // Map block number to miner who found that block

    // This loops primes the simulation with n_blocks being found at random intervals
    // starting from t=0:
    double t = 0.0;
    for (int i = 0; i < n_blocks; i++) {
        int which_miner = dist(rng);
        block_owners.insert(std::make_pair(i, which_miner));
        auto t_delta = block_time_gen()*600.0;
        auto t_found = t + t_delta;
        auto f = boost::bind(&Miner::FindBlock, miners[which_miner], boost::ref(simulator), i, t_found);
        simulator.schedule(f, t_found);
        t = t_found;
    }

    simulator.serviceQueue();

    blocks_found.clear();
    blocks_found.insert(blocks_found.begin(), miners.size(), 0);

    // Run through miner0's idea of the best chain to count
    // how many blocks each miner found in that chain:
    std::vector<int> best_chain = miners[0]->GetBestChain();
    for (int i = 0; i < best_chain.size(); i++) {
        int b = best_chain[i];
        int m = block_owners[b];
        ++blocks_found[m];
    }
    return best_chain.size();
}

int main(int argc, char** argv)
{
    int n_blocks = 2016;
    double block_latency = 1.0;
    int n_runs = 1;
    int rng_seed = 0;

    if (argc > 1) {
        n_blocks = atoi(argv[1]);
    }
    if (argc > 2) {
        block_latency = atof(argv[2]);
    }
    if (argc > 3) {
        n_runs = atoi(argv[3]);
    }
    if (argc > 4) {
        rng_seed = atoi(argv[4]);
    }

    std::cout << "Simulating " << n_blocks << " blocks, latency " << block_latency << "secs, rng seed: " << rng_seed << "\n";

    // TODO: read miner config (hash rate, connectivity, connection latency...) from a config file...
    // Create 8 miners
    std::vector<Miner*> miners;
    miners.push_back(new Miner(0.3));
    for (int i = 1; i < 8; i++) miners.push_back(new Miner(0.1));

    // miners 1-7 connected to each other, directly
    for (int i = 1; i < 8; i++) {
        for (int j = i+1; j < 8; j++) {
            Connect(miners[i], miners[j], block_latency);
        }
    }

    // miners[0] sends to, and receives from, miner1/2/3
    miners[0]->AddPeer(miners[1], block_latency);
    miners[1]->AddPeer(miners[0], block_latency);
    miners[0]->AddPeer(miners[2], block_latency);
    miners[2]->AddPeer(miners[0], block_latency);
    miners[0]->AddPeer(miners[3], block_latency);
    miners[3]->AddPeer(miners[0], block_latency);
    // ... but miner[0] receives from all peers:
    for (int i = 4; i < 8; i++)
        miners[i]->AddPeer(miners[0], block_latency);

    boost::random::mt19937 rng;
    rng.seed(rng_seed);

    int best_chain_sum = 0;
    double fraction_orphan_sum = 0.0;
    std::vector<int> blocks_found_sum;
    blocks_found_sum.reserve(miners.size());
    for (int run = 0; run < n_runs; run++) {
        for (auto miner : miners) miner->ResetChain();

        std::vector<int> blocks_found;
        int best_chain_length = run_simulation(rng, n_blocks, miners, blocks_found);
        best_chain_sum += best_chain_length;
        fraction_orphan_sum += 1.0 - (double)best_chain_length/(double)n_blocks;;
        for (int i = 0; i < blocks_found.size(); i++) blocks_found_sum[i] += blocks_found[i];
    }

    std::cout.precision(4);
    std::cout << "Orphan rate: " << (fraction_orphan_sum*100.0)/n_runs << "%\n";
    std::cout << "Miner shares (%):";

    for (int i = 0; i < miners.size(); i++) {
        double average_blocks_found = (double)blocks_found_sum[i]/n_runs;
        double average_best_chain = (double)best_chain_sum/n_runs;
        double fraction = average_blocks_found/average_best_chain;
        std::cout << " " << fraction*100;
    }
    std::cout << "\n";

    return 0;
}
