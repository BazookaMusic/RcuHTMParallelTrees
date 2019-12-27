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



#include "generic_tree.hpp"
#include "TSXGuard.hpp"

static constexpr int MAX_THREADS = RCU_HTM_MAX_THREADS;

/*  SafeTrees require Nodes with the following methods for the general tree version
    1.     A copy constructor, copy assignment and destructor (RULE OF THREE)
    2.     NodeType* getChild(int i)
    3.     void setChild(int i, NodeType * n)
    4.     NodeType** getChildPointer(int i): get a pointer to the stored tree pointer
            (where the i'th child is stored)
    The optimized search tree version also requires:
    5.     A bool hasKey(int key) method to return if the node contains a key
    6.     static constexpr int maxChildren(): max amount of children for each node, 
    7.     bool traversalDone(int key): returns true if the node with key is the caller, can also
            consider other factors like if it is a terminal node
    8.     int nextChild(int target_key): return index of next child when looking for node with target_key
    9.     int nextChild(NodeType* target): return index of next child when looking for target node
*/

// used by all ConnectionPoint class instances to signal
// if a transaction succeeded or failed
thread_local bool __internal__thread_transaction_success_flag__ = false;

// thread safe operation macro definitions
#ifdef TM_EARLY_ABORT
    #define TM_SAFE_OPERATION_START \
    __internal__thread_transaction_success_flag__ = false;\
    while(!__internal__thread_transaction_success_flag__) { \
    try {\

    #define TM_SAFE_OPERATION_END \
    }catch (const ValidationAbortException& e) {\
        __internal__thread_transaction_success_flag__ = false;\
    }\
    }

#else
    #define TM_SAFE_OPERATION_START \
    __internal__thread_transaction_success_flag__ = false;\
    while(!__internal__thread_transaction_success_flag__) \


    #define TM_SAFE_OPERATION_END
#endif


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
            int at_level;
            TreePathStackWithIndex<NodeType> path;


        public:
            // PathTracker: receives adress of pointer to root of structure. 
            // Tracks a path on the tree to be used for a thread safe operation.
            explicit PathTracker(NodeType** root): root_(root), current_pos_(*root), at_level(-1) {}

            // getNode: return the node where the
            // tracker is currently pointing
            NodeType* getNode() {
                return current_pos_;
            }

            // moveToChild: move the tracker to one of
            // the children of the node it's currently pointing to
            NodeType* moveToChild(int pos) {
                ++at_level;

                path.push(current_pos_, pos);
                current_pos_ = current_pos_->getChild(pos);

                return current_pos_;
            }

             // moveUp: move the tracker up
            // n levels on the path it has followed
            void moveUp(int n_levels = 1) {
                at_level = at_level >= n_levels ? at_level - n_levels: -1;

                for (int i = 0; at_level >= 0 && i < n_levels - 1; i++) {
                    path.pop();
                }

                if (!path.Empty()) {
                    current_pos_ =  path.pop().node;
                }   
            }

            // set previous node in path followed
            // as connection point. Return ConnPointData
            // object which allows for a tree
            // of copies to be constructed.
            ConnPointData<NodeType> connectHere() {

                ConnPointData<NodeType> conn_point_snapshot;

                bool at_root = at_level == -1;
                NodeAndNextPointer<NodeType> conn_point_and_next_child = path.pop();
                conn_point_snapshot.connection_point_ = at_root ? nullptr: conn_point_and_next_child.node;

                path.move_to(conn_point_snapshot.path);

                conn_point_snapshot.root_of_structure = root_;

                conn_point_snapshot.con_ptr.child_index = at_root? INSERT_POSITIONS::AT_ROOT :  conn_point_and_next_child.next_child;
                conn_point_snapshot.con_ptr.snapshot = current_pos_;


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
    friend class ConnPoint<NodeType>;
    private:
        ConnPoint<NodeType>& conn_point;
        NodeType* const original;
        NodeType* copy;
        std::array<SafeNode*, NodeType::maxChildren()> children;
        std::array<bool,NodeType::maxChildren()> modified;
        std::array<NodeType*, NodeType::maxChildren()> children_pointers_snapshot;
        std::size_t children_pointers_snapshot_hash;

        bool deleted;

        bool node_from_orig_tree;


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
                sub_tree_delete(root->children[i]);
            }
            
            root->deleted = true;
        }

        void make_copy() {
            assert(original == copy);

            #ifdef USER_MEM_POOL
                copy = ConnPoint<NodeType>::create_new_node(*original);
            #else
                copy = new NodeType(*original);
            #endif


            for (int i = 0; i < NodeType::maxChildren(); i++) {
                #ifdef TM_EARLY_ABORT_ON_COPY
                    if (children_pointers_snapshot[i] != original->getChild(i)) {
                        conn_point. validation_abort();
                        throw ValidationAbortException();
                    }
                #endif

                copy->setChild(i,children_pointers_snapshot[i]);
            }
        }

        NodeType* node_to_be_connected() {
            return copy;
        }

    
    public:
        SafeNode(const SafeNode& other) = delete;
        SafeNode& operator=(const SafeNode&) = delete;
        SafeNode(){}

        #ifndef USER_NODE_POOL
        // ~SafeNode() {
        //     if (node_from_orig_tree) {
        //         if (deleted && original != copy) {
        //             delete copy;
        //         }

        //         if (conn_point.copyWasConnected()) {
        //             delete original;
        //         }
        //     }  
        // }
        #endif
        SafeNode(ConnPoint<NodeType>& _conn_point, NodeType* _original, bool node_from_orig_tree = true): 
        conn_point(_conn_point), 
        original(_original), 
        copy(_original),
        deleted(false),
        node_from_orig_tree(node_from_orig_tree)
        {

            // all nodes will be added to validation set
            // as it is used for memory management as well
            conn_point.add_to_validation_set(this);

            if (node_from_orig_tree) {
                for (int i = 0; i < NodeType::maxChildren(); i++) {
                        // keep backup of the original child pointers
                        auto original_child = original->getChild(i);
                        children_pointers_snapshot[i] = original_child;
                }
            }
            
            for (int i = 0; i < NodeType::maxChildren(); i++)
            {
                // the modified children
                // are initialized to null
                children[i] = nullptr;
                modified[i] = false;
            }        
        }

        static constexpr int children_length() {
            return NodeType::maxChildren();
        }

        // getOriginal: used to access original
        // node (any modifications are not guaranteed thread safe)
        const NodeType* getOriginal() {
            return original;
        }

        // peekOriginal: used to access original
        // node (any modifications are not guaranteed thread safe)
        const NodeType* peekOriginal() const {
            return original;
        }

        // rwRef: get reference to safe copy of node,
        // can be modified freely
        // as only one thread has access to it
        NodeType* rwRef() {
            if (node_from_orig_tree && copy == original) {
                make_copy();
            }
            
            return copy;
        }

        // returns a reference safe for reading
        const NodeType* rRef() const {
            return copy;
        }



        NodeType* peekChild(const int child_pos) {
            return  copy == original? children_pointers_snapshot[child_pos]: copy->getChild(child_pos);
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

            // force parent copy
            // getting a child
            // explicitly
            // means that you
            // seek to modify it
            if (copy == original && node_from_orig_tree) {
                make_copy();
            }
            

            
            //child has already been copied
            if (modified[child_pos]) {
                return children[child_pos];
            }

            // child has been modified
            modified[child_pos] = true;

            SafeNode<NodeType>* new_safe = nullptr;

            // if node has been copied read from copy
            // else read from original child pointers backup
            const auto original_child = copy->getChild(child_pos);

            //child not copied yet,
            //copy and use the copy from now on
            if (original_child) {
                new_safe = node_from_orig_tree? conn_point.wrap_safe(original_child): conn_point.create_safe(original_child);
                children[child_pos] = new_safe;
                copy->setChild(child_pos, new_safe->rwRef());
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

            // node needs to be modified
            // copy it
            if (copy == original && node_from_orig_tree) {
                make_copy();
            }

            // copy has happened
            modified[child_pos] = true;

            auto child = children[child_pos];

            if (!child) {
                children[child_pos] = newVal;
                copy->setChild(child_pos, newVal? newVal->node_to_be_connected() : nullptr);
                return nullptr;
            }

            // update children of safe node
            // old node is deleted
            SafeNode<NodeType>* old_node = move(children[child_pos], newVal);
            
            // connect copy pointer, replace with new
            copy->setChild(child_pos, newVal? newVal->node_to_be_connected() : nullptr);

            return old_node;
        }


        // clipTree: delete entire subtree with
        // the child at position child_pos as root
        void clipTree(int child_pos) {
            assert(child_pos >= 0 && child_pos < NodeType::maxChildren());

            if (children[child_pos]) {
                children[child_pos]->subtreeDelete();
            }

            children[child_pos] = nullptr;
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
struct NodeData {
    SafeNode<NodeType>* safeNode;
    std::array<NodeType*, NodeType::maxChildren()> childrenSnapShot;
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
NodeType* find(NodeType* root, int desired_key);


// forward declaration to have it grouped with
// find_conn_point
template <class NodeType>
bool find_target_node(NodeType* root, NodeType* target);

template <class T>
struct ConnPointData {
    friend class ConnPoint<T>;
    template <class NodeType>
    friend ConnPointData<NodeType> find_conn_point(int key, NodeType** root);
    #if TREE_TYPE == GENERAL_TREE
        friend class PathTracker<T>;
    #endif

    private:
        ConnectionPointer<T> con_ptr;    
        T* connection_point_;
        #if TREE_TYPE == SEARCH_TREE
            bool found_;
        #endif
        TreePathStackWithIndex<T> path;
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


        
};



class Transaction {
    private:
        int& retries_;
        TSX::SpinLock &lock_;
        TSX::TSXStats &stats_;
        bool has_locked_;

    public:
        Transaction(int &retries,TSX::SpinLock &global_lock, TSX::TSXStats &stats): 
        retries_(retries), 
        lock_(global_lock), 
        stats_(stats),
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

#ifdef TSX_MEM_POOL
    // A SafeNode Memory Pool
    

    template<class Object>
    struct memory_pool {
        using buffer_type = typename std::aligned_storage<sizeof(Object), alignof(Object)>::type;

        static_assert(sizeof(buffer_type) >= sizeof(Object), "wrong size");

        explicit memory_pool(std::size_t limit)
                : objects(new buffer_type[limit]),limit_(limit), used_(0) {
        }

        memory_pool(const memory_pool&) = delete;
        memory_pool(memory_pool&&) = default;
        memory_pool& operator=(const memory_pool&) = delete;
        memory_pool& operator=(memory_pool&&) = delete;

        template<class...Args>
        Object* create(Args &&...args) {
            if (used_ < limit_) {
                auto candidate = new(std::addressof(objects[used_])) Object(std::forward<Args>(args)...);
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
            return objects;
        }

        buffer_type* objects;
        std::size_t limit_;
        std::size_t used_;
       
    };

    template<class Object>
    struct memory_pool_tracked: public memory_pool<Object> {

        using buffer_type = typename std::aligned_storage<sizeof(Object), alignof(Object)>::type;
        
        memory_pool_tracked(std::size_t limit): memory_pool<Object>(limit) {
            pool_lock.lock();
            ++index;
            thread_buffers[index] = memory_pool<Object>::objects;
            pool_lock.unlock();
        }

        void fill_pool() {
            memory_pool<Object>::objects = new buffer_type[memory_pool<Object>::limit_];
            pool_lock.lock();
            ++index;
            thread_buffers[index] = memory_pool<Object>::objects;
            pool_lock.unlock();
        }

        
         void hard_reset() {
            for (auto i = 0; i <= index; i++) {
                delete [] thread_buffers[i];
                thread_buffers[i] = nullptr;
            }

            memory_pool<Object>::used_ = 0;
            memory_pool<Object>::objects = nullptr;

            index = -1;

        }

         static TSX::SpinLock pool_lock;
         static typename memory_pool_tracked<Object>::buffer_type** thread_buffers;
         static int index;

    };

    template<class Object>
    TSX::SpinLock memory_pool_tracked<Object>::pool_lock;

    template<class Object>
    typename memory_pool_tracked<Object>::buffer_type** memory_pool_tracked<Object>::thread_buffers = new buffer_type*[MAX_THREADS];

    template<class Object>
    int memory_pool_tracked<Object>::index = -1;
#endif


template <class NodeType>
class ConnPoint
{
    using Stack = TreePathStackWithIndex<NodeType>;
    friend class SafeNode<NodeType>;

    private:
        #ifdef TSX_MEM_POOL
            thread_local static memory_pool<SafeNode<NodeType>> pool;
        #endif

        #ifdef USER_MEM_POOL
            thread_local static memory_pool_tracked<NodeType> user_node_pool;
        #endif

        // returns if copy connection was successful
        bool& connect_success;
        #ifdef STATIC_VECTORS_AND_ARRAYS
            static thread_local std::vector<SafeNode<NodeType>*> validation_set;
        #else
            std::deque<SafeNode<NodeType>*> validation_set;
        #endif

        // the connection point is the node whose child pointer will
        // be modified to connect the tree of copies
        NodeType* connection_point;
        // if null, the insertion should be done at
        // the root of the data structure

        // the point of insertion of the new tree
        NodeType** connection_pointer;

        // the root of the data structure to modify
        NodeType** _root;

        // original connection pointer
        // should not have changed during transaction
        NodeType* conn_pointer_snapshot;


        // if a node is popped from path,
        // to be set as the new connection point,
        // the old head will be its child at index
        // child_to_exchange
        int child_to_exchange;

        // path to connection point
        Stack& path_to_conn_point;

        // copy was successfully connected
        bool copy_connected;


        // The global lock
        TSX::SpinLock& _lock;

        // tsx stats
        TSX::TSXStats &_stats;


        // the root node of the tree of copies
        SafeNode<NodeType>* head;

        // determines if any modification
        // was done to the tree
        bool tree_was_modified;

        // won't run the operation 
        // using tsx if lock is already locked
        bool already_locked;
        
        // transactional retries before
        // acquiring fallback lock
        int& trans_retries;

        // set to true if operation
        // throws a validation error
        bool validation_aborted;
        

        using SafeNodeType = SafeNode<NodeType>;

        #if TREE_TYPE == GENERAL_TREE
            // chech if path to connPoint hasn't changed
            // and if conn_point exists in the tree
            bool path_unchanged() {

                
                // modifying at root?
                if (path_to_conn_point.Empty()) {
                    return true;
                }

                // root is unchanged?
                auto supposed_root = path_to_conn_point.bottom();

                auto to_be_validated = *_root;
                auto next_child = supposed_root.next_child;

                if (supposed_root.node != to_be_validated) {
                    return false;
                }

               


                // do the same for the rest of the path to the conn_point
                NodeType* supposed_next_child = supposed_root.node->getChild(next_child);

                for (int i = 1; i < path_to_conn_point.size(); ++i) {
                    
                    if (path_to_conn_point[i].node != supposed_next_child) {
                        return false;
                    }

                    to_be_validated = supposed_next_child;
                    supposed_next_child = to_be_validated->getChild(path_to_conn_point[i].next_child);
                }


                // the last node of the path should be the connection point 
                return supposed_next_child == connection_point;
            }
        #endif


        
        
        // lookup but returns node, nullptr when not found
        bool connPointConnected() {
            #if TREE_TYPE == SEARCH_TREE
                return find_target_node<NodeType>(*_root, connection_point);
            #else
                return path_unchanged();
            #endif
        }


        void add_to_validation_set(SafeNode<NodeType>* a_node) {
            #ifdef STATIC_VECTORS_AND_ARRAYS
                validation_set.emplace_back(a_node);
            #else
                validation_set.push_back(a_node);
            #endif    
        }


        NodeType* getConnPoint() {
            return connection_point;
        }

        bool connect_atomically() noexcept {

            if (validation_aborted) {
                return false;
            }

            unsigned char err_status = 0;
            
            TSX::TSXTransOnlyGuard guard(trans_retries,_lock,err_status,_stats, already_locked, TSX::STUBBORN);

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
            validation_aborted = true;
            trans_retries--;
        }


    public:
        // no copying or moving
        ConnPoint& operator=(const ConnPoint&) = delete;
        ConnPoint(const ConnPoint&) = delete;

        ConnPoint(ConnPointData<NodeType>& data, Transaction& t):
        connect_success(__internal__thread_transaction_success_flag__),
        connection_point(data.connection_point_), connection_pointer(nullptr),
        _root(data.root_of_structure),
        conn_pointer_snapshot(data.con_ptr.snapshot),
        child_to_exchange(data.con_ptr.child_index),
        path_to_conn_point(data.path), 
        copy_connected(false),
        _lock(t.get_lock()),
        _stats(t.get_stats()),
        head(nullptr),
        tree_was_modified(false),
        already_locked(t.has_locked()),
        trans_retries(t.get_retries()),
        validation_aborted(false)
        {
            #ifdef TSX_MEM_POOL
                pool.reset();
            #endif

            #ifdef STATIC_VECTORS_AND_ARRAYS
                validation_set.clear();
            #endif

            if (child_to_exchange != INSERT_POSITIONS::AT_ROOT) {
                connection_pointer =  connection_point->getChildPointer(child_to_exchange);
            } else {
                connection_pointer = nullptr;
            }

            connect_success = false;
        }

         ~ConnPoint() {

            // if ConnPoint is deleted
            // but its copy
            // isn't connected
            // everything should be cleaned up
            

            // in transaction
            if (tree_was_modified && !copy_connected) {
                connect_success = connect_atomically();
            }
            //
            
            // CLEANUP DISABLED !!!
            
            const bool clean_all = !copy_connected;

            for (auto it = validation_set.begin(); it != validation_set.end(); ++it)  {
                if (clean_all || (*it)->deleted) {
                    (*it)->deleted |= clean_all;
                    // // cleanup should happen here
                    // #ifndef TSX_MEM_POOL
                    //     delete (*it);
                    // #else
                    //     (*it)->cleanup();
                    // // #endif
                }

            }

        }

        // if using memory pool
        // used to create new nodes
        // instead of new
        #ifdef USER_MEM_POOL
            // create a new node from the pool
            template <typename ...Args>
            static NodeType* create_new_node(Args&& ...args) {
                return user_node_pool.create(std::forward<Args>(args)...);

            }

            static void init_node_pool() {
                user_node_pool.fill_pool();
            }

            static void reset_node_pool() {
                user_node_pool.hard_reset();
            }

        #endif


        // returns the saved connection pointer value
        // of the original tree. Can be used as it is
        // validated.
        NodeType* getConnPointer() {
            return conn_pointer_snapshot;
        }

        // create_safe: use to create a SafeNode from a node not already contained in the
        // original tree, returns SafeNode containing the new node.
        SafeNode<NodeType>* create_safe(NodeType* some_node) {
            if (!some_node) {
                return nullptr;
            }

            
            #ifdef TSX_MEM_POOL
                auto newNode = ConnPoint::pool.create(*this, some_node, false);
            #else
                auto newNode = new SafeNode<NodeType>(*this, some_node, false);
            #endif


            for (auto elem = newNode->modified.begin(); elem != newNode->modified.end(); elem++ ) {
                    *elem = true;
            }
            
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
            return ConnPoint::pool.create(*this, some_node);
           #else
            return new SafeNode<NodeType>(*this, some_node);
           #endif
        }



        // newTree: Wraps newly created node in a SafeNode and sets it to
        // be connected as the root node of the original tree.
        // Useful for creating an entirely new tree.
        // Returns new head.
        SafeNode<NodeType>* newTree(NodeType* node) {

            // also set connection pointer
            connection_pointer = nullptr;

            head = !node  ? nullptr : create_safe(node);

            return head;
        }


        // setRoot: replace current root of the tree of copies  
        // with a SafeNode
        SafeNode<NodeType>* setRoot(SafeNode<NodeType>* newSafeNode) {
            tree_was_modified = true;
            head = newSafeNode;
            return head;
        }
        

        // get current root of the tree of copies
        SafeNode<NodeType>* getRoot() {
            if (!tree_was_modified && !head) {
                tree_was_modified = true;
                head = wrap_safe(conn_pointer_snapshot);
                head->rwRef();
            }
            return head;
        }

        // add node to validation set
        // without adding it to the tree of copies.
        // Useful if node is to be thrown away.
        // Returns SafeNode.
        SafeNode<NodeType> watchNode(NodeType* node) {
            return wrap_safe(node);
        }


        bool copyWasConnected() {
            return copy_connected;
        }

        

        // connect created tree copy by exchanging proper parent pointer
        void connect_copy() noexcept {

            NodeType* copied_tree_head = head ? head->node_to_be_connected() : nullptr; 

            // inserting at root
            if (!connection_point) {
                connection_pointer = _root;
            }

            // connect connection point to the new tree
            *connection_pointer = copied_tree_head;
            
            // originals can be deleted
            copy_connected = true;
        }


        // validate copy and abort transaction
        // on failure
        bool validate_copy() {
            if (!connection_point) {

                // insertion at root

                // has root copy changed ?
                if (conn_pointer_snapshot != *_root) {
                    TSX::TSXGuard::abort<VALIDATION_FAILED>();
                    return false;
                }
            } else { 
                // has the connection pointer to be modified
                // changed?
                if (conn_pointer_snapshot != connection_point->getChild(child_to_exchange)) {
                    TSX::TSXGuard::abort<VALIDATION_FAILED>();
                    return false;
                }

                // is the root of the tree of copies still connected?
                if (!connPointConnected()) {
                    TSX::TSXGuard::abort<VALIDATION_FAILED>();
                    return false;
                }
            }



            // have the children pointers of the original nodes changed?
            for (auto it = validation_set.begin(); it != validation_set.end(); ++it) {
                    // skip for new nodes
                    if ((*it)->node_from_orig_tree) {
                        // the original node and its current children
                        const auto original_node = (*it)->original;

                        // the saved children which shouldn't have changed
                        const auto &saved_children = (*it)->children_pointers_snapshot;

                        for (int i = 0; i < NodeType::maxChildren(); i++) {
                            if (saved_children[i] != original_node->getChild(i)) {
                                TSX::TSXGuard::abort<VALIDATION_FAILED>();
                                return false;
                            }
                        }
                    }  
            }

            // all checks ok
            return true;
        }

        // pop_path: pops a node from the path to the connection point,
        // sets it as the new connection point, then connects the old 
        // connection point as the root of the tree of copies.
        // Used to connect the tree of copies at a higher tree level.
        SafeNode<NodeType>* pop_path() {

            // connection point should be not null
            // to pop, else it makes no sense

            if (!connection_point) {
                return nullptr;
            }


            NodeType* new_conn_point = nullptr;
            int new_conn_point_next_index = -1;

            if (!path_to_conn_point.Empty()) {
                // pop node
                auto parent_of_conn_point = path_to_conn_point.pop();
                new_conn_point = parent_of_conn_point.node;
                new_conn_point_next_index = parent_of_conn_point.next_child;
            }

            // keep old conn pointer snapshot to force its validation
            auto old_conn_pointer_snapshot = conn_pointer_snapshot;

            // for the new connection point
            // the pointer to change
            // should point to the previous
            // connection point
            conn_pointer_snapshot = connection_point;
            
            // calculate which child should be changed to insert the tree of copies
            // by using the previous connection point's key
            const int next_child_index = new_conn_point_next_index;


            // change connection point to the new conn point
            connection_point = new_conn_point;

            // also update connection pointer
            // if not at root
            if (connection_point) {
                connection_pointer = connection_point->getChildPointer(next_child_index);
            }
            
            
            // now for the root of the tree

            // using the previous connection point
            // make a safeNode to turn to new head
            SafeNode<NodeType>* newHead = wrap_safe(conn_pointer_snapshot);


            // and connect it
            newHead->setChild(child_to_exchange, head);

            
            // ACHTUNG
            // the old connection point should necessarily continue 
            // pointing to it's old value
            #ifdef TM_EARLY_ABORT
                if (newHead->original->getChild(child_to_exchange) != old_conn_pointer_snapshot) {
                    validation_abort();
                    throw ValidationAbortException();
                }
            #endif

            newHead->children_pointers_snapshot[child_to_exchange] = old_conn_pointer_snapshot;
            
            
            // now head can be updated
            head = newHead; 
            // as well which child to update for the next pop
            child_to_exchange = next_child_index;

            return head;
        }

        


};

// STATIC DECLARATIONS

//---------------------//
#ifdef TSX_MEM_POOL
    template <class NodeType>
    thread_local memory_pool<SafeNode<NodeType>> ConnPoint<NodeType>::pool(100);
#endif

#ifdef USER_MEM_POOL
    template <class NodeType>
    thread_local memory_pool_tracked<NodeType> ConnPoint<NodeType>::user_node_pool(50000000);
#endif

#ifdef STATIC_VECTORS_AND_ARRAYS
    template <class NodeType>
    thread_local std::vector<SafeNode<NodeType>*> ConnPoint<NodeType>::validation_set(100);
#endif

//---------------------//

// Search functions come for free, if 
// building a search tree

#if TREE_TYPE == SEARCH_TREE

    // Traverse tree with given root and return the node with
    // the key given. The node will be determined by the traversalDone
    // method and the path taken by the nextChild method. 
    template <class NodeType>
    NodeType* find(NodeType* root, int desired_key) {
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
    bool find_target_node(NodeType* root, NodeType* target) {
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
    ConnPointData<NodeType> find_conn_point(int key, NodeType** root) {
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


#endif
