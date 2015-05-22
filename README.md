# Mining / block propagation simulator

Simulates new block announcements propagating across the Bitcoin
network.

Use these command-line options to control the simulation parameters:

```
  --help                     show options
  --blocks arg (=2016)       number of blocks to simulate
  --latency arg (=1)         block relay/validate latency (in seconds) to
                             simulate
  --runs arg (=1)            number of times to run simulation
  --rng_seed arg (=0)        random number generator seed
  --config arg (=mining.cfg) Mining config filename
```

The network topology and miner configuration is specified in
a config file; see the .cfg files here for examples.

Config file options:
```
miner=hashrate type
biconnect=m n connection_latency
```

I don't do autotools, so you'll have to edit the Makefile
unless you're compiling on OSX. The only dependency is
boost (but you do need a c++11-capable C++ compiler).


Example runs:

```
$ mining_simulator
Simulating 2016 blocks, latency 1secs
  with 10 miners over 1 runs
  Configuration: Ten equal miners, connected in a ring
  Orphan rate: 0.1984%
  Miner hashrate shares (%): 10 10 10 10 10 10 10 10 10 10
  Miner block shares (%): 9.692 11.08 9.791 10.04 9.543 9.195 9.543 9.841 9.94 11.33

$ mining_simulator --latency 20
Simulating 2016 blocks, latency 20secs
  with 10 miners over 1 runs
  Configuration: Ten equal miners, connected in a ring
  Orphan rate: 3.224%
  Miner hashrate shares (%): 10 10 10 10 10 10 10 10 10 10
  Miner block shares (%): 9.585 11.28 9.79 9.995 9.534 8.97 9.687 9.841 9.995 11.33

$ mining_simulator --latency 20 --config mining_30.cfg --runs 100
Simulating 2016 blocks, latency 20secs
  with 8 miners over 100 runs
  Configuration: 30% miner, with 7 10% miners, selected connectivity
  Orphan rate: 2.694%
  Miner hashrate shares (%): 30 10 10 10 10 10 10 10
  Miner block shares (%): 30.32 9.859 10.04 10.03 9.977 9.942 9.883 9.947
```

