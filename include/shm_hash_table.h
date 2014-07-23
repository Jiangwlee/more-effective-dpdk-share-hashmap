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
#include <bits/stl_function.h>
#include <memory.h>
#include <iostream>
#include <sstream>
#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_rwlock.h>
#include "hash_fun.h"
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

const u_int32_t DEFAULT_ENTRIES = 4096;

template <typename _Key, typename _Value>
class Node {
    public:
        Node () : m_sig(0), m_next(NULL) {rte_rwlock_init(&m_lock);}
        ~Node () {}
        
        void fill(_Key k, _Value v, sig_t s) {
            rte_rwlock_write_lock(&m_lock);
            m_key = k;
            m_value = v;
            m_sig = s;
            rte_rwlock_write_unlock(&m_lock);
        }

        void set_next(Node * next) {
            rte_rwlock_write_lock(&m_lock);
            m_next = next;
            rte_rwlock_write_unlock(&m_lock);
        }

        void set_index(uint32 idx) {
            rte_rwlock_write_lock(&m_lock);
            m_index = idx;
            rte_rwlock_write_unlock(&m_lock);
        }

        template <typename _Tmpvalue, typename _Modifier>
        void update(_Tmpvalue & tmp_value, _Modifier &action) {
            rte_rwlock_write_lock(&m_lock);
            action(m_value, tmp_value);
            rte_rwlock_write_unlock(&m_lock);
        }

        _Key key(void) const {return m_key;}
        _Value value(void) const {return m_value;}
        sig_t signature(void) const {return m_sig;}
        Node * next(void) const {return m_next;}
        uint32 index(void) const {return m_index;}

        void str(ostream &os) {
            os << "[ <" << m_key << ", " << m_value << ">, " << m_sig << " ] --> " << std::endl; 
        }

    private:
        _Key   m_key;
        _Value m_value; // The member of _Key and _Value should be volatile
        sig_t  m_sig;   // the sinature - hash value
        Node * volatile m_next;  // the pointer of next node
        uint32 m_index; // the index of this node in node list, it should never be changed after initialization
        rte_rwlock_t m_lock;
};

/*
 * @brief : FreeNodePool manages free nodes used by hashmap. A hashmap should always
 *          get a free node from FreeNodePool and return it back when it decides to
 *          erase a node from hashmap.
 *
 *          FreeNodePool can resize itself if all free nodes are exhausted. It creates
 *          a new free node list whose size is double of previous free node list. The
 *          max resize count is 32 by default. It means you can create 32 free node
 *          lists including the first one at most.
 *
 *          All free nodes in free node lists are chained together and be accessed
 *          from m_nodepool_head.
 *
 *          This class provides following methods to programmers:
 *          1. GetNode - Get a free node from FreeNodePool
 *          2. PutNode - Put a node to FreeNodePool
 *          3. PutNodeList - Put a list of nodes to FreeNodePool
 *
 *          Important:
 *          1. Programmers should not free any node outside of FreeNodePool
 *
 *          Following is a chart to illustrate this class:
 *
 *          m_freelist_array --> +-----------------------------------------------+
 *                                |     |     |     |     |     |     |     |     | 
 *                                +-----------------------------------------------+
 *                 [the first list]  |     |   [create the second free list if the first is exhausted]
 *                                   V     +-----> +-------------------------------+
 *          m_nodepool_head -->  +-----+          |   |   |   |   |   |   |   |   |
 *                                |     |          +-------------------------------+
 *                                +-----+            ^
 *                                |     |            |
 *                                +-----+            |
 *                                |     |            |
 *                                +-----+            |
 *                                |     |            | [chain the new created free nodes to m_nodepool_head]
 *                                +-----+            |
 *                                   |_______________|
 *
 *  About The lock:
 *  The NodePool is in share memory. The lock is to protect the data of a NodePool object.
 *
 *  For public methods, they should be protected by m_lock. And you must make sure are not
 *  called by other methods of NodePool. Otherwise, it make cause a dead lock. If a
 *  public method has to be called by another method , create a private method like
 *  return_nodelist, then call the private method from these method. This small trick can
 *  make sure all public method are proteced by m_lock
 *
 *  For private methods, because all public methods are protected by m_lock, they are also
 *  protected. 
 *
 *               +--------------------------+
 *  client --->  | public methods with lock | ---> < private methods >
 *               +--------------------------+                 |
 *                            |                               v
 *                            -------------------> < private date in share memory >
 * */
template <typename _Node>
class NodePool {
    public:
        typedef _Node node_type;
        static const uint32 MAX_RESIZE_COUNT = 5;
        static const uint32 DEFAULT_LIST_SIZE = 16;  // The default size of the first free list

        NodePool(uint32 size) : m_capacity(0), m_free_entries(0), m_freelist_num(0), 
                                m_next_freelist_size(size), m_nodepool_head(NULL) {
                                    rte_rwlock_init(&m_lock);
                                    resize();
                                }

        ~NodePool() {
            rte_rwlock_write_lock(&m_lock);

            for (uint32 i = 0; i < m_freelist_num; ++i) {
                void * free_list = (void *)(m_freelist_array[i]);
                rte_free(free_list);
                m_freelist_array[i] = NULL;
            }

            m_capacity = 0;
            m_free_entries = 0;
            m_freelist_num = 0;
            m_next_freelist_size = DEFAULT_LIST_SIZE;
            m_nodepool_head = NULL;

            rte_rwlock_write_unlock(&m_lock);
        }

        // Get a free node
        node_type * get_node(void) {
            rte_rwlock_write_lock(&m_lock);

            node_type * head = NULL;
            if (m_nodepool_head == NULL)
                resize();

            if (m_nodepool_head == NULL)
                goto exit;

            head = m_nodepool_head;
            if (head->next() == NULL) {
                // If the head is the last free node
                m_nodepool_head = NULL;
            } else {
                // If the head is not the last free node, m_free_node_list points to the next free node
                m_nodepool_head = head->next(); 
            }

            --m_free_entries;
            construct_node(head);

exit:
            rte_rwlock_write_unlock(&m_lock);
            return head;
        }

        // Return a node to free list
        void put_node(node_type * node) {
            if (node == NULL)
                return;

            rte_rwlock_write_lock(&m_lock);

            // Put this node at the front of m_free_node_list
            node->set_next(m_nodepool_head); 
            m_nodepool_head = node;

            ++m_free_entries;

            rte_rwlock_write_unlock(&m_lock);
        }

        // Return nodes in a bucket to free list
        void put_nodelist(node_type *start, node_type *end, uint32 size) {
            // If start or end is NULL, do nothing
            if (!start || !end)
                return;

            rte_rwlock_write_lock(&m_lock);
            return_nodelist(start, end, size);
            rte_rwlock_write_unlock(&m_lock);
        }

        // Following three methods do not use lock
        uint32 capacity(void) const {return m_capacity;}
        uint32 free_entries(void) const {return m_free_entries;}

        void print(void) const {
            std::ostringstream os;
            str(os);
            std::cout << os.str() << std::endl;

            os << "\nFree Node Pool : " << std::endl;
            node_type * start = m_nodepool_head;
            PrintNode<node_type> action;
            while (start) {
                action(*start, os);
                start = start->next();
            }

            std::cout << os.str() << std::endl;
        }

    private:
        // Create a new free node list and chain it to free node pool
        void resize(void) {
            // Have reached the maxinum size
            if (m_freelist_num >= MAX_RESIZE_COUNT)
                return;

            // Create a new free list
            uint32 node_cnt = m_next_freelist_size;
            uint32 list_size_in_byte = node_cnt * sizeof(node_type);
            std::ostringstream name;
            name << "NodePool_FreeList_" << m_freelist_num;
            node_type * new_list = static_cast<node_type *>(rte_zmalloc(name.str().c_str(), list_size_in_byte, 0));
            if (new_list == NULL)
                return;

            // Now we have created the new free list successfully, add it to m_freelist_array
            // and free node pool
            initialize_freenode_list(new_list, node_cnt, m_capacity);
            m_freelist_array[m_freelist_num] = new_list;
            node_type * end_of_list = &new_list[node_cnt - 1];
            return_nodelist(new_list, end_of_list, node_cnt); // PutNodeList will calculate m_free_entries

            // Calculate new capacity, free_list_num and next_free_list_size 
            m_capacity += node_cnt;
            m_freelist_num++;
            m_next_freelist_size = node_cnt << 1;

#ifdef DEBUG
            std::cout << "Just Resize Node Pool! ...... " << std::endl;
            print();
#endif
        }

        void initialize_freenode_list(node_type *list, uint32 size, uint32 index_start) {
            uint32 i = 0;
            for ( ; i < size - 1; ++i) {
                // call the constructor of node_type
                node_type * node = &list[i];
                construct_node(node);
                node->set_next(&list[i + 1]);
                node->set_index(index_start++);
            }

            // Set the index of last node
            list[i].set_index(index_start);
        }

        // This private function does not use lock
        void return_nodelist(node_type *start, node_type *end, uint32 size) {
            // If start or end is NULL, do nothing
            if (!start || !end)
                return;

            // Put the node list decribed by start and end at the front of m_nodepool_head
            end->set_next(m_nodepool_head);
            m_nodepool_head = start;
            m_free_entries += size;
        }

        void construct_node(node_type *node) {::new ((void *)node) node_type;}

        void str(std::ostream &os) {
            os << "Node Pool Status : " << std::endl;
            os << "Capacity      : " << m_capacity << std::endl;
            os << "Free entries  : " << m_free_entries << std::endl;
            os << "Free list num : " << m_freelist_num << std::endl;
        }

    private:
        volatile uint32     m_capacity;           // the capacity of this free node pool
        volatile uint32     m_free_entries;       // the count of available free nodes in this pool
        volatile uint32     m_freelist_num;       // how many free lists we have now
        volatile uint32     m_next_freelist_size; // the size of next free list
        node_type * volatile m_nodepool_head;      // the head of free node pool
        node_type * volatile m_freelist_array[MAX_RESIZE_COUNT]; // free lists
        rte_rwlock_t m_lock;               // used to protect this NodePool in share memory
};

template <typename _Key, typename _Value, typename _HashFunc = hash<_Key>, typename _EqualKey = std::equal_to<_Key> >
class hash_table {
    public:
        typedef Node<_Key, _Value> node_type;
        typedef _Key key_type;
        typedef _Value value_type;
        typedef _HashFunc hasher;
        typedef _EqualKey key_equal;
        typedef Bucket<node_type, key_type, key_equal>  bucket_type;
        typedef NodePool<node_type> node_pool_type;
        typedef BucketMgr<bucket_type> bucket_mgr;

    public:
        hash_table(uint32 entries = DEFAULT_ENTRIES, uint32 buckets = DEFAULT_BUCKET_NUM) : 
                   m_buckets(buckets), m_node_pool(entries) {}

        ~hash_table(void) {}

        bool insert(const key_type & key, const value_type & value) {
            // Check if this key is already in hash table
            if (find(key))
                return false;
                        
            // Get a new node from free list
            node_type * node = m_node_pool.get_node();
            if (node == NULL)
                return false;

            // Compute signature and fill the node
            sig_t sig = m_hash_func(key);
            node->fill(key, value, sig);

            // Put node to bucket
            bucket_type * bucket = m_buckets.get_bucket_by_sig(sig); 
            bucket->put(node);

#ifdef DEBUG
            m_node_pool.print();
            print_bucketlist(*bucket);
#endif

            return true;
        }

        /*
         * @brief
         *  This method takes two parameters :
         *  key is an input parameter for hash table lookup
         *  ret is an output parameter to take the value if the key is in the hash table
         * */
        bool find(const key_type & key, value_type * ret = NULL) {
            node_type * node = lookup_node_by_key(key);
            if (node) {
                if (ret) *ret = node->value();
                return true;
            } else {
                return false;
            }
        }

        bool erase(const key_type &key, value_type * ret = NULL) {
            // Compute signature and get bucket
            sig_t sig = m_hash_func(key);
            bucket_type * bucket = m_buckets.get_bucket_by_sig(sig);

            // Remove this node from bucket
            if (bucket) { 
                node_type * node = bucket->remove(sig, key);
                if (node) {
                    if (ret) *ret = node->value();

                    // Put this node to free node list
                    m_node_pool.put_node(node);
#ifdef DEBUG
                    m_node_pool.print();
                    print_bucketlist(*bucket);
#endif
                    return true;
                }
            }

#ifdef DEBUG
            m_node_pool.print();
            print_bucketlist(*bucket);
#endif

            return false;
        }

        // Update the value
        template <typename _Modifier>
        bool update(const key_type & key, value_type & new_value, _Modifier &action) {
            node_type * node = lookup_node_by_key(key);
            if (node) {
                node->update(new_value, action); 
                return true;
            } else {
                return false;
            }
        }

        // Clear this hash table
        void clear(void) {
            for (uint32 i = 0; i < m_buckets.size(); ++i) {
                put_bucket_to_freelist(m_buckets.get_bucket_by_index(i));
            }
#ifdef DEBUG
            m_node_pool.print();
#endif
        }

        void str(ostream & os) const {
            os << "\nHash Table Information : " << std::endl;
            os << "** Total Entries : " << m_node_pool.capacity() << std::endl;
            os << "** Free  Entries : " << m_node_pool.free_entries() << std::endl;
            m_buckets.str(os);
        }

    private:
        template <typename _Action>
        void traval_nodelist(node_type * head, _Action action, ostream &os) const {
            if (head == NULL)
                return;

            node_type * current_node = head;
            while (current_node != NULL) {
                action(*current_node, os);
                current_node = current_node->next();
            }
        }


        // Return nodes in a bucket to free list
        void put_bucket_to_freelist(bucket_type *bucket) {
            if (bucket == NULL)
                return;

            node_type * start = bucket->clear();
            node_type * end = start;
            uint32 size = 0;

            while (end) {
                ++size;
                end = end->next();
            }

            // Put nodes to m_node_pool
            m_node_pool.put_nodelist(start, end, size);

#ifdef DEBUG
            print_bucketlist(*bucket);
            std::ostringstream log;
            bucket->str(log);
            std::cout << log.str() << std::endl;
#endif
        }

        node_type * lookup_node_by_key(const key_type & key) {
            // Compute signature and get bucket
            sig_t sig = m_hash_func(key);
            bucket_type * bucket = m_buckets.get_bucket_by_sig(sig);

            // Search in this bucket
            if (bucket)
                return bucket->lookup(sig, key); 
            else
                return NULL;
        }

        void print_bucketlist(const bucket_type &bucket) const {
            std::ostringstream os;
            PrintNode<node_type> action;
            os << "\nCurrent Bucket is : " << std::endl;
            traval_nodelist(bucket.head(), action, os);
            std::cout << os.str() << std::endl;
        }
        
    private:
        hasher m_hash_func;
        bucket_mgr m_buckets;
        node_pool_type m_node_pool;
};

__SHM_STL_END

#endif
