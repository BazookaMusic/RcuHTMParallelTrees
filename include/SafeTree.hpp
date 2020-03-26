#ifndef SAFE_TREE_HPP
    #define SAFE_TREE_HPP
    
    


    // configuration options

    //-------------------------------------------//

    // self explanatory
    #ifndef RCU_HTM_MAX_THREADS
        #define RCU_HTM_MAX_THREADS 100
    #endif

    // search tree allows the creation
    // of search trees as defined by the
    // methods of the user node given
    // with optimized conflict detection
    // and a search functions provided
    // automatically
    #define SEARCH_TREE 0
    // general tree allows the creation
    // of any kind of tree structure
    // but with an unoptimized
    // conflict detection mechanism
    #define GENERAL_TREE 1

    // Set desired tree type
    #ifndef TREE_TYPE
        #define TREE_TYPE SEARCH_TREE
    #endif



    // memory pools for SafeNodes
    #define TSX_MEM_POOL

    // memory pools for user defined nodes
    #define USER_MEM_POOL

    #define PREALLOC_VALIDATION_SET

    #define PATH_MAX_LEN 10000

    // all internal vectors and arrays are
    // static (decreases performance in my tests)
    //#define STATIC_VECTORS_AND_ARRAYS

    
    // Abort before htm transaction
    // if conflict found when moving
    // tree root upwards

    //#define TM_EARLY_ABORT

    // abort early even during
    // copying if conflict found

    //#define TM_EARLY_ABORT_ON_COPY

    //-------------------------------------------//

    


#include <memory>
#include <tuple>
#include <iostream>
#include <deque>
#include <array>
#include <tuple>
#include <new>
#include <cassert>
#include <algorithm>



#include "helper_data_structures.hpp"
#include "TSXGuard.hpp"

namespace SafeTree {
        static constexpr int MAX_THREADS = RCU_HTM_MAX_THREADS;

    /*  SafeTrees require Nodes with the following methods for the general tree version
        1.     A copy constructor, copy assignment and destructor (RULE OF THREE)
        2.     NodeType* getChild(int i)
        3.     void setChild(int i, NodeType * n)
        4.     NodeType** getChildPointer(int i): get a pointer to the stored tree pointer
                (where the i'th child is stored)
        5.     static constexpr int maxChildren(): max amount of children for each node, 
        The optimized search tree version also requires:
        6.     A using KeyType = (type of key used) declaration
        7.     A bool hasKey(KeyType key) method to return if the node contains a key
        8.     bool traversalDone(KeyType key): returns true if the node with key is the caller, can also
                consider other factors like if it is a terminal node
        9.     int nextChild(KeyType target_key): return index of next child when looking for node with target_key
        10.     int nextChild(NodeType* target): return index of next child when looking for target node
    */

    // internal use
    enum INSERT_POSITIONS {AT_ROOT = -1, UNDEFINED = -2};



    #if TREE_TYPE == GENERAL_TREE
        template <class NodeType>
        struct ConnPointData;

        // used to implement
        // lookup functions for the thread safe operations
        template <class NodeType>
        class PathTracker {
            private:
                // adress of pointer to root of structure
                NodeType** root_;

                NodeType* current_pos_;
                int at_level_;
                
                TreePathStackWithIndex<NodeType, PATH_MAX_LEN> path_;


            public:
                // PathTracker: receives adress of pointer to root of structure. 
                // Tracks a path on the tree to be used for a thread safe operation.
                explicit PathTracker(NodeType** root): root_(root), current_pos_(*root), at_level_(-1) {}

                // getNode: return the node where the
                // tracker is currently pointing
                NodeType* getNode() {
                    return current_pos_;
                }

                // moveToChild: move the tracker to one of
                // the children of the node it's currently pointing to
                NodeType* moveToChild(int pos) {
                    ++at_level_;

                    path_.push(current_pos_, pos);
                    current_pos_ = current_pos_->getChild(pos);

                    return current_pos_;
                }

                // moveUp: move the tracker up
                // n levels on the path it has followed
                void moveUp(int n_levels = 1) {
                    at_level_ = at_level_ >= n_levels ? at_level_ - n_levels: -1;

                    for (int i = 0; at_level_ >= 0 && i < n_levels - 1; i++) {
                        path_.pop();
                    }

                    if (!path_.Empty()) {
                        current_pos_ =  path_.pop().node;
                    }   
                }

                // set previous node in path followed
                // as connection point. Return ConnPointData
                // object which allows for a tree
                // of copies to be constructed.
                ConnPointData<NodeType> connectHere() {

                    //path_check();

                    ConnPointData<NodeType> conn_point_snapshot;

                    //path_.print_contents();

                    bool at_root = at_level_ == -1;

                    NodeAndNextPointer<NodeType> conn_point_and_next_child = path_.pop();
                    conn_point_snapshot.connection_point_ = at_root ? nullptr: conn_point_and_next_child.node;

                    path_.move_to(conn_point_snapshot.path);

                    conn_point_snapshot.root_of_structure = root_;

                    conn_point_snapshot.con_ptr.child_index = at_root? INSERT_POSITIONS::AT_ROOT : conn_point_and_next_child.next_child;
                    conn_point_snapshot.con_ptr.snapshot = current_pos_;

                    // restore state
                    path_.push(conn_point_and_next_child.node, conn_point_and_next_child.next_child);


                    return conn_point_snapshot;
                }
        };
    #endif


    static constexpr unsigned char VALIDATION_FAILED = TSX::ABORT_VALIDATION_FAILURE;

    #ifdef TM_EARLY_ABORT
        class ValidationAbortException: std::exception{};   
    #endif

    template <class NodeType>
    class ConnPoint;

    template <class NodeType>
    class SafeNode
    {
        enum SAFENODE_TYPE {
            ORIG_TREE_NODE,
            ORIG_TREE_NO_VALIDATION,
            NEW_NODE
        };

        friend class ConnPoint<NodeType>;
        private:
            ConnPoint<NodeType>& conn_point_;
            NodeType* const original_;
            NodeType* copy_;
            std::array<SafeNode<NodeType>*,NodeType::maxChildren()> children_;
            std::array<bool,NodeType::maxChildren()> modified_;
            std::array<NodeType*, NodeType::maxChildren()> children_pointers_snapshot_;

            bool deleted_;

            SAFENODE_TYPE node_type_;


            // mark previous for deletion and move new value
            // returns old value
            static SafeNode<NodeType>* move(SafeNode<NodeType>*& to_be_replaced, SafeNode<NodeType>* replacement) {

                auto temp = to_be_replaced;  

                // replace value
                to_be_replaced = replacement;

                return temp;
            }

            
            static void sub_tree_delete(SafeNode<NodeType>* root) {
                if (!root) {
                    return;
                }

                for (int i = 0; i < NodeType::maxChildren(); i++)
                {
                    sub_tree_delete(root->children_[i]);
                }
                
                root->deleted_ = true;
            }

            void make_copy() {
                assert(original_ == copy_);

                #ifdef USER_MEM_POOL
                    copy_ = ConnPoint<NodeType>::create_new_node(*original_);
                #else
                    copy_ = new NodeType(*original_);
                #endif


                for (int i = 0; i < NodeType::maxChildren(); i++) {
                    #ifdef TM_EARLY_ABORT_ON_COPY
                        if (children_pointers_snapshot_[i] != original_->getChild(i)) {
                            conn_point_.validation_abort();
                            throw ValidationAbortException();
                        }
                    #endif

                    copy_->setChild(i,children_pointers_snapshot_[i]);
                }
            }

            NodeType* node_to_be_connected() {
                return copy_;
            }

            void cleanup() {
                if (node_type_ == ORIG_TREE_NODE) {
                    if (deleted_ && original_ != copy_) {
                        delete copy_;
                    }

                    if (conn_point_.copyWasConnected() && original_ != copy_) {
                        delete original_;
                    }
                } else {
                    if (!conn_point_.copyWasConnected()) {
                        delete copy_;
                    }
                }
            }

            
        
        public:
            SafeNode(const SafeNode& other) = delete;
            SafeNode& operator=(const SafeNode&) = delete;
            SafeNode(){}

            #ifndef USER_NODE_POOL
            // ~SafeNode() {
            //     if (node_from_orig_tree) {
            //         if (deleted_ && original_ != copy_) {
            //             delete copy_;
            //         }

            //         if (conn_point_.copyWasConnected()) {
            //             delete original_;
            //         }
            //     }  
            // }
            #endif
            SafeNode(ConnPoint<NodeType>& conn_point, NodeType* original, SAFENODE_TYPE node_type = ORIG_TREE_NODE): 
            conn_point_(conn_point), 
            original_(original), 
            copy_(original),
            deleted_(false),
            node_type_(node_type)
            {

                // all nodes will be added to validation set
                // as it is used for memory management as well
                conn_point_.add_to_validation_set(this);

                if (node_type_ == ORIG_TREE_NODE) {
                    for (int i = 0; i < NodeType::maxChildren(); i++) {
                        // keep backup of the original_ child pointers
                        const auto original_child = original_->getChild(i);
                        children_pointers_snapshot_[i] = original_child;
                    }
                }

                children_.fill(nullptr);
                modified_.fill(node_type_ == NEW_NODE);
            }


            static constexpr int children_length() {
                return NodeType::maxChildren();
            }

            // peekOriginal: used to access original_
            // node (any modifications are not guaranteed thread safe)
            const NodeType* peekOriginal() const {
                return original_;
            }

            // rwRef: get reference to safe copy of node,
            // can be modified freely
            // as only one thread has access to it
            NodeType* rwRef() {
                if (node_type_ == ORIG_TREE_NODE && copy_ == original_) {
                    make_copy();
                }
                
                return copy_;
            }


            NodeType* peekChild(const int child_pos) {
                return  copy_ == original_? children_pointers_snapshot_[child_pos]: copy_->getChild(child_pos);
            }


            //getChild: returns either an already linked SafeNode
            //or creates a new SafeNode when accessing an unsafe node
            SafeNode<NodeType>* getChild(const int child_pos) {
                /* process:
                A SafeNode without set children will have nullptr in
                its array. When getting the child of a SafeNode:
                if:
                    1. has been set) return it
                    2. has not been set) create SafeNode, assign to children and return it 

                */
                // no oob errors
                assert(child_pos >= 0 && child_pos < NodeType::maxChildren());
                assert(node_type_ != ORIG_TREE_NO_VALIDATION && "Validation Error: Tried to access a child of a node with disabled validation");

                // force parent copy
                // getting a child
                // explicitly
                // means that you
                // seek to modify it
                if (copy_ == original_ && node_type_ == ORIG_TREE_NODE) {
                    make_copy();
                }
                

                
                //child has already been copied
                if (modified_[child_pos]) {
                    return children_[child_pos];
                }

                // child has been modified
                modified_[child_pos] = true;

                SafeNode<NodeType>* new_safe = nullptr;

                // if node has been copied read from copy
                // else read from original child pointers backup
                const auto original_child = copy_->getChild(child_pos);

                //child not copied yet,
                //copy and use the copy from now on
                if (original_child) {
                    new_safe = node_type_ == ORIG_TREE_NODE ? conn_point_.wrap_safe(original_child): conn_point_.create_safe(original_child);
                    children_[child_pos] = new_safe;
                    copy_->setChild(child_pos, new_safe->rwRef());
                }
            

                return new_safe;
            }

            // setChild: sets the child with index child_pos
            // to be the SafeNode newVal.
            // Returns unique pointer to deleted node to
            // be used for manual memory management 
            // (eg. deleting the entire subtree)
            SafeNode<NodeType>* setChild(int child_pos, SafeNode<NodeType>* newVal) {
                // no oob errors
                assert(child_pos >= 0 && child_pos < NodeType::maxChildren());
                assert(node_type_ != ORIG_TREE_NO_VALIDATION && "Validation Error: Tried to access a child of a node with disabled validation");


                // node needs to be modified
                // copy it
                if (copy_ == original_ && node_type_ == ORIG_TREE_NODE) {
                    make_copy();
                }

                // copy has happened
                modified_[child_pos] = true;

                auto child = children_[child_pos];

                if (!child) {
                    children_[child_pos] = newVal;
                    copy_->setChild(child_pos, newVal? newVal->node_to_be_connected() : nullptr);
                    return nullptr;
                }

                // update children of safe node
                // old node is deleted
                SafeNode<NodeType>* old_node = move(children_[child_pos], newVal);
                
                // connect copy pointer, replace with new
                copy_->setChild(child_pos, newVal? newVal->node_to_be_connected() : nullptr);

                return old_node;
            }


            // clipTree: delete entire subtree with
            // the child at position child_pos as root
            void clipTree(int child_pos) {
                assert(child_pos >= 0 && child_pos < NodeType::maxChildren());

                if (children_[child_pos]) {
                    children_[child_pos]->subtreeDelete();
                }

                children_[child_pos] = nullptr;
            }
            
            // subtreeDelete: delete subtree rooted at this node
            void subtreeDelete() {
                sub_tree_delete(this); 
            }


            // make copy of SafeNode
            SafeNode<NodeType>* clone(SafeNode<NodeType>* a_safe_node) {
                if (!a_safe_node) {
                    return nullptr;
                }
                auto new_node = new SafeNode<NodeType>();

                *new_node = *a_safe_node;

                return new_node;
            }
    };


    template <class NodeType>
    struct ConnectionPointer {
        enum {AT_ROOT = -1};
        // the previous value of the pointer to be changed
        NodeType* snapshot;
        // at which index of its parent
        // set to AT_ROOT if connecting to root
        int child_index;

        ConnectionPointer(){}
        ConnectionPointer(NodeType* snapshot, int child_index): snapshot(snapshot),child_index(child_index) {}
    };


    // forward declaration to have it grouped with
    // find_conn_point
    template <class NodeType>
    NodeType* find(NodeType* root, typename NodeType::KeyType desired_key);


    // forward declaration to have it grouped with
    // find_conn_point
    template <class NodeType>
    bool find_target_node(NodeType* root, NodeType* target);

    template <class T>
    struct ConnPointData {
        friend class ConnPoint<T>;
        template <class NodeType>
        friend ConnPointData<NodeType> find_conn_point(typename NodeType::KeyType key, NodeType** root);
        #if TREE_TYPE == GENERAL_TREE
            friend class PathTracker<T>;
        #endif

        private:
            ConnectionPointer<T> con_ptr;    
            T* connection_point_;
            #if TREE_TYPE == SEARCH_TREE
                bool found_;
            #endif
            TreePathStackWithIndex<T,PATH_MAX_LEN> path;
            T** root_of_structure;
        public:
            #if TREE_TYPE == SEARCH_TREE
                bool found() const {
                    return found_;
                }
            #endif

            T* connection_point() const {
                return connection_point_;
            }

            T** root() {
                return root_of_structure;
            }

    };


    #ifdef TSX_MEM_POOL
        // A SafeNode Memory Pool
        

        template<class Object>
        struct memory_pool {
            using buffer_type = typename std::aligned_storage<sizeof(Object), alignof(Object)>::type;

            static_assert(sizeof(buffer_type) >= sizeof(Object), "wrong size");

            explicit memory_pool(std::size_t limit)
                    : objects_(new buffer_type[limit]),limit_(limit), used_(0) {
            }

            memory_pool(const memory_pool&) = delete;
            memory_pool(memory_pool&&) = default;
            memory_pool& operator=(const memory_pool&) = delete;
            memory_pool& operator=(memory_pool&&) = delete;

            template<class...Args>
            Object* create(Args &&...args) {
                if (used_ < limit_) {
                    auto candidate = new(std::addressof(objects_[used_])) Object(std::forward<Args>(args)...);
                    ++ used_;
                    return candidate;
                }
                else {
                    std::cerr << "OOM Buffer used: " << used_ << std::endl;
                    exit(-1);
                    return nullptr;
                }
            }


            buffer_type* reset() {
                used_ = 0;
                return objects_;
            }

            buffer_type* objects_;
            std::size_t limit_;
            std::size_t used_;
        
        };

        template<class Object>
        struct memory_pool_tracked: public memory_pool<Object> {

            using buffer_type = typename std::aligned_storage<sizeof(Object), alignof(Object)>::type;

            
            explicit memory_pool_tracked(std::size_t limit): memory_pool<Object>(limit) {
                pool_lock_.lock();
                ++index_;
                thread_buffers_[index_] = memory_pool<Object>::objects_;
                pool_lock_.unlock();
            }

            void fill_pool() {
                memory_pool<Object>::objects_ = new buffer_type[memory_pool<Object>::limit_];
                pool_lock_.lock();
                ++index_;
                thread_buffers_[index_] = memory_pool<Object>::objects_;
                pool_lock_.unlock();
            }

            
            void hard_reset() {
                for (auto i = 0; i <= index_; i++) {
                    delete [] thread_buffers_[i];
                    thread_buffers_[i] = nullptr;
                }

                memory_pool<Object>::used_ = 0;
                memory_pool<Object>::objects_ = nullptr;

                index_ = -1;

            }

            void set_checkpoint() {
                checkpoint_ = memory_pool<Object>::used_;
            }

            void reset_to_checkpoint() {
                memory_pool<Object>::used_ = checkpoint_;
            }

            std::size_t checkpoint_;

            static TSX::SpinLock pool_lock_;
            static typename memory_pool_tracked<Object>::buffer_type** thread_buffers_;
            static int index_;

        };

        template<class Object>
        TSX::SpinLock memory_pool_tracked<Object>::pool_lock_;

        template<class Object>
        typename memory_pool_tracked<Object>::buffer_type** memory_pool_tracked<Object>::thread_buffers_ = new buffer_type*[MAX_THREADS];

        template<class Object>
        int memory_pool_tracked<Object>::index_ = -1;
    #endif


    template <class NodeType>
    class ConnPoint
    {
        using Stack = TreePathStackWithIndex<NodeType,PATH_MAX_LEN>;
        friend class SafeNode<NodeType>;

        private:
            #ifdef TSX_MEM_POOL
                thread_local static memory_pool<SafeNode<NodeType>> pool_;
            #endif

            #ifdef USER_MEM_POOL
                thread_local static memory_pool_tracked<NodeType> user_node_pool_;
            #endif

            // returns if copy connection was successful
            bool& connect_success_;

            #ifdef PREALLOC_VALIDATION_SET
                thread_local static PreAllocVec<SafeNode<NodeType>*, 500> validation_set_;
            #else
                std::deque<SafeNode<NodeType>*> validation_set_;
            #endif

            // the connection point is the node whose child pointer will
            // be modified to connect the tree of copies
            NodeType* connection_point_;
            // if null, the insertion should be done at
            // the root of the data structure

            // the point of insertion of the new tree
            NodeType** connection_pointer_;

            // the root of the data structure to modify
            NodeType** root_;

            // original connection pointer
            // should not have changed during transaction
            NodeType* conn_pointer_snapshot_;


            // if a node is popped from path,
            // to be set as the new connection point,
            // the old head will be its child at index
            // child_to_exchange_
            int child_to_exchange_;

            // path to connection point
            Stack& path_to_conn_point_;

            // copy was successfully connected
            bool copy_connected_;


            // The global lock
            TSX::SpinLock& _lock;

            // tsx stats
            TSX::TSXStats &stats_;


            // the root node of the tree of copies
            SafeNode<NodeType>* head_;

            // determines if any modification
            // was done to the tree
            bool tree_was_modified_;

            // won't run the operation 
            // using tsx if lock is already locked
            bool already_locked_;
            
            // transactional retries before
            // acquiring fallback lock
            int& trans_retries_;

            // set to true if operation
            // throws a validation error
            bool validation_aborted_;
            

            using SafeNodeType = SafeNode<NodeType>;

            #if TREE_TYPE == GENERAL_TREE
                // chech if path to connPoint hasn't changed
                // and if conn_point exists in the tree
                bool path_unchanged() {
                    // std::cout << "Path recorded" << std::endl;
                    // path_to_conn_point_.print_contents();

                    
                    auto to_be_validated = *root_;
                    
                    // modifying at root?
                    if (path_to_conn_point_.Empty()) {
                        return connection_point_ == to_be_validated;
                    }

                    // root is unchanged?
                    auto supposed_root = path_to_conn_point_.bottom();

                    auto next_child = supposed_root.next_child;

                    if (supposed_root.node != to_be_validated) {
                        return false;
                    }
                    


                    // do the same for the rest of the path to the conn_point
                    NodeType* supposed_next_child = supposed_root.node->getChild(next_child);

                    for (int i = 1; i < path_to_conn_point_.size(); ++i) {

                        
                        if (path_to_conn_point_[i].node != supposed_next_child) {
                            return false;
                        }

                        to_be_validated = supposed_next_child;
                        supposed_next_child = to_be_validated->getChild(path_to_conn_point_[i].next_child);
                    }


                    // the last node of the path should be the connection point 
                    return supposed_next_child == connection_point_;
                }
            #endif


            
            
            // lookup but returns node, nullptr when not found
            bool connPointConnected() {
                #if TREE_TYPE == SEARCH_TREE
                    return find_target_node<NodeType>(*root_, connection_point_);
                #else
                    return path_unchanged();
                #endif
            }


            void add_to_validation_set(SafeNode<NodeType>* a_node) {
                #ifdef PREALLOC_VALIDATION_SET
                    validation_set_.push_back(a_node);
                #else
                    validation_set_.push_back(a_node);  
                #endif
            }


            NodeType* getConnPoint() {
                return connection_point_;
            }

            bool connect_atomically() noexcept {

                if (validation_aborted_) {
                    return false;
                }

                unsigned char err_status = 0;
                
                TSX::TSXTransOnlyGuard guard(trans_retries_,_lock,err_status,stats_, already_locked_, TSX::STUBBORN);

                if (err_status != VALIDATION_FAILED) {

                    if (!validate_copy()) {
                        return false;
                    }

                    connect_copy();
                    return true;
                }

                return false;
            }

            void validation_abort() {
                validation_aborted_ = true;
                trans_retries_--;
            }

            
            bool copyWasConnected() {
                return copy_connected_;
            }

            

            // connect created tree copy by exchanging proper parent pointer
            void connect_copy() noexcept {

                NodeType* copied_tree_head_ = head_ ? head_->node_to_be_connected() : nullptr; 

                // inserting at root
                if (!connection_point_) {
                    connection_pointer_ = root_;
                }

                // connect connection point to the new tree
                *connection_pointer_ = copied_tree_head_;
                
                // originals can be deleted
                copy_connected_ = true;
            }


            // validate copy and abort transaction
            // on failure
            bool validate_copy() {
                if (!connection_point_) {
                    // insertion at root
                    // has root copy changed ?
                    if (conn_pointer_snapshot_ != *root_) {
                        TSX::TSXGuard::abort<VALIDATION_FAILED>();
                        return false;
                    }
                } else { 
                    // has the connection pointer to be modified
                    // changed?
                    if (conn_pointer_snapshot_ != connection_point_->getChild(child_to_exchange_)) {
                        TSX::TSXGuard::abort<VALIDATION_FAILED>();
                        return false;
                    }

                    // is the root of the tree of copies still connected?
                    if (!connPointConnected()) {
                        TSX::TSXGuard::abort<VALIDATION_FAILED>();
                        return false;
                    }
                }


                #ifdef PREALLOC_VALIDATION_SET
                    // have the children pointers of the original nodes changed?
                    for (auto i = 0; i < validation_set_.size(); i++) {
                            auto safe_node = validation_set_.get(i);
                            // skip for new nodes
                            if (safe_node->node_type_ == SafeNode<NodeType>::ORIG_TREE_NODE) {
                                // the original node and its current children
                                const auto original_node = safe_node->original_;

                                // the saved children which shouldn't have changed
                                const auto &saved_children = safe_node->children_pointers_snapshot_;

                                for (int i = 0; i < NodeType::maxChildren(); i++) {
                                    if (saved_children[i] != original_node->getChild(i)) {
                                        TSX::TSXGuard::abort<VALIDATION_FAILED>();
                                        return false;
                                    }
                                }
                            }  
                    }

                #else
                    // have the children pointers of the original nodes changed?
                    for (auto it = validation_set_.begin(); it != validation_set_.end(); ++it) {
                            // skip for new nodes
                            if ((*it)->node_type_ == SafeNode<NodeType>::ORIG_TREE_NODE) {
                                // the original node and its current children
                                const auto original_node = (*it)->original_;

                                // the saved children which shouldn't have changed
                                const auto &saved_children = (*it)->children_pointers_snapshot_;

                                for (int i = 0; i < NodeType::maxChildren(); i++) {
                                    if (saved_children[i] != original_node->getChild(i)) {
                                        TSX::TSXGuard::abort<VALIDATION_FAILED>();
                                        return false;
                                    }
                                }
                            }  
                    }

                #endif

               

                // all checks ok
                return true;
            }



        public:
            // no copying or moving
            ConnPoint& operator=(const ConnPoint&) = delete;
            ConnPoint(const ConnPoint&) = delete;

            ConnPoint(ConnPointData<NodeType>& data):
            connect_success_(__internal__thread_transaction_success_flag__),
            connection_point_(data.connection_point_), connection_pointer_(nullptr),
            root_(data.root_of_structure),
            conn_pointer_snapshot_(data.con_ptr.snapshot),
            child_to_exchange_(data.con_ptr.child_index),
            path_to_conn_point_(data.path), 
            copy_connected_(false),
            _lock(TSX::__internal__trans_pointer->get_lock()),
            stats_(TSX::__internal__trans_pointer->get_stats()),
            head_(nullptr),
            tree_was_modified_(false),
            already_locked_(TSX::__internal__trans_pointer->has_locked()),
            trans_retries_(TSX::__internal__trans_pointer->get_retries()),
            validation_aborted_(false)
            {
                #ifdef TSX_MEM_POOL
                    pool_.reset();
                #endif

                #ifdef PREALLOC_VALIDATION_SET
                    validation_set_.reset();
                #endif

                if (child_to_exchange_ != INSERT_POSITIONS::AT_ROOT) {
                    connection_pointer_ =  connection_point_->getChildPointer(child_to_exchange_);
                } else {
                    connection_pointer_ = nullptr;
                }

                connect_success_ = false;

                #ifdef USER_MEM_POOL 
                    user_node_pool_.set_checkpoint();
                #endif
            }

            ~ConnPoint() {

                // if ConnPoint is deleted
                // but its copy
                // isn't connected
                // everything should be cleaned up
                

                // in transaction
                if (tree_was_modified_ && !copy_connected_) {
                    connect_success_ = connect_atomically();
                }

                #ifdef USER_MEM_POOL 
                    if (!copy_connected_) {
                        user_node_pool_.reset_to_checkpoint();
                    }
                #endif


                
                
                // CLEANUP DISABLED !!!
                const bool clean_all = !copy_connected_;

                #ifdef PREALLOC_VALIDATION_SET
                    for (int i = 0; i < validation_set_.size(); i++) {
                        auto item = validation_set_.get(i);

                        if (clean_all || item->deleted_) {
                            item->deleted_ |= clean_all;
                            // // cleanup should happen here
                            // #ifndef TSX_MEM_POOL
                            //     delete (*it);
                            // #else
                            //     (*it)->cleanup();
                            // // #endif
                        }


                    }
                #else
                    for (auto it = validation_set_.begin(); it != validation_set_.end(); ++it)  {
                        if (clean_all || (*it)->deleted_) {
                            (*it)->deleted_ |= clean_all;
                            // // cleanup should happen here
                            // #ifndef TSX_MEM_POOL
                            //     delete (*it);
                            // #else
                            //     (*it)->cleanup();
                            // // #endif
                        }

                    }
                #endif
                

                

            }

            // if using memory pool
            // used to create new nodes
            // instead of new
            #ifdef USER_MEM_POOL
                // create a new node from the pool
                template <typename ...Args>
                static NodeType* create_new_node(Args&& ...args) {
                    return user_node_pool_.create(std::forward<Args>(args)...);

                }

                static void init_node_pool() {
                    user_node_pool_.fill_pool();
                }

                static void reset_node_pool() {
                    user_node_pool_.hard_reset();
                }

            #endif


            // returns the saved connection pointer value
            // of the original tree. Can be used as it is
            // validated.
            NodeType* getConnPointer() {
                return conn_pointer_snapshot_;
            }

            // create_safe: use to create a SafeNode from a node not already contained in the
            // original tree, returns SafeNode containing the new node.
            SafeNode<NodeType>* create_safe(NodeType* some_node) {
                if (!some_node) {
                    return nullptr;
                }

                
                #ifdef TSX_MEM_POOL
                    auto newNode = ConnPoint::pool_.create(*this, some_node, SafeNode<NodeType>::NEW_NODE);
                #else
                    auto newNode = new SafeNode<NodeType>(*this, some_node, SafeNode<NodeType>::NEW_NODE);
                #endif
                
                return newNode;  
            }

            // wrap_safe: wrap a node in a SafeNode,
            // node is from the original tree,
            // returns nullptr if nullptr is given.
            SafeNode<NodeType>* wrap_safe(NodeType* some_node) {
                if (!some_node) {
                    return nullptr;
                }
            #ifdef TSX_MEM_POOL
                return ConnPoint::pool_.create(*this, some_node);
            #else
                return new SafeNode<NodeType>(*this, some_node);
            #endif
            }

            // wrap_safe: wrap a node in a SafeNode,
            // node is from the original tree but skip validation
            SafeNode<NodeType>* wrap_no_validate(NodeType* some_node) {
                if (!some_node) {
                    return nullptr;
                }
            #ifdef TSX_MEM_POOL
                return ConnPoint::pool_.create(*this, some_node, SafeNode<NodeType>::ORIG_TREE_NO_VALIDATION);
            #else
                return new SafeNode<NodeType>(*this, some_node, SafeNode<NodeType>::ORIG_TREE_NO_VALIDATION);
            #endif
            }




            // newTree: Wraps newly created node in a SafeNode and sets it to
            // be connected as the root node of the original tree.
            // Useful for creating an entirely new tree.
            // Returns new head.
            SafeNode<NodeType>* newTree(NodeType* node) {

                // also set connection pointer
                connection_pointer_ = nullptr;

                head_ = !node  ? nullptr : create_safe(node);

                return head_;
            }


            // setRoot: replace current root of the tree of copies  
            // with a SafeNode
            SafeNode<NodeType>* setRoot(SafeNode<NodeType>* newSafeNode) {
                tree_was_modified_ = true;
                head_ = newSafeNode;
                return head_;
            }
            

            // get current root of the tree of copies
            SafeNode<NodeType>* getRoot() {
                if (!tree_was_modified_ && !head_) {
                    tree_was_modified_ = true;
                    head_ = wrap_safe(conn_pointer_snapshot_);
                    if (head_) {
                        head_->rwRef();
                    }
                }
                return head_;
            }

            // add node to validation set
            // without adding it to the tree of copies.
            // Useful if node is to be thrown away.
            // Returns SafeNode.
            SafeNode<NodeType> watchNode(NodeType* node) {
                return wrap_safe(node);
            }


            // pop_path: pops a node from the path to the connection point,
            // sets it as the new connection point, then connects the old 
            // connection point as the root of the tree of copies.
            // Used to connect the tree of copies at a higher tree level.
            SafeNode<NodeType>* pop_path() {

                // connection point should be not null
                // to pop, else it makes no sense

                if (!connection_point_) {
                    return nullptr;
                }


                NodeType* new_conn_point = nullptr;
                int new_conn_point_next_index = -1;

                if (!path_to_conn_point_.Empty()) {
                    // pop node
                    auto parent_of_conn_point = path_to_conn_point_.pop();
                    new_conn_point = parent_of_conn_point.node;
                    new_conn_point_next_index = parent_of_conn_point.next_child;
                }

                // keep old conn pointer snapshot to force its validation
                auto old_conn_pointer_snapshot_ = conn_pointer_snapshot_;

                // for the new connection point
                // the pointer to change
                // should point to the previous
                // connection point
                conn_pointer_snapshot_ = connection_point_;
                
                // calculate which child should be changed to insert the tree of copies
                // by using the previous connection point's key
                const int next_child_index = new_conn_point_next_index;


                // change connection point to the new conn point
                connection_point_ = new_conn_point;

                // also update connection pointer
                // if not at root
                if (connection_point_) {
                    connection_pointer_ = connection_point_->getChildPointer(next_child_index);
                }
                
                
                // now for the root of the tree

                // using the previous connection point
                // make a safeNode to turn to new head
                SafeNode<NodeType>* newHead = wrap_safe(conn_pointer_snapshot_);


                // and connect it
                newHead->setChild(child_to_exchange_, head_);

                
                // ACHTUNG
                // the old connection point should necessarily continue 
                // pointing to it's old value
                #ifdef TM_EARLY_ABORT
                    if (newHead->original->getChild(child_to_exchange_) != old_conn_pointer_snapshot_) {
                        validation_abort();
                        throw ValidationAbortException();
                    }
                #endif

                newHead->children_pointers_snapshot_[child_to_exchange_] = old_conn_pointer_snapshot_;
                
                
                // now head can be updated
                head_ = newHead; 
                // as well which child to update for the next pop
                child_to_exchange_ = next_child_index;

                return head_;
            }

            


    };

    // STATIC DECLARATIONS

    //---------------------//
    #ifdef TSX_MEM_POOL
        template <class NodeType>
        thread_local memory_pool<SafeNode<NodeType>> ConnPoint<NodeType>::pool_(100);
    #endif

    #ifdef USER_MEM_POOL
        template <class NodeType>
        thread_local memory_pool_tracked<NodeType> ConnPoint<NodeType>::user_node_pool_(50000000);
    #endif

     #ifdef PREALLOC_VALIDATION_SET
        template <class NodeType>
        thread_local PreAllocVec<SafeNode<NodeType>*,500> ConnPoint<NodeType>::validation_set_;
    #endif

    //---------------------//

    // Search functions come for free, if 
    // building a search tree

    #if TREE_TYPE == SEARCH_TREE

        // Traverse tree with given root and return the node with
        // the key given. The node will be determined by the traversalDone
        // method and the path taken by the nextChild method. 
        template <class NodeType>
        inline NodeType* find(NodeType* root, typename NodeType::KeyType desired_key) {
            auto curr = root;
            for (; curr && !curr->traversalDone(desired_key); curr = curr->getChild(curr->nextChild(desired_key))) {
            // search for node with key
            }

            return curr;
        }

        // Traverse tree with given root and find if it contains
        // the target node. The nextChild method determines the
        // path taken
        template <class NodeType>
        inline bool find_target_node(NodeType* root, NodeType* target) {
            auto curr = root;

            for (; curr; curr = curr->getChild(curr->nextChild(target))) {
                if (curr == target) {
                    return true;
                }
            }

            return curr == target;
        }

        // find_conn_point: traverse tree with given root and return a ConnPointData object for the
        // node with the given key. The node will be determined by the traversalDone
        // method and the path taken by the nextChild method.
        template <class NodeType>
        ConnPointData<NodeType> find_conn_point(typename NodeType::KeyType key, NodeType** root) {
            // give address of root node
            // find the connection point of both remove and insert operations
            // we are looking for the node before the one with key

            // everything we need to create a conn point
            ConnPointData<NodeType> result;

            result.root_of_structure = root;

            NodeType* orig_head = *root;  // first step, get snapshot of head 
                                        //(can change during runtime, use only copy)

            // will keep the previous node      
            // which will be the connection point


            NodeType* curr = orig_head;

            // search for node with key
            for (; curr && !curr->traversalDone(key);) {
                
                auto next_child = curr->nextChild(key);

                // search for node with key, adding path to stack and keeping the prev
                result.path.push(curr, next_child);    
                curr = curr->getChild(next_child);                                     
            };



            int prev_next_child_index = INSERT_POSITIONS::AT_ROOT;

            NodeType* prev_s = nullptr;

            if (!result.path.Empty()) {
                auto stack_top =  result.path.pop();
                prev_s = stack_top.node;
                prev_next_child_index = stack_top.next_child;
            }


            // was a node found
            result.found_ = curr && curr->hasKey(key);

            // set prev as the connection point
            // will be null if operation happens at head
            result.connection_point_ = prev_s;

            // the child to which the pointer to change corresponds
            // should be AT_ROOT if there's no connection_point
            result.con_ptr.child_index = prev_next_child_index;

            // the original value of the pointer to change
            result.con_ptr.snapshot = result.connection_point_ ? curr : orig_head;

            return result;
        }

    #endif

    

        
    }



#endif
