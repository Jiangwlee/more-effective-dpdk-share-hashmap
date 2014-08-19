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


#ifndef __SHM_BUCKET_H_
#define __SHM_BUCKET_H_

#include <sys/types.h>
#include <memory.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include "shm_node_pool.h"
#include "shm_profiler.h"

using std::ostream;
    
__SHM_STL_BEGIN

const u_int32_t DEFAULT_BUCKET_NUM = 4096;
const u_int32_t ENTRIES_PER_BUCKET = 16;

template <typename _Node, typename _Key, typename _Value, typename _KeyEqual>
class Bucket {
    public:
        typedef _Node node_t;
        typedef _Key  key_t;
        typedef _Value value_t;
        typedef NodePool<node_t> node_pool_t;

    public:
        Bucket (uint32 pool_size = ENTRIES_PER_BUCKET)
            : m_node_pool(pool_size), m_size(0), m_head(NULL){
                rte_rwlock_init(&m_lock);
            }
        ~Bucket () {}

        uint32 capacity(void) const {return m_node_pool.capacity();}
        uint32 free_entries(void) const {return m_node_pool.free_entries();}

        void clear(void) {
            if (m_head == NULL) {
                m_size = 0;
                return;
            }

            rte_rwlock_write_lock(&m_lock);
            
            // return nodes to node pool
            node_t* end = m_head;
            while (end->next())
                end = end->next();

            m_node_pool.put_nodelist(m_head, end, m_size);

            m_size = 0;
            m_head = NULL;

            rte_rwlock_write_unlock(&m_lock);
            return;
        }

        // Put a node at the head of this bucket
        bool put(const sig_t &signature, const key_t &key, const value_t &value) {
            bool ret = true;
            node_t * node = NULL;

            rte_rwlock_write_lock(&m_lock);

            // check if this key is already in this bucket
            if (find_node(signature, key)) {
                ret = false;
                goto exit;
            }

            node = m_node_pool.get_node();
            if (node == NULL) {
                ret = false;
                goto exit;
            }
            node->fill(key, value, signature);
            node->set_next(m_head);
            m_head = node;
            ++m_size;
            
exit:
            rte_rwlock_write_unlock(&m_lock);
            return ret;
        }

        // Lookup a node by signature and key
        bool lookup(const sig_t &sig, const key_t &key, value_t * ret) {
            rte_rwlock_read_lock(&m_lock);

            node_t* node = find_node(sig, key);
            if (node && ret) *ret = node->value();

            rte_rwlock_read_unlock(&m_lock);

            if (node)
                return true;
            else
                return false;
        }

        // Remove a node from this bucket
        bool remove(const sig_t &sig, const key_t &key, value_t * ret) {
            rte_rwlock_write_lock(&m_lock);

            node_t * node = find_node(sig, key);

            // If we find this node, now it is in the front of our node list,
            // we just remove it from the head
            if (node) {
                if (ret)
                    *ret = node->value();

                m_head = node->next();
                node->set_next(NULL);
                --m_size;
                
                // put this node back to node_pool
                m_node_pool.put_node(node);
            }

            rte_rwlock_write_unlock(&m_lock);

            if (node)
                return true;
            else
                return false;
        }

        // update a node in this bucket
        template <typename _Params, typename _Modifier>
        bool update(const sig_t &sig, const key_t &key, _Params &params, _Modifier &action) {
            rte_rwlock_write_lock(&m_lock);

            bool ret = false;
            node_t * node = find_node(sig, key);

            // If we find this node, update it! 
            if (node) {
                node->update(params, action);
                ret = true;
            }
            
            rte_rwlock_write_unlock(&m_lock);
            return ret;
        } 

        uint32  size(void) const {return m_size;}

        void str(ostream &os) const {
            os << "\nBucket Size : " << m_size << std::endl;
            node_t* curr = m_head;
            while (curr) {
                curr->str(os);
                curr = curr->next();
            }
        }

    private:
        node_t * find_node(const sig_t &sig, const key_t &key) const {
            // Search in this bucket
            node_t * current = m_head;
            while (current) {
                if (sig == current->signature() && m_equal_to(key, current->key())) {
                    break;
                }

                current = current->next();
            }

            return current;
        }

    public:
        node_pool_t m_node_pool;
        volatile uint32 m_size; // the size of this bucket
        node_t * volatile m_head; // the pointer of the first node in this bucket
        _KeyEqual m_equal_to;
        rte_rwlock_t m_lock;
}; 

__SHM_STL_END

#endif
