#ifndef __SHM_BUCKET_H_
#define __SHM_BUCKET_H_

#include <sys/types.h>
#include <memory.h>
#include <iostream>
#include <sstream>
#include <rte_malloc.h>
#include "shm_common.h"

using std::ostream;
    
__SHM_STL_BEGIN

const u_int32_t DEFAULT_BUCKET_NUM = 512;

/* typedef */
typedef u_int32_t sig_t;
typedef u_int32_t uint32;
typedef int32_t   int32;

template <typename _Node>
struct PrintNode {
    void operator() (const _Node & node, ostream & os) {
        os << "[" << node.index() << "] --> ";
    }
};

template <typename _Node, typename _Key, typename _KeyEqual>
class Bucket {
    public:
        Bucket () : m_size(0), m_head(NULL) {}
        ~Bucket () {clear();}

        void clear(void) {
            m_size = 0;
            m_head = NULL;
        }

        // Put a node at the head of this bucket
        void put(_Node * node) {
            node->set_next(m_head);
            m_head = node;
            ++m_size;
        }

        // Lookup a node by signature and key
        _Node * lookup(const sig_t &sig, const _Key &key) {
            // Search in this bucket
            _Node * current = m_head;
            _Node * prev = current;
            while (current) {
                if (sig == current->signature() && m_equal_to(key, current->key())) {
                    break;
                }

                prev = current;
                current = current->next();
            }

            // If we find this key in bucket, move it to the front of bucket
            if (current != NULL) {
                if (current != m_head) {
                    prev->set_next(current->next());
                    current->set_next(m_head);
                    m_head = current;
                }

#ifdef DEBUG
                std::ostringstream log;
                str(log);
                std::cout << log.str() << std::endl;
#endif
            }

            return current;
        }

        // Remove a node from this bucket
        _Node * remove(const sig_t &sig, const _Key &key) {
            _Node * node = lookup(sig, key);

            // If we find this node, now it is in the front of our node list,
            // we just remove it from the head
            if (node) {
                m_head = node->next();
                node->set_next(NULL);
                --m_size;
            }

            return node;
        }

        uint32  size(void) const {return m_size;}
        _Node * head(void) const {return m_head;}
        _Node * tail(void) const {
            if (m_head == NULL)
                return NULL;

            _Node * current = m_head;
            while (current->next()) {
                current = current->next();
            }

            // If we find a node whose next is NULL, it is the tail of this bucket
            return current;
        }

        void str(ostream &os) {
            os << "\nBucket Size : " << m_size << std::endl;
            _Node * curr = m_head;
            while (curr) {
                curr->str(os);
                curr = curr->next();
            }
        }

    public:
        uint32 m_size; // the size of this bucket
        _Node *m_head; // the pointer of the first node in this bucket
        _KeyEqual m_equal_to;
}; 

template <typename _Bucket>
class BucketMgr {
    public:
        typedef _Bucket bucket_t;

        BucketMgr(uint32 size) : m_size(size), m_mask(0), m_bucket_array(NULL) {
            initialize();
        }

        ~BucketMgr(void) {
            if (m_bucket_array) {
                rte_free(m_bucket_array);
                m_bucket_array = NULL;
            }
            
            m_size = 0;
            m_mask = 0;
        }

        inline bucket_t * get_bucket_by_index(uint32 index) const {
            if (index < m_size)
                return &m_bucket_array[index];
            else
                return NULL;
        }

        inline bucket_t * get_bucket_by_sig(sig_t sig) const {
            return get_bucket_by_index(sig & m_mask);
        }

        inline uint32 size(void) const {
            return m_size;
        }

        inline void str(ostream &os) const {
            os << "** Total Buckets : " << m_size << std::endl;
            os << "** Bucket Mask   : 0x" << std::hex << m_mask << std::dec << std::endl;

            for (uint32 i = 0; i < m_size; ++i) {
                os << std::endl;
                os << "Bucket[" << i << "]" << std::endl;
                m_bucket_array[i].str(os); 
            }
        }

    private:
        inline bool initialize(void) {
            // Adjust bucket number if necessary
            if (!is_power_of_2(m_size))
                m_size = convert_to_power_of_2(m_size);

            m_mask = m_size - 1;

            // Allocate memory for bucket 
            // m_bucket_array = new bucket_t[m_size];
            char name[] = "bucket_array";
            uint32 bucket_array_size_in_bytes = m_size * sizeof(bucket_t);
            m_bucket_array = static_cast<bucket_t*>(rte_zmalloc(name, bucket_array_size_in_bytes, 0));
            if (m_bucket_array == NULL)
                return false;
            else
                return true;
        }

    private:
        uint32    m_size;
        uint32    m_mask;
        bucket_t *m_bucket_array;
};

__SHM_STL_END

#endif
