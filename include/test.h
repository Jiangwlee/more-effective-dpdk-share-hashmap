#ifndef __TEST_H_
#define __TEST_H_

#include <iostream>
#include <sstream>
#include <rte_eal.h>
#include "shm_hash_map.h"

#define OCCUPY_HASHMAP

using shm_stl::hash_map;
using namespace std;

template <typename _T>
struct Add {
    void operator() (_T & old_v, _T &new_v) {
        old_v += new_v;
    } 
};

static int prompt_key(void) {
    int key;

    cout << " ... Please input the key [1 ~ 65536] : ";
    cin  >> key;
    cout << " ... You just input " << key << endl;

    return key;
}

static int prompt_value(void) {
    int value;

    cout << " ... Please input the value[1 ~ 65536] : ";
    cin  >> value;
    cout << " ... You just input " << value << endl;

    return value;
}

template <typename _HashMap>
void init(_HashMap &hashmap) {
    if (rte_eal_process_type() != RTE_PROC_PRIMARY)
        return;

    for (int i = 0; i < 1000; ++i) {
        if (!hashmap.insert(i, i * i))
            cout << "Insert <" << i << ", " << i * i << "> fail!" << endl;
    }

    hashmap.print();
}

template <typename _Key, typename _Value>
//void test(const char *name) {
void test() {
    char name[] = "test";
    hash_map<_Key, _Value> hashmap(name, 8, 8);
    hashmap.create_or_attach();
    //init(hashmap);
    const rte_memzone * zone;
    volatile bool * flag = NULL;
    char zone_name[] = "share_flag";

    if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
        zone = rte_memzone_reserve(zone_name, sizeof(bool), 0, 0);
        flag = (bool*)zone->addr;
        *flag = false;
    } else {
        zone = rte_memzone_lookup(zone_name);
        flag = (bool*)zone->addr;
    }

    cout << "Please input your choice : a[dd], d[elete], f[ind], m[odify], s[how], q[uit]" << endl;

    while (1) {
        int input = getchar();
        int quit = false;
        int key = 0;
        int value = 0;
        int count = 20000;

        switch (input) {
            case 'a':
                key = prompt_key();
                value = prompt_value();
                hashmap.insert(key, value);
                break;
            case 'd':
                key = prompt_key();
                hashmap.erase(key);
                break;
            case 'f':
                key = prompt_key();
                if (hashmap.find(key, &value)) {
                    cout << "Key : " << key << " Value : " << value << endl;
                }
                break;
            case 'm':
                key = prompt_key();
                value = prompt_value();
                shm_stl::Assignment<_Value> assign;
                hashmap.update(key, value, assign);
                break;
            case 's':
                hashmap.print();
                break;
            case 'q':
                quit = true;
                break;

            case 't':
                // test lock on multi processes
                key = prompt_key();
                value = prompt_value();
                Add<_Value> add;
                if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
                    cout << "primary process! " << endl;
                    if (*flag)
                        cout << "flag is true! " << endl;
                    else
                        cout << "flag is false! " << endl;
                    *flag = true;
                    while (count > 0) {
                        hashmap.update(key, value, add);
                        count--;
                    }
                } else {
                    cout << "primary process! " << endl;
                    if (*flag)
                        cout << "flag is true! " << endl;
                    else
                        cout << "flag is false! " << endl;
                    while (*flag == false) {}
                    while (count > 0) {
                        hashmap.update(key, value, add);
                        count--;
                    }
                }
                break;

            default:
                continue;
        }

        cout << "Please input your choice : a[dd], d[elete], f[ind], m[odify], s[how], q[uit]" << endl;

        if (quit)
            break;
    }

    return;
}

#endif
