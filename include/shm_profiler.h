#ifndef __SHM_PROFILER_H_
#define __SHM_PROFILER_H_

#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>

#include <rte_cycles.h>

#include "shm_common.h"

__SHM_STL_BEGIN

class Profiler {
    public:
        static const uint32_t k_StatsSize = 20;
        static const uint32_t k_MaxCnt = 1 << 15;
        static const uint32_t k_MaxCycles = 1 << 30;
        static const uint32_t k_MaxNameSize = 128; 

        class Stats {
            public:
                Stats(void) : cycles(0), cnt(0) {}
                uint64_t cycles;
                uint32_t cnt;
        };

        Profiler(const char * name, uint32_t max_cnt = k_MaxCnt, uint32_t max_cycle = k_MaxCycles)
            : m_enabled(true)
            , m_ready_to_log(false)
            , m_max_cnt(max_cnt)
            , m_max_cycle(max_cycle) {
            copy_name(&m_filename[0], name);
            memset(&m_stats_name[0][0], 0, k_StatsSize * k_MaxNameSize);
        }
        ~Profiler() {}

        void set_stats_name(const uint32_t index, const char * name) {copy_name(&m_stats_name[index][0], name);}
        void disable(void) {m_enabled = false;}
        void enable(void) {m_enabled = true;}

        uint64_t start(void) {
            if (m_enabled && m_ready_to_log) {
                log_to_file();
                clear();
            }

            return read_tsc();
        }

        uint64_t stop(uint32_t index, uint64_t start) {
            if (m_enabled)
                accumulate(index, read_tsc() - start);

            return read_tsc();
        }

        void log_to_file(std::ostringstream &log) {
            std::ostringstream filename;
            filename << "/tmp/shm_profiler_" << m_filename << getpid() << ".txt";
            std::ofstream ofs(filename.str().c_str(), std::ofstream::out);
            if (ofs.is_open()) {
                ofs << log.str().c_str() << std::endl;
                ofs.close();
            }
        }

    private:
        uint64_t read_tsc(void) {return rte_rdtsc();}

        void copy_name(char * dst, const char * src) {
            if (src) {
                strncpy(dst, src, k_MaxNameSize - 1);
                dst[k_MaxNameSize - 1] = '\0';
            } else {
                memset(dst, 0, k_MaxNameSize);
            }
        }

        void accumulate(uint32_t index, uint64_t delta) {
            if (index >= k_StatsSize)
                return;

            m_stats[index].cycles += delta;
            m_stats[index].cnt++;

            // If it exceeds the max count or cycles, we are ready to log
            if ((m_stats[index].cnt > m_max_cnt) || (m_stats[index].cycles > m_max_cycle))
                m_ready_to_log = true;
        }

        void clear(void) {
            for (uint32_t i = 0; i < k_StatsSize; ++i) {
                m_stats[i].cycles = 0;
                m_stats[i].cnt = 0;
            }

            m_ready_to_log = false;
        }

        void log_to_file(void) {
            std::ostringstream filename;
            filename << "/tmp/shm_profiler_" << m_filename << getpid() << ".txt";
            //filename << "/tmp/shm_profiler_" << getpid() << ".txt";

            // Do not use std::ofstream::app here, it may cause a huge log file
            std::ofstream ofs(filename.str().c_str());
            if (ofs.is_open()) {
                for (uint32_t i = 0; i < k_StatsSize; ++i) {
                    if (m_stats[i].cnt > 0) {
                        ofs << "Statistic " << i << " - " << &m_stats_name[i][0] 
                            << " *** cycles :" << (m_stats[i].cycles / m_stats[i].cnt) 
                            << ", cnt : " << m_stats[i].cnt << std::endl; 
                    }
                }
                ofs.close();
            }
        }

    private:
        bool m_enabled;
        bool m_ready_to_log;
        char m_filename[k_MaxNameSize];
        Stats m_stats[k_StatsSize];
        char m_stats_name[k_StatsSize][k_MaxNameSize];
        uint32_t m_max_cnt;
        uint32_t m_max_cycle;
};

__SHM_STL_END

#endif
