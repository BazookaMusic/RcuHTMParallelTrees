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

                    if (!spin_lock_.exchange(LOCKED, std::memory_order_acquire)) {
                        break;
                    }

                }
            }

            void unlock() noexcept{
                spin_lock_.store(false, std::memory_order_release);
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
       
        const int max_retries_;  // how many retries before lock acquire
        SpinLock &spin_lock_;    // fallback
        bool has_locked_;        // avoid checking global lock if haven't locked
        bool user_explicitly_aborted_;   // explicit user aborts mean that lock is not taken and
                                        // transaction not pending
        int nretries_;           // how many retries have been made so far
                                // used to resume transaction in case of user abort
        bool disabled_;
    public:
        TSXGuard(const int max_tx_retries, SpinLock &mutex, unsigned char &err_status,  bool disabled = false, RETRY_STRATEGY strat = STUBBORN): 
        max_retries_(max_tx_retries),
        spin_lock_(mutex),
        has_locked_(false),
        user_explicitly_aborted_(false),
        nretries_(max_tx_retries),
        disabled_(disabled)
        {
            if (!disabled_) {
                while(1) {
                    --nretries_;

                    // try to init transaction
                    unsigned int status = _xbegin();
                    if (status == _XBEGIN_STARTED) {      // tx started
                        if (!spin_lock_.isLocked()) return; //successfully started transaction
                        // started txn but someone is executing the txn  section non-speculatively
                        // (acquired the  fall-back lock) -> aborting
                        _xabort(ABORT_GL_TAKEN); // abort with code 0xff
                    } else if (strat == HALF && (status & _XABORT_CONFLICT))  {
                        nretries_ >>= 1; // half strategy
                    } else if (status & _XABORT_EXPLICIT) {
                        if (_XABORT_CODE(status) == ABORT_GL_TAKEN && !(status & _XABORT_NESTED)) {
                            while (spin_lock_.isLocked()) _mm_pause();
                        } else if (_XABORT_CODE(status) > USER_OPTION_LOWER_BOUND) {
                            user_explicitly_aborted_ = true;
                            err_status = _XABORT_CODE(status);
                            return;
                        } else if(!(status & _XABORT_RETRY)) {
                            // if the system recommends not to retry
                            // go to the fallback immediately
                            goto fallback_lock; 
                        } 
                    } 

                    // too many retries, take the fall-back lock 
                    if (!nretries_) break;

                }   //end
        fallback_lock:
                    has_locked_ = true;
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
            user_explicitly_aborted_ = true;
            return max_retries_ - nretries_;
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
            if (!user_explicitly_aborted_ && !disabled_) {
                // no abort code
                if (has_locked_ && spin_lock_.isLocked()) {
                    spin_lock_.unlock();
                } else {
                    _xend();
                }
            }
            
        }     
    };


    class TSXGuardWithStats {
    private:
        const int max_retries_;  // how many retries before lock acquire
        SpinLock &spin_lock_;    // fallback
        bool has_locked_;        // avoid checking global lock if haven't locked
        bool user_explicitly_aborted_;   // explicit user aborts mean that lock is not taken and
                                        // transaction not pending
        int nretries_;           // how many retries have been made so far
                                // used to resume transaction in case of user abort
        TSXStats &stats_;
        bool disabled_;

    public:
        TSXGuardWithStats(const int max_tx_retries, SpinLock &mutex, unsigned char &err_status, TSXStats &stats, bool disabled = false, RETRY_STRATEGY strat = STUBBORN):
        max_retries_(max_tx_retries),
        spin_lock_(mutex),
        has_locked_(false),
        user_explicitly_aborted_(false),
        nretries_(max_tx_retries),
        stats_(stats),
        disabled_(disabled)
        {
            

            if (!disabled_) {
                while(true) {

                    --nretries_;
                    
                    // try to init transaction
                    unsigned int status = _xbegin();
                    if (status == _XBEGIN_STARTED) {   // tx started
                        stats_.tx_starts++;
                        if (!spin_lock_.isLocked()) return;  //successfully started transaction
                        
                        // started txn but someone is executing the txn  section non-speculatively 
                        // (acquired the  fall-back lock) -> aborting

                        _xabort(ABORT_GL_TAKEN); // abort with code 0xff  
                    } else if (status & _XABORT_CAPACITY) {
                        stats_.tx_aborts++;
                        stats_.tx_aborts_per_reason[TX_ABORT_CAPACITY]++;
                    } else if (status & _XABORT_CONFLICT) {
                        stats_.tx_aborts++;
                        stats_.tx_aborts_per_reason[TX_ABORT_CONFLICT]++;

                        if (strat == HALF) {
                            nretries_ >>= 1;
                        }
                    } else if (status & _XABORT_EXPLICIT) {
                        stats_.tx_aborts++;
                        stats_.tx_aborts_per_reason[TX_ABORT_EXPLICIT]++;
                        if (_XABORT_CODE(status) == ABORT_GL_TAKEN && !(status & _XABORT_NESTED)) {
                            stats_.tx_aborts_per_reason[TX_ABORT_LOCK_TAKEN]++;
                            while (spin_lock_.isLocked()) _mm_pause();
                        } else if (_XABORT_CODE(status) > USER_OPTION_LOWER_BOUND) {
                            user_explicitly_aborted_ = true;
                            stats_.tx_aborts_per_reason[TX_ABORT_LOCK_TAKEN]++;
                            err_status = _XABORT_CODE(status);
                            return;
                        } else if(!(status & _XABORT_RETRY)) {
                            stats_.tx_aborts_per_reason[TX_ABORT_REST]++;
                            // if the system recommends not to retry
                            // go to the fallback immediately
                            goto fallback_lock; 
                        } else {
                            stats_.tx_aborts_per_reason[TX_ABORT_REST]++;
                        }   
                    }
                    
                    
                    // too many retries, take the fall-back lock 
                    if (!nretries_) break;

            }   //end
    fallback_lock:
                stats_.tx_lacqs++;
                has_locked_ = true;
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
            user_explicitly_aborted_ = true;
            return max_retries_ - nretries_;
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
            if (!user_explicitly_aborted_ && !disabled_) {
                
                // no abort code
                if (has_locked_ && spin_lock_.isLocked()) {
                    spin_lock_.unlock();
                } else {
                    stats_.tx_commits++; 
                    _xend();
                }
            }
            
        }     
        
    };

    
    class TSXTransOnlyGuard {
    private:
        int& retries_;  // how many retries before lock acquire
        SpinLock &spin_lock_;    // fallback
        bool user_explicitly_aborted_;   // explicit user aborts mean that lock is not taken and
                                        // transaction not pending
        bool validation_failure_;
        TSXStats &stats_;
        bool disabled_;

    public:
        TSXTransOnlyGuard(int& retries, SpinLock &mutex, unsigned char &err_status, TSXStats &stats, bool disabled = false, RETRY_STRATEGY strat = STUBBORN):
        retries_(retries),
        spin_lock_(mutex),
        user_explicitly_aborted_(false),
        validation_failure_(false),
        stats_(stats),
        disabled_(disabled)
        {
            

            if (!disabled_) {
                while(retries_) {

                    retries_--;
                    
                    // try to init transaction
                    unsigned int status = _xbegin();
                    if (status == _XBEGIN_STARTED) {   // tx started
                        stats_.tx_starts++;
                        if (!spin_lock_.isLocked()) return;  //successfully started transaction
                        
                        // started txn but someone is executing the txn  section non-speculatively 
                        // (acquired the  fall-back lock) -> aborting

                        _xabort(ABORT_GL_TAKEN); // abort with code 0xff  
                    } else if (status & _XABORT_CAPACITY) {
                        stats_.tx_aborts++;
                        stats_.tx_aborts_per_reason[TX_ABORT_CAPACITY]++;
                    } else if (status & _XABORT_CONFLICT) {
                        stats_.tx_aborts++;
                        stats_.tx_aborts_per_reason[TX_ABORT_CONFLICT]++;

                        if (strat == HALF) {
                            retries_ >>= 1;
                        }
                    } else if (status & _XABORT_EXPLICIT) {
                        stats_.tx_aborts++;
                        stats_.tx_aborts_per_reason[TX_ABORT_EXPLICIT]++;
                        if (_XABORT_CODE(status) == ABORT_GL_TAKEN && !(status & _XABORT_NESTED)) {
                            stats_.tx_aborts_per_reason[TX_ABORT_LOCK_TAKEN]++;
                            while (spin_lock_.isLocked()) _mm_pause();
                        } else if (_XABORT_CODE(status) > USER_OPTION_LOWER_BOUND) {
                            user_explicitly_aborted_ = true;
                            err_status = _XABORT_CODE(status);
                            return;
                        } else if(!(status & _XABORT_RETRY)) {
                            stats_.tx_aborts_per_reason[TX_ABORT_REST]++;
                            // if the system recommends not to retry
                            // go to the fallback immediately
                            goto HARD_ABORT; 
                        } else {
                            stats_.tx_aborts_per_reason[TX_ABORT_REST]++;
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
                validation_failure_ = true;
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
            if (!user_explicitly_aborted_ && !disabled_ && !validation_failure_) {
                    stats_.tx_commits++; 
                    _xend();
                }
            }   
        
    };

    thread_local SpinLock __internal__global_lock;
    thread_local TSXStats __internal__trans_stats;


    // Handles the fallback and retries
    // when using a TransOnlyGuard
    class Transaction {
        private:
            int& retries_;
            TSX::SpinLock &lock_;
            TSX::TSXStats &stats_;
            bool has_locked_;

        public:
            Transaction(int &retries): 
            retries_(retries), 
            lock_(__internal__global_lock), 
            stats_(__internal__trans_stats),
            has_locked_(false) {
                if (retries == 0) {
                    lock_.lock();
                    has_locked_ = true;
                }

            }

            bool go_to_fallback() {
                return retries_ == 0;
            }


            TSX::SpinLock& get_lock() {
                return lock_;
            }

            bool has_locked() {
                return has_locked_;
            }

            TSX::TSXStats& get_stats() {
                return stats_;
            }

            int& get_retries() {
                return retries_;
            }

            ~Transaction() {
                if (has_locked_) {
                    lock_.unlock();
                }
            }
    };

    thread_local Transaction* __internal__trans_pointer = nullptr;
};


// Macros to reduce boilerplate to make a transactional operation
// Syntax is of the form
    //
    //  TM_SAFE_OPERATION_START {
    //      code...
    //  } TM_SAFE_OPERATION_END 
    //

    // if a transaction succeeded or failed
    thread_local bool __internal__thread_transaction_success_flag__ = false;

    // thread safe operation macro definitions
    #ifdef TM_EARLY_ABORT
        #define TM_SAFE_OPERATION_START(n_retries) \
        __internal__thread_transaction_success_flag__ = false;\
        int __current__op__retries = n_retries; \
        while(!__internal__thread_transaction_success_flag__) { \
            TSX::Transaction __trans_obj__(__current__op__retries);
            TSX::__internal__trans_pointer = &__trans_obj__;
        try {\

        #define TM_SAFE_OPERATION_END \
        }catch (const ValidationAbortException& e) {\
            __internal__thread_transaction_success_flag__ = false;\
        }\
        }

    #else
        #define TM_SAFE_OPERATION_START(n_retries) \
        __internal__thread_transaction_success_flag__ = false;\
        int __current__op__retries = n_retries; \
        while(!__internal__thread_transaction_success_flag__) {\
            TSX::Transaction __trans_obj(__current__op__retries);\
            TSX::__internal__trans_pointer = &__trans_obj;


        #define TM_SAFE_OPERATION_END }
    #endif

#endif
