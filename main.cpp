#include "scheduler.h"

#include <assert.h>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/random/discrete_distribution.hpp>
#include <boost/random/exponential_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/random_number_generator.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "standard_miner.hpp"


static void
Connect(Miner* m1, Miner* m2, double latency)
{
    m1->AddPeer(m2, latency);
    m2->AddPeer(m1, latency);
}

double
random_real(boost::random::mt19937& rng, double min, double max)
{
    boost::random::uniform_real_distribution<> d(min,max);
    return d(rng);
}

int
run_simulation(boost::random::mt19937& rng, int n_blocks, double block_latency, 
               std::vector<Miner*>& miners, std::vector<int>& blocks_found)
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
        auto f = boost::bind(&Miner::FindBlock, miners[which_miner], boost::ref(simulator), i, t_found, block_latency);
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
    namespace po = boost::program_options;

    int n_blocks = 2016;
    double block_latency = 1.0;
    int n_runs = 1;
    int rng_seed = 0;
    std::string config_file;

    po::options_description desc("Command-line options");
    desc.add_options()
        ("help", "show options")
        ("blocks", po::value<int>(&n_blocks)->default_value(2016), "number of blocks to simulate")
        ("latency", po::value<double>(&block_latency)->default_value(1.0), "block relay/validate latency (in seconds) to simulate")
        ("runs", po::value<int>(&n_runs)->default_value(1), "number of times to run simulation")
        ("rng_seed", po::value<int>(&rng_seed)->default_value(0), "random number generator seed")
        ("config", po::value<std::string>(&config_file)->default_value("mining.cfg"), "Mining config filename")
        ;
    po::variables_map vm;

    po::options_description config("Mining config file options");
    config.add_options()
        ("miner", po::value<std::vector<std::string>>()->composing(), "hashrate type")
        ("biconnect", po::value<std::vector<std::string>>()->composing(), "m n connection_latency")
        ("description", po::value<std::string>(), "Configuration description")
        ;

    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    std::ifstream f(config_file.c_str());
    po::store(po::parse_config_file<char>(f, config), vm);
    f.close();
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        std::cout << config << "\n";
        return 1;
    }

    boost::random::mt19937 rng;
    rng.seed(rng_seed);

    std::vector<Miner*> miners;
    if (!vm.count("miner")) {
        std::cout << "You must configure one or more miner in " << config_file << "\n";
        return 1;
    }
    for (auto m : vm["miner"].as<std::vector<std::string>>()) {
        std::vector<std::string> v;
        boost::split(v, m, boost::is_any_of(" \t,"));
        if (v.size() < 2) {
            std::cout << "Couldn't parse miner description: " << m << "\n";
            continue;
        }
        double hashpower = atof(v[0].c_str());
        if (v[1] == "standard") {
            miners.push_back(new Miner(hashpower, boost::bind(random_real, boost::ref(rng), _1, _2)));
        }
        else {
            std::cout << "Couldn't parse miner description: " << m << "\n";
            continue;
        }
    }
    std::vector<std::string> c = vm["biconnect"].as<std::vector<std::string>>();
    for (auto m : c) {
        std::vector<std::string> v;
        boost::split(v, m, boost::is_any_of(" \t,"));
        if (v.size() < 3) {
            std::cout << "Couldn't parse biconnect description: " << m << "\n";
            continue;
        }
        int m1 = atoi(v[0].c_str());
        int m2 = atoi(v[1].c_str());
        double latency = atof(v[2].c_str());
        if (m1 >= miners.size() || m2 >= miners.size()) {
            std::cout << "Couldn't parse biconnect description: " << m << "\n";
            continue;
        }
        Connect(miners[m1], miners[m2], latency);
    }

    std::cout << "Simulating " << n_blocks << " blocks, latency " << block_latency << "secs\n";
    std::cout << "  with " << miners.size() << " miners over " << n_runs << " runs\n";
    if (vm.count("description")) {
        std::cout << "Configuration: " << vm["description"].as<std::string>() << "\n";
    }

    int best_chain_sum = 0;
    double fraction_orphan_sum = 0.0;
    std::vector<int> blocks_found_sum;
    blocks_found_sum.assign(miners.size(), 0);
    for (int run = 0; run < n_runs; run++) {
        for (auto miner : miners) miner->ResetChain();

        std::vector<int> blocks_found;
        int best_chain_length = run_simulation(rng, n_blocks, block_latency, miners, blocks_found);
        best_chain_sum += best_chain_length;
        fraction_orphan_sum += 1.0 - (double)best_chain_length/(double)n_blocks;;
        for (int i = 0; i < blocks_found.size(); i++) blocks_found_sum[i] += blocks_found[i];
    }

    std::cout.precision(4);
    std::cout << "Orphan rate: " << (fraction_orphan_sum*100.0)/n_runs << "%\n";
    std::cout << "Miner hashrate shares (%):";
    for (int i = 0; i < miners.size(); i++) {
        std::cout << " " << miners[i]->GetHashFraction()*100;
    }
    std::cout << "\n";
    std::cout << "Miner block shares (%):";

    for (int i = 0; i < miners.size(); i++) {
        double average_blocks_found = (double)blocks_found_sum[i]/n_runs;
        double average_best_chain = (double)best_chain_sum/n_runs;
        double fraction = average_blocks_found/average_best_chain;
        std::cout << " " << fraction*100;
    }
    std::cout << "\n";

    return 0;
}
