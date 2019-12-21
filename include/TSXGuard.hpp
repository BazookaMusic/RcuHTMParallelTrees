#ifndef INCLUDE_TSX_GUARD_HPP
    
    #define INCLUDE_TSX_GUARD_HPP

#include <atomic>
#include <vector>
#include "rtm.h"
#include "emmintrin.h"
#include "iostream"

namespace TSX {
    static constexpr int ALIGNMENT = 128;
    static constexpr unsigned char ABORT_VALIDATION_FAILURE = 0xee;
    static constexpr unsigned char ABORT_GL_TAKEN = 0;
    static constexpr unsigned char USER_OPTION_LOWER_BOUND = 0x01;

    enum RETRY_STRATEGY {
        STUBBORN,
        HALF
    };

    bool transaction_pending() {
        return _xtest() > 0;
    }

    class SpinLock {
        private:
            enum {
                UNLOCKED = false,
                LOCKED = true
            };
            std::atomic<bool> spin_lock_;
        public:
            SpinLock(): spin_lock_(false) {}

            void lock() noexcept{


                for (;;) {
                    // test
                    while (spin_lock_.load(std::memory_order_relaxed) == LOCKED) _mm_pause();

                    if (!spin_lock_.exchange(LOCKED)) {
                        break;
                    }

                }
            }

            void unlock() noexcept{
                spin_lock_.store(false);
            }

            bool isLocked() noexcept {
                return spin_lock_.load(std::memory_order_relaxed);
            }

    };

    enum {
	TX_ABORT_CONFLICT = 0,
	TX_ABORT_CAPACITY,
	TX_ABORT_EXPLICIT,
    TX_ABORT_LOCK_TAKEN,
	TX_ABORT_REST,
	TX_ABORT_REASONS_END
    };

    struct alignas(ALIGNMENT) TSXStats {
         long long tx_starts,
            tx_commits,
            tx_aborts,
            tx_lacqs;

        long long tx_aborts_per_reason[TX_ABORT_REASONS_END];

        TSXStats(): tx_starts(0), tx_commits(0), tx_aborts(0), tx_lacqs(0) {
            for (int i = 0; i < TX_ABORT_REASONS_END; i++) {
                tx_aborts_per_reason[i] = 0;
            }
        }

        void reset() {
            tx_starts = tx_commits = tx_aborts = tx_lacqs = 0;
            for (int i = 0; i < TX_ABORT_REASONS_END; i++) {
                tx_aborts_per_reason[i] = 0;
            }

        }


        void print_stats() {
            std::cout << "Transaction stats:" << std::endl 
            << "Starts:" << tx_starts << std::endl <<
            "Commits:" << tx_commits << std::endl <<
            "Aborts:" << tx_aborts << std::endl <<
            "Lock acquisitions:" << tx_lacqs << std::endl <<
            "Conflict Aborts:" << tx_aborts_per_reason[0] << std::endl <<
            "Capacity Aborts:" << tx_aborts_per_reason[1] << std::endl <<
            "Explicit Aborts:" << tx_aborts_per_reason[2] << std::endl <<
            "Lock Taken Aborts:" << tx_aborts_per_reason[3] << std::endl <<
            "Other Aborts:" << tx_aborts_per_reason[4] << std::endl;

        }

        void print_lite_stats() {
            double aborts_per_start;
            double commit_pc;

            if (tx_starts > 0) {
                aborts_per_start = (1.0*tx_aborts) / (tx_starts);
                commit_pc =  (100.0*tx_commits) / (tx_starts);
            } else {
                aborts_per_start = 0;
                commit_pc = 0;
            }

            double confl_pc;
            double cap_pc;
            double lock_pc;
            double explicit_pc;

            if (tx_aborts > 0) {
                confl_pc = (100.0*tx_aborts_per_reason[0]) / (tx_aborts);
                cap_pc = (100.0*tx_aborts_per_reason[1]) / (tx_aborts);
                lock_pc = (100.0*tx_aborts_per_reason[3]) / (tx_aborts);
                explicit_pc = (100 * tx_aborts_per_reason[2]) / (tx_aborts);
            } else {
                confl_pc = 0;
                cap_pc = 0;
                lock_pc = 0;
                explicit_pc = 0;

            }
            
            
            std::cout << "ABORTS: " << tx_aborts / 1000000.0 << "M" << std::endl <<
            "AB/START:" << aborts_per_start << std::endl <<
            "CONF:" << confl_pc << "%   " << "CAP:" << cap_pc << "%   " << "LOCK:" << lock_pc << "%" <<
            "EXPL: " << explicit_pc << "%" << std::endl <<
            "COM/START:" << commit_pc << "%" << std::endl ;

        }

        TSXStats operator+(const TSXStats& rhs ) {
            TSXStats total_stats;
            total_stats.tx_starts += rhs.tx_starts;
            total_stats.tx_commits += rhs.tx_commits;
            total_stats.tx_aborts += rhs.tx_aborts;
            total_stats.tx_lacqs += rhs.tx_lacqs;
            total_stats.tx_aborts_per_reason[0] += rhs.tx_aborts_per_reason[0];
            total_stats.tx_aborts_per_reason[1] += rhs.tx_aborts_per_reason[1];
            total_stats.tx_aborts_per_reason[2] += rhs.tx_aborts_per_reason[2];
            total_stats.tx_aborts_per_reason[3] += rhs.tx_aborts_per_reason[3];
            total_stats.tx_aborts_per_reason[4] += rhs.tx_aborts_per_reason[4];
            return total_stats;
        }

        TSXStats& operator+=(const TSXStats& rhs ) {
            tx_starts += rhs.tx_starts;
            tx_commits += rhs.tx_commits;
            tx_aborts += rhs.tx_aborts;
            tx_lacqs += rhs.tx_lacqs;
            tx_aborts_per_reason[0] += rhs.tx_aborts_per_reason[0];
            tx_aborts_per_reason[1] += rhs.tx_aborts_per_reason[1];
            tx_aborts_per_reason[2] += rhs.tx_aborts_per_reason[2];
            tx_aborts_per_reason[3] += rhs.tx_aborts_per_reason[3];
            tx_aborts_per_reason[4] += rhs.tx_aborts_per_reason[4];
            return *this;
        }






    };


    TSXStats total_stats(std::vector<TSXStats> stats) {
        TSXStats total_stats;

        for (auto i = stats.begin(); i != stats.end(); i++) {
            total_stats += *i;
        }

        return total_stats;
    }


    // TSXGuard works similarly to std::lock_guard
    // but uses hardware transactional memory to 
    // achieve synchronization. The result is the
    // synchronization of commands in all TSXGuards'
    // scopes.
    class TSXGuard {
    protected:
       
        const int max_retries;  // how many retries before lock acquire
        SpinLock &spin_lock_;    // fallback
        bool has_locked;        // avoid checking global lock if haven't locked
        bool user_explicitly_aborted;   // explicit user aborts mean that lock is not taken and
                                        // transaction not pending
        int nretries;           // how many retries have been made so far
                                // used to resume transaction in case of user abort
        bool disabled;
    public:
        TSXGuard(const int max_tx_retries, SpinLock &mutex, unsigned char &err_status,  bool disabled = false, RETRY_STRATEGY strat = STUBBORN): 
        max_retries(max_tx_retries),
        spin_lock_(mutex),
        has_locked(false),
        user_explicitly_aborted(false),
        nretries(max_tx_retries),
        disabled(disabled)
        {
            if (!disabled) {
                while(1) {
                    --nretries;

                    // try to init transaction
                    unsigned int status = _xbegin();
                    if (status == _XBEGIN_STARTED) {      // tx started
                        if (!spin_lock_.isLocked()) return; //successfully started transaction
                        // started txn but someone is executing the txn  section non-speculatively
                        // (acquired the  fall-back lock) -> aborting
                        _xabort(ABORT_GL_TAKEN); // abort with code 0xff
                    } else if (strat == HALF && (status & _XABORT_CONFLICT))  {
                        nretries >>= 1; // half strategy
                    } else if (status & _XABORT_EXPLICIT) {
                        if (_XABORT_CODE(status) == ABORT_GL_TAKEN && !(status & _XABORT_NESTED)) {
                            while (spin_lock_.isLocked()) _mm_pause();
                        } else if (_XABORT_CODE(status) > USER_OPTION_LOWER_BOUND) {
                            user_explicitly_aborted = true;
                            err_status = _XABORT_CODE(status);
                            return;
                        } else if(!(status & _XABORT_RETRY)) {
                            // if the system recommends not to retry
                            // go to the fallback immediately
                            goto fallback_lock; 
                        } 
                    } 

                    // too many retries, take the fall-back lock 
                    if (!nretries) break;

                }   //end
        fallback_lock:
                    has_locked = true;
                    spin_lock_.lock();

            } 
        }

        // abort_to_retry: aborts current transaction
        // and returns retries left
        // in order to retry transaction.
        // Takes the error code as a template
        // parameter.
        template <unsigned char imm>
        int abort_to_retry() {
            static_assert(imm > USER_OPTION_LOWER_BOUND, 
            "User aborts should be larger than USER_OPTION_LOWER_BOUND, as lower numbers are reserved");
            _xabort(imm);
            user_explicitly_aborted = true;
            return max_retries - nretries;
        }

        // abort: aborts current transaction.
        // Takes the error code as a template
        // parameter.
        template <unsigned char imm>
        static void abort() {
            static_assert(imm > USER_OPTION_LOWER_BOUND, 
            "User aborts should be larger than USER_OPTION_LOWER_BOUND, as lower numbers are reserved");
            _xabort(imm);
        }


        ~TSXGuard() {
            if (!user_explicitly_aborted && !disabled) {
                // no abort code
                if (has_locked && spin_lock_.isLocked()) {
                    spin_lock_.unlock();
                } else {
                    _xend();
                }
            }
            
        }     
    };


    class TSXGuardWithStats {
    private:
        const int max_retries;  // how many retries before lock acquire
        SpinLock &spin_lock_;    // fallback
        bool has_locked;        // avoid checking global lock if haven't locked
        bool user_explicitly_aborted;   // explicit user aborts mean that lock is not taken and
                                        // transaction not pending
        int nretries;           // how many retries have been made so far
                                // used to resume transaction in case of user abort
        TSXStats &_stats;
        bool disabled;

    public:
        TSXGuardWithStats(const int max_tx_retries, SpinLock &mutex, unsigned char &err_status, TSXStats &stats, bool disabled = false, RETRY_STRATEGY strat = STUBBORN):
        max_retries(max_tx_retries),
        spin_lock_(mutex),
        has_locked(false),
        user_explicitly_aborted(false),
        nretries(max_tx_retries),
        _stats(stats),
        disabled(disabled)
        {
            

            if (!disabled) {
                while(true) {

                    --nretries;
                    
                    // try to init transaction
                    unsigned int status = _xbegin();
                    if (status == _XBEGIN_STARTED) {   // tx started
                        _stats.tx_starts++;
                        if (!spin_lock_.isLocked()) return;  //successfully started transaction
                        
                        // started txn but someone is executing the txn  section non-speculatively 
                        // (acquired the  fall-back lock) -> aborting

                        _xabort(ABORT_GL_TAKEN); // abort with code 0xff  
                    } else if (status & _XABORT_CAPACITY) {
                        _stats.tx_aborts++;
                        _stats.tx_aborts_per_reason[TX_ABORT_CAPACITY]++;
                    } else if (status & _XABORT_CONFLICT) {
                        _stats.tx_aborts++;
                        _stats.tx_aborts_per_reason[TX_ABORT_CONFLICT]++;

                        if (strat == HALF) {
                            nretries >>= 1;
                        }
                    } else if (status & _XABORT_EXPLICIT) {
                        _stats.tx_aborts++;
                        _stats.tx_aborts_per_reason[TX_ABORT_EXPLICIT]++;
                        if (_XABORT_CODE(status) == ABORT_GL_TAKEN && !(status & _XABORT_NESTED)) {
                            _stats.tx_aborts_per_reason[TX_ABORT_LOCK_TAKEN]++;
                            while (spin_lock_.isLocked()) _mm_pause();
                        } else if (_XABORT_CODE(status) > USER_OPTION_LOWER_BOUND) {
                            user_explicitly_aborted = true;
                            _stats.tx_aborts_per_reason[TX_ABORT_LOCK_TAKEN]++;
                            err_status = _XABORT_CODE(status);
                            return;
                        } else if(!(status & _XABORT_RETRY)) {
                            _stats.tx_aborts_per_reason[TX_ABORT_REST]++;
                            // if the system recommends not to retry
                            // go to the fallback immediately
                            goto fallback_lock; 
                        } else {
                            _stats.tx_aborts_per_reason[TX_ABORT_REST]++;
                        }   
                    }
                    
                    
                    // too many retries, take the fall-back lock 
                    if (!nretries) break;

            }   //end
    fallback_lock:
                _stats.tx_lacqs++;
                has_locked = true;
                spin_lock_.lock();
            }
            
        }
        // abort_to_retry: aborts current transaction
        // and returns retries left
        // in order to retry transaction.
        // Takes the error code as a template
        // parameter.
        template <unsigned char imm>
        int abort_to_retry() {
            static_assert(imm > USER_OPTION_LOWER_BOUND, 
            "User aborts should be larger than USER_OPTION_LOWER_BOUND, as lower numbers are reserved");
            _xabort(imm);
            user_explicitly_aborted = true;
            return max_retries - nretries;
        }

        // abort: aborts current transaction.
        // Takes the error code as a template
        // parameter.
        template <unsigned char imm>
        static void abort() {
            static_assert(imm > USER_OPTION_LOWER_BOUND, 
            "User aborts should be larger than USER_OPTION_LOWER_BOUND, as lower numbers are reserved");
            _xabort(imm);
        }

        ~TSXGuardWithStats() {
            if (!user_explicitly_aborted && !disabled) {
                
                // no abort code
                if (has_locked && spin_lock_.isLocked()) {
                    spin_lock_.unlock();
                } else {
                    _stats.tx_commits++; 
                    _xend();
                }
            }
            
        }     
        
    };

    
    class TSXTransOnlyGuard {
    private:
        int& retries_;  // how many retries before lock acquire
        SpinLock &spin_lock_;    // fallback
        bool user_explicitly_aborted;   // explicit user aborts mean that lock is not taken and
                                        // transaction not pending
        bool validation_failure;
        TSXStats &_stats;
        bool disabled;

    public:
        TSXTransOnlyGuard(int& retries, SpinLock &mutex, unsigned char &err_status, TSXStats &stats, bool disabled = false, RETRY_STRATEGY strat = STUBBORN):
        retries_(retries),
        spin_lock_(mutex),
        user_explicitly_aborted(false),
        validation_failure(false),
        _stats(stats),
        disabled(disabled)
        {
            

            if (!disabled) {
                while(retries_) {

                    retries_--;
                    
                    // try to init transaction
                    unsigned int status = _xbegin();
                    if (status == _XBEGIN_STARTED) {   // tx started
                        _stats.tx_starts++;
                        if (!spin_lock_.isLocked()) return;  //successfully started transaction
                        
                        // started txn but someone is executing the txn  section non-speculatively 
                        // (acquired the  fall-back lock) -> aborting

                        _xabort(ABORT_GL_TAKEN); // abort with code 0xff  
                    } else if (status & _XABORT_CAPACITY) {
                        _stats.tx_aborts++;
                        _stats.tx_aborts_per_reason[TX_ABORT_CAPACITY]++;
                    } else if (status & _XABORT_CONFLICT) {
                        _stats.tx_aborts++;
                        _stats.tx_aborts_per_reason[TX_ABORT_CONFLICT]++;

                        if (strat == HALF) {
                            retries_ >>= 1;
                        }
                    } else if (status & _XABORT_EXPLICIT) {
                        _stats.tx_aborts++;
                        _stats.tx_aborts_per_reason[TX_ABORT_EXPLICIT]++;
                        if (_XABORT_CODE(status) == ABORT_GL_TAKEN && !(status & _XABORT_NESTED)) {
                            _stats.tx_aborts_per_reason[TX_ABORT_LOCK_TAKEN]++;
                            while (spin_lock_.isLocked()) _mm_pause();
                        } else if (_XABORT_CODE(status) > USER_OPTION_LOWER_BOUND) {
                            user_explicitly_aborted = true;
                            err_status = _XABORT_CODE(status);
                            return;
                        } else if(!(status & _XABORT_RETRY)) {
                            _stats.tx_aborts_per_reason[TX_ABORT_REST]++;
                            // if the system recommends not to retry
                            // go to the fallback immediately
                            goto HARD_ABORT; 
                        } else {
                            _stats.tx_aborts_per_reason[TX_ABORT_REST]++;
                        }   
                    }
                    
                    
                    // too many retries_, take the fall-back lock 
                    if (!retries_) break;

            }   //end
    HARD_ABORT:
                // after too many aborts
                // just restart from scratch
                // by using the transaction
                //std::cout << "should lock" << std::endl;
                retries_ = 0;
                validation_failure = true;
                err_status = ABORT_VALIDATION_FAILURE;
                return;
            }
            
        }


        // abort: aborts current transaction.
        // Takes the error code as a template
        // parameter.
        template <unsigned char imm>
        static void abort() {
            static_assert(imm > USER_OPTION_LOWER_BOUND, 
            "User aborts should be larger than USER_OPTION_LOWER_BOUND, as lower numbers are reserved");
            _xabort(imm);
        }

        ~TSXTransOnlyGuard() {
            if (!user_explicitly_aborted && !disabled && !validation_failure) {
                    _stats.tx_commits++; 
                    _xend();
                }
            }   
        
    };



};
   
#endif
