//
// Standard miner. Relays and mines on longest chain it has seen.
//
#ifndef STANDARD_MINER_H
#define STANDARD_MINER_H

#include <boost/function.hpp>
#include <list>
#include <vector>

class Miner;
class PeerInfo {
public:
    PeerInfo(Miner* _peer, int _chain_tip, double _latency) :
        peer(_peer), chain_tip(_chain_tip), latency(_latency) { }

    Miner* peer;
    int chain_tip;
    double latency;
};

class Miner
{
public:
    // Return a random double in the range passed.
    typedef boost::function<double(double, double)> JitterFunction;

    Miner(double _hash_fraction, double _block_latency, JitterFunction _func) :
        hash_fraction(_hash_fraction), block_latency(_block_latency), jitter_func(_func) {
        best_chain = std::make_shared<std::vector<int>>();
    }
    
    void AddPeer(Miner* peer, double latency) {
        peers.push_back(PeerInfo(peer, -1, latency));
    }

    virtual void FindBlock(CScheduler& s, int blockNumber, double t) {
        // Extend the chain:
        auto chain_copy = std::make_shared<std::vector<int>>(best_chain->begin(), best_chain->end());
        chain_copy->push_back(blockNumber);
        best_chain = chain_copy;
        
        RelayChain(this, s, chain_copy, t+block_latency);
    }

    virtual void ConsiderChain(Miner* from, CScheduler& s, std::shared_ptr<std::vector<int>> chain, double t) {
        if (chain->size() > best_chain->size()) {
            best_chain = chain;
            RelayChain(from, s, chain, t);
        }
    }

    virtual void RelayChain(Miner* from, CScheduler& s, std::shared_ptr<std::vector<int>> chain, double t) {
        for (auto&& peer : peers) {
            if (peer.chain_tip == chain->back()) continue; // Already relayed to this peer
            peer.chain_tip = chain->back();
            if (peer.peer == from) continue; // don't relay to peer that just sent it!

            auto f = boost::bind(&Miner::ConsiderChain, peer.peer, from, boost::ref(s), chain, t);
            double jitter = 0;
            if (peer.latency > 0) jitter = jitter_func(-peer.latency/1000., peer.latency/1000.);
            s.schedule(f, t + peer.latency + jitter);
        }
    }

    virtual void ResetChain() {
        best_chain->clear();
    }

    const double GetHashFraction() const { return hash_fraction; }
    std::vector<int> GetBestChain() const { return *best_chain; }

protected:
    double hash_fraction; // This miner has hash_fraction of hash rate
    double block_latency; // This miner produces blocks that take block_latency seconds to relay/validate
    JitterFunction jitter_func;

    std::shared_ptr<std::vector<int>> best_chain;
    std::list<PeerInfo> peers;
};

#endif /* STANDARD_MINER_H */
