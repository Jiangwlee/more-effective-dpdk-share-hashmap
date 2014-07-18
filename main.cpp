#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <iostream>

#include <sys/queue.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

#include "include/test.h"

using namespace std;

int main(int argc, char **argv) {
    int ret;

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
    	rte_panic("Cannot init EAL\n");

    char name[] = "test";
    shm_stl::hash_map<int, int> hs(name);
    hs.create_or_attach();
    hs.insert(1, 10);

    int value = 0;
    if (hs.find(1, &value))
        cout << "Key : 1 --> Value : " << value << endl;

    int new_value = 11;
    Add<int> add;
    hs.update(1, new_value, add);

    if (hs.find(1, &value))
        cout << "Key : 1 --> Value : " << value << endl;

    if (hs.erase(1, &value))
        cout << "Erase Key : 1 --> Value " << value << " from hash_map!" << endl;
    else
        cout << "Erase Key : 1 fail!" << endl;


    if (!hs.find(1, &value))
        cout << "Can't find Key 1!" << endl;
    else
        cout << "Key : 1 --> Value : " << value << endl;

    //test<int, int>();
    return 0;
}
