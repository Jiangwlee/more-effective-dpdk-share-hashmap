more-effective-dpdk-share-hashmap
=================================

An effective share hashmap based on dpdk
---
This is an implementation of share hash_map based on dpdk. Dpdk lib implements a hash table named rte_hash itself.
But it is difficult to use and lack of extensibility. So I write this hash_map. It is as convenience as std::hash_map
and could be shared by multi process.

Major Features
---
1. Based on DPDK library
2. Could be shared by multi process
3. It is an template container, could hold arbitrary value type
4. It looks like std::hash_map, you can define your own hasher and key_equal functor
5. It can expand its size automatically

Build
---
1. Download the source codes of dpdk from dpdk.org
2. Read the intel-dpdk-getting-start-guide.pdf to learn how to set up a dpdk develop environment
3. Build and run the helloworld example in dpdk-1.6.0r2/examples/
4. Copy the files under hashmap/mk/ to dpdk-1.6.0r2/mk/ to enable g++ for dpdk
5. Build this program by following command:
    $ make CC=g++

Run
---
1. Make sure you have run dpdk-1.6.0r2/tools/setup.sh to set up your dpdk running environment
   >< You would know what I mean if you have run the helloworld program >

2. Start the primary process
   >$ sudo ./build/hashmap -c 1 -n 4 --proc-type=primary

3. Start the secondary process
   >$ sudo ./build/hashmap -c c -n 4 --proc-type=secondary

Any issue, you can contact with me by email <jiangwlee@163.com>
