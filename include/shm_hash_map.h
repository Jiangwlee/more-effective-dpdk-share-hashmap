/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) Bruce.Li <jiangwlee@163.com>, 2014
 */


#ifndef __SHM_HASH_MAP_H_
#define __SHM_HASH_MAP_H_

#include <iostream>
#include <sstream>
#include <rte_memzone.h>
#include <rte_string_fns.h>
#include "shm_hash_table.h"

#define RETURN_FALSE_IF_NULL(ptr) do {\
    if (ptr == NULL) return false;\
} while (0)

#define SHM_NAME_SIZE 32

__SHM_STL_BEGIN

template <typename _Key, typename _Value, typename _HashFunc = hash<_Key>, typename _EqualKey = std::equal_to<_Key> >
class hash_map {
    public:
        typedef _Key key_type;
        typedef _Value value_type;
        typedef _HashFunc hasher;
        typedef _EqualKey key_equal;
        typedef hash_table<key_type, value_type, hasher, key_equal> _Ht;

    public:
        hash_map(const char * name, uint32 entries = DEFAULT_ENTRIES, uint32 buckets = DEFAULT_BUCKET_NUM):
                 m_entries(entries), m_buckets(buckets) {
                     rte_snprintf(m_name, sizeof(m_name), "HT_%s", name);
                 }

        ~hash_map() {
            // call the destructor to release memory on primary process
            if (rte_eal_process_type() == RTE_PROC_PRIMARY)
                m_ht->~_Ht();

            m_ht = NULL;
        }

        bool create_or_attach(void) {
            uint32 shm_size = sizeof(_Ht);
            const rte_proc_type_t proc_type = rte_eal_process_type();

            if (proc_type == RTE_PROC_PRIMARY) {
                const struct rte_memzone * zone = rte_memzone_reserve(&m_name[0], shm_size, 0, 
                                                                      RTE_MEMZONE_SIZE_HINT_ONLY);
                // replacement new, call the constructor of hash table
                m_ht = ::new (zone->addr) _Ht(m_entries, m_buckets);
            } else if (proc_type == RTE_PROC_SECONDARY) {
                const struct rte_memzone * zone = rte_memzone_lookup(&m_name[0]);
                m_ht = static_cast<_Ht*>(zone->addr);
            } else {
                m_ht = NULL;
            }

            if (m_ht) {
                return true;
            } else {
                return false;
            }
        }

        bool find(const key_type &key, value_type * ret = NULL) {
            RETURN_FALSE_IF_NULL(m_ht);
            return m_ht->find(key, ret); 
        }

        bool insert(const key_type &key, const value_type &value) {
            RETURN_FALSE_IF_NULL(m_ht);
            return m_ht->insert(key, value);
        }

        bool erase(const key_type &key, value_type * ret = NULL) {
            RETURN_FALSE_IF_NULL(m_ht);
            return m_ht->erase(key, ret);
        }

        template <typename _Tmpvalue, typename _Modifier>
        bool update(const key_type &key, _Tmpvalue tmp_value, _Modifier &update) {
            RETURN_FALSE_IF_NULL(m_ht);
            return m_ht->update(key, tmp_value, update);
        }

        void clear(void) {
            if (m_ht) m_ht->clear();
        }

        void print(void) {
            std::ostringstream os;
            if (m_ht) {
                m_ht->str(os);
            } else {
                os << "Hash table is not created!" << std::endl;
            }

            std::cout << os.str() << std::endl;
        }

    private:
        uint32 m_entries;
        uint32 m_buckets;
        char   m_name[SHM_NAME_SIZE];
        _Ht *  m_ht;
};

#undef SHM_NAME_SIZE

__SHM_STL_END

#endif
