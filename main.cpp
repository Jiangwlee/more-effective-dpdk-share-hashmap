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

    //char name[] = "test";
    test<int, int>();
    return 0;
}
