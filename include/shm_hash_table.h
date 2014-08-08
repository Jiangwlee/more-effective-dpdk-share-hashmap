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


#ifndef __SHM_HASH_TABLE_H_
#define __SHM_HASH_TABLE_H_

#include <sys/types.h>
#include <memory.h>
#include <iostream>
#include <sstream>
#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_rwlock.h>
#include "shm_hash_fun.h"
#include "shm_common.h"
#include "shm_bucket.h"

using std::ostream;
    
__SHM_STL_BEGIN

template <typename _Value>
struct Assignment {
    void operator() (volatile _Value &old_value, const _Value &new_value) {
        old_value = new_value;
    }
};

template <typename _Key, typename _Value, typename _HashFunc = hash<_Key>, typename _EqualKey = std::equal_to<_Key> >
class hash_table {
    public:
        typedef Node<_Key, _Value> node_type;
        typedef _Key key_type;
        typedef _Value value_type;
        typedef _HashFunc hasher;
        typedef _EqualKey key_equal;
        typedef NodePool<node_type> node_pool_t;
        typedef Bucket<node_type, key_type, value_type, key_equal>  bucket_type;

    public:
        hash_table(uint32 buckets = DEFAULT_BUCKET_NUM)
            : m_mask(0), m_bucket_num(buckets), m_bucket_array(NULL) {
                initialize();
            }

        ~hash_table(void) {finalize();}

        bool insert(const key_type & key, const value_type & value) {
            sig_t sig = m_hash_func(key);
            bucket_type * bucket = get_bucket_by_sig(sig);

            // Put node to bucket
            return bucket->put(sig, key, value);
        }

        /*
         * @brief
         *  This method takes two parameters :
         *  key is an input parameter for hash table lookup
         *  ret is an output parameter to take the value if the key is in the hash table
         * */
        bool find(const key_type & key, value_type * ret = NULL) const {
            // Get bucket
            sig_t sig = m_hash_func(key);
            bucket_type * bucket = get_bucket_by_sig(sig);

            // Search in this bucket
            return bucket->lookup(sig, key, ret); 
        }

        bool erase(const key_type &key, value_type * ret = NULL) {
            // Get bucket
            sig_t sig = m_hash_func(key);
            bucket_type * bucket = get_bucket_by_sig(sig);

            return bucket->remove(sig, key, ret);
        }

        // Update the value
        template <typename _Params, typename _Modifier>
        bool update(const key_type & key, _Params & params, _Modifier &action) {
            sig_t sig = m_hash_func(key);
            bucket_type * bucket = get_bucket_by_sig(sig);

            return bucket->update(sig, key, params, action);
        }

        // Clear this hash table
        void clear(void) {
            if (m_bucket_array == NULL)
                return;

            for (uint32 i = 0; i < m_bucket_num; ++i) {
                m_bucket_array[i].clear();
            }
        }

        uint32 capacity(void) const {
            if (m_bucket_array == NULL)
                return 0;

            uint32 capacity = 0;
            for (uint32 i = 0; i < m_bucket_num; ++i) {
                capacity += m_bucket_array[i].capacity();
            }

            return capacity;
        }

        uint32 free_entries(void) const {
            if (m_bucket_array == NULL)
                return 0;

            uint32 free_entries = 0;
            for (uint32 i = 0; i < m_bucket_num; ++i) {
                free_entries += m_bucket_array[i].free_entries();
            }

            return free_entries;
        }

        uint32 used_entries(void) const {
            if (m_bucket_array == NULL)
                return 0;

            uint32 count = 0;
            for (uint32 i = 0; i < m_bucket_num; ++i) {
                count += m_bucket_array[i].size();
            }

            return count;
        }

        void str(ostream & os) const {
            os << "\nHash Table Information : " << std::endl;
            os << "** Total Entries : " << capacity() << std::endl;
            os << "** Free  Entries : " << free_entries() << std::endl;
            os << "** Used  Entries : " << used_entries() << std::endl;
        }

    private:
        bool initialize(void) {
            // Adjust bucket number if necessary
            if (!is_power_of_2(m_bucket_num))
                m_bucket_num = convert_to_power_of_2(m_bucket_num);

            m_mask = m_bucket_num - 1;

            // Allocate memory for bucket 
            char name[] = "bucket_array";
            uint32 bucket_array_size_in_bytes = m_bucket_num * sizeof(bucket_type);
            m_bucket_array = static_cast<bucket_type*>(rte_zmalloc(name, bucket_array_size_in_bytes, 0));
            if (m_bucket_array == NULL) {
                return false;
            } else {
                // Initialize Buckets
                for (uint32 i = 0; i < m_bucket_num; ++i)
                    ::new (&m_bucket_array[i]) bucket_type;
                return true;
            }
        }

        void finalize(void) {
            if (m_bucket_array) {
                for (uint32 i = 0; i < m_bucket_num; ++i) {
                    bucket_type * bucket = &(m_bucket_array[i]);
                    // call the destructor of this bucket
                    if (bucket) {
                        bucket->~bucket_type();
                    }
                }
                rte_free(m_bucket_array);
                m_bucket_array = NULL;
            }
        }

        bucket_type * get_bucket_by_index(uint32 index) const {
            if (m_bucket_array == NULL)
                return NULL;

            if (index < m_bucket_num)
                return &m_bucket_array[index];
            else
                return NULL;
        }

        bucket_type * get_bucket_by_sig(sig_t sig) const {
            return get_bucket_by_index(sig & m_mask);
        }


    private:
        hasher       m_hash_func;
        uint32       m_mask;
        uint32       m_bucket_num;
        bucket_type *m_bucket_array;
};

__SHM_STL_END

#endif
