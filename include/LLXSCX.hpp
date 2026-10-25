#ifndef INCLUDE_LLX_SCX_HPP
    #define INCLUDE_LLX_SCX_HPP

#include <atomic>
#include <vector>
#include <utility>
#include <cstddef>
#include <new>
#include "TSXGuard.hpp"

/*
    LLX/SCX: the load-link extended / store-conditional extended primitives of
    T. Brown, F. Ellen and E. Ruppert, "Pragmatic Primitives for Non-blocking
    Data Structures" (PODC 2013), implemented with CAS.

    - LLX(r) returns a snapshot of the mutable fields of the data record r.
    - SCX(V, R, fld, new) writes new to the field fld of a record of V and
      finalizes the records of R, only if no record of V has changed since
      the LLX this thread performed on it. Otherwise nothing is changed.
    - A finalized record can not be changed any more, an LLX on it returns
      FINALIZED. Records which are removed from a data structure are
      finalized, so that operations based on them fail.

    An SCX freezes the records of V by pointing their info field to its
    SCX-record. Threads which find a frozen record help the SCX complete,
    which makes the primitives lock-free. The exception is an SCX which
    changes nullptr to a node, only its owner completes it (see scx). SafeTree
    only does this for the root pointer, when inserting into an empty structure.

    SCX-records stay reachable from the info fields of the records they froze,
    so they are not freed one by one. They are allocated from an SCXArena which
    is freed as a whole when no operations are running.
*/

namespace LLXSCX {

    enum SCXState {
        IN_PROGRESS,
        COMMITTED,
        ABORTED
    };

    enum LLXResult {
        LLX_SNAPSHOT,   // the fields were read
        LLX_FAIL,       // the record was being changed, try again
        LLX_FINALIZED   // the record has been finalized
    };

    struct SCXRecordBase {
        // help the SCX to complete, returns true if it committed.
        // Called by threads which did not start the SCX
        typedef bool (*HelpFunction)(SCXRecordBase*);

        std::atomic<int> state;
        std::atomic<bool> all_frozen;
        const HelpFunction help;

        constexpr SCXRecordBase(int initial_state, HelpFunction help_function):
        state(initial_state), all_frozen(false), help(help_function) {}
    };

    // the info of records which were never frozen
    inline SCXRecordBase* dummy_scx_record() {
        static SCXRecordBase dummy(ABORTED, nullptr);
        return &dummy;
    }


    class DataRecord;

    template <class Field>
    struct SCXRecord;

    template <class Field>
    bool help_scx(SCXRecord<Field>* scx, bool is_owner);

    template <class ReadFields>
    LLXResult llx(DataRecord* r, SCXRecordBase*& info, ReadFields read_fields);


    // DataRecord: the fields LLX and SCX need in a data record.
    // A copy of a record is a new, unfrozen record.
    class DataRecord {
        template <class Field>
        friend bool help_scx(SCXRecord<Field>* scx, bool is_owner);

        template <class ReadFields>
        friend LLXResult llx(DataRecord* r, SCXRecordBase*& info, ReadFields read_fields);

        private:
            std::atomic<SCXRecordBase*> info_;
            std::atomic<bool> marked_;

        public:
            DataRecord(): info_(dummy_scx_record()), marked_(false) {}
            DataRecord(const DataRecord&): DataRecord() {}

            DataRecord& operator=(const DataRecord&) {
                return *this;
            }

            // reset: make the record unfrozen and unfinalized,
            // only when no operations are running
            void reset() {
                info_.store(dummy_scx_record());
                marked_.store(false);
            }
    };


    // a record of V and the info read from it by the LLX linked to the SCX
    struct LinkedLLX {
        DataRecord* record;
        SCXRecordBase* info;
    };


    template <class Field>
    struct SCXRecord: public SCXRecordBase {
        Field* fld;
        Field old_value;
        Field new_value;

        // only the thread which started the SCX may
        // change fld, see scx
        bool owner_only_update;

        LinkedLLX* v;
        int n_v;

        DataRecord** r;
        int n_r;

        static bool help_as_helper(SCXRecordBase* scx) {
            return help_scx(static_cast<SCXRecord<Field>*>(scx), false);
        }

        SCXRecord(): SCXRecordBase(IN_PROGRESS, &help_as_helper) {}
    };


    // help_scx: the Help procedure, freeze the records of V,
    // finalize the records of R and change fld.
    // Returns true if the SCX committed
    template <class Field>
    bool help_scx(SCXRecord<Field>* scx, bool is_owner) {
        if (scx->owner_only_update && !is_owner) {
            // the owner will complete it
            return false;
        }

        // freeze all records of V
        for (int i = 0; i < scx->n_v; i++) {
            DataRecord* r = scx->v[i].record;
            SCXRecordBase* expected = scx->v[i].info;

            if (!r->info_.compare_exchange_strong(expected, scx)) {
                if (r->info_.load() != scx) {
                    // r is frozen for another SCX or has changed
                    if (scx->all_frozen.load()) {
                        // the SCX already completed
                        return true;
                    }

                    scx->state.store(ABORTED, std::memory_order_release);
                    return false;
                }
            }
        }

        // stores are ordered by the freezing CASes and the update CAS,
        // they are read after the state with acquire loads
        scx->all_frozen.store(true, std::memory_order_release);

        for (int i = 0; i < scx->n_r; i++) {
            scx->r[i]->marked_.store(true, std::memory_order_release);
        }

        Field expected = scx->old_value;
        __atomic_compare_exchange_n(scx->fld, &expected, scx->new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);

        scx->state.store(COMMITTED, std::memory_order_release);
        return true;
    }


    // llx: LLX on the record r. read_fields reads its mutable fields,
    // they are a snapshot if LLX_SNAPSHOT is returned, info is then
    // set to the info to be passed to the linked SCX.
    template <class ReadFields>
    LLXResult llx(DataRecord* r, SCXRecordBase*& info, ReadFields read_fields) {
        const bool marked1 = r->marked_.load();
        SCXRecordBase* rinfo = r->info_.load();
        const int state = rinfo->state.load();
        const bool marked2 = r->marked_.load();

        if (state == ABORTED || (state == COMMITTED && !marked2)) {
            // r is not frozen
            read_fields();

            // the fields are read before info is checked again
            std::atomic_thread_fence(std::memory_order_acquire);

            if (r->info_.load() == rinfo) {
                info = rinfo;
                return LLX_SNAPSHOT;
            }
        }

        if (marked1) {
            const int rinfo_state = rinfo->state.load();
            if (rinfo_state == COMMITTED || (rinfo_state == IN_PROGRESS && rinfo->help(rinfo))) {
                return LLX_FINALIZED;
            }
        }

        SCXRecordBase* current_info = r->info_.load();
        if (current_info->state.load() == IN_PROGRESS) {
            current_info->help(current_info);
        }

        return LLX_FAIL;
    }


    // SCXArena: bump allocator for SCX-records, one per thread.
    // release_all frees the memory of all threads.
    template <class Tag>
    class SCXArena {
        private:
            static constexpr std::size_t CHUNK_SIZE = 1 << 20;

            char* chunk_;
            std::size_t used_;
            std::size_t size_;
            unsigned generation_;

            static TSX::SpinLock lock_;
            static std::vector<char*> chunks_;
            static std::atomic<unsigned> global_generation_;

            void new_chunk(std::size_t min_size) {
                size_ = min_size > CHUNK_SIZE ? min_size : CHUNK_SIZE;

                lock_.lock();
                chunk_ = static_cast<char*>(::operator new(size_));
                chunks_.push_back(chunk_);
                generation_ = global_generation_.load();
                lock_.unlock();

                used_ = 0;
            }

        public:
            SCXArena(): chunk_(nullptr), used_(0), size_(0), generation_(0) {}

            SCXArena(const SCXArena&) = delete;
            SCXArena& operator=(const SCXArena&) = delete;

            void* allocate(std::size_t size, std::size_t alignment) {
                std::size_t start = (used_ + alignment - 1) & ~(alignment - 1);

                // chunks freed by release_all are not reused
                if (!chunk_ || generation_ != global_generation_.load() || start + size > size_) {
                    new_chunk(size + alignment);
                    start = 0;
                }

                used_ = start + size;
                return chunk_ + start;
            }

            template <class T>
            T* allocate_array(int n) {
                return static_cast<T*>(allocate(sizeof(T) * (n > 0 ? n : 1), alignof(T)));
            }

            // release_all: free the SCX-records of all threads,
            // only when no operations are running
            static void release_all() {
                lock_.lock();
                for (auto chunk: chunks_) {
                    ::operator delete(chunk);
                }
                chunks_.clear();
                ++global_generation_;
                lock_.unlock();
            }
    };

    template <class Tag>
    constexpr std::size_t SCXArena<Tag>::CHUNK_SIZE;

    template <class Tag>
    TSX::SpinLock SCXArena<Tag>::lock_;

    template <class Tag>
    std::vector<char*> SCXArena<Tag>::chunks_;

    template <class Tag>
    std::atomic<unsigned> SCXArena<Tag>::global_generation_(0);


    // scx: SCX(V, R, fld, new_value). V must be sorted by record and
    // contain the record of fld, old_value is the value of fld read
    // by the LLX on that record. Returns true if fld was changed.
    template <class Field, class Arena>
    bool scx(Arena& arena, const LinkedLLX* v, int n_v, DataRecord* const* r, int n_r,
             Field* fld, Field old_value, Field new_value) {
        auto record = new(arena.allocate(sizeof(SCXRecord<Field>), alignof(SCXRecord<Field>))) SCXRecord<Field>();

        record->fld = fld;
        record->old_value = old_value;
        record->new_value = new_value;

        // a helper which is late could repeat the update after fld
        // has been changed back to old_value (ABA). Values written to
        // fld are new nodes, apart from nullptr, so in that case
        // only the owner changes fld
        record->owner_only_update = old_value == nullptr && new_value != nullptr;

        record->v = arena.template allocate_array<LinkedLLX>(n_v);
        record->n_v = n_v;
        for (int i = 0; i < n_v; i++) {
            record->v[i] = v[i];
        }

        record->r = arena.template allocate_array<DataRecord*>(n_r);
        record->n_r = n_r;
        for (int i = 0; i < n_r; i++) {
            record->r[i] = r[i];
        }

        return help_scx(record, true);
    }


    // RootRecords: data records for the root pointers of data structures,
    // which are not fields of a node. The record of a root pointer is
    // found by its address.
    template <class Field>
    class RootRecords {
        private:
            static TSX::SpinLock lock_;
            static std::vector<std::pair<Field*, DataRecord*>> records_;

            thread_local static Field* cached_root_;
            thread_local static DataRecord* cached_record_;

        public:
            static DataRecord* get(Field* root) {
                if (cached_root_ != root) {
                    DataRecord* record = nullptr;

                    lock_.lock();
                    for (auto& entry: records_) {
                        if (entry.first == root) {
                            record = entry.second;
                        }
                    }

                    if (!record) {
                        record = new DataRecord();
                        records_.push_back(std::make_pair(root, record));
                    }
                    lock_.unlock();

                    cached_root_ = root;
                    cached_record_ = record;
                }

                return cached_record_;
            }

            // reset_all: reset the records after their
            // SCX-records have been freed
            static void reset_all() {
                lock_.lock();
                for (auto& entry: records_) {
                    entry.second->reset();
                }
                lock_.unlock();
            }
    };

    template <class Field>
    TSX::SpinLock RootRecords<Field>::lock_;

    template <class Field>
    std::vector<std::pair<Field*, DataRecord*>> RootRecords<Field>::records_;

    template <class Field>
    thread_local Field* RootRecords<Field>::cached_root_ = nullptr;

    template <class Field>
    thread_local DataRecord* RootRecords<Field>::cached_record_ = nullptr;
}

#endif
