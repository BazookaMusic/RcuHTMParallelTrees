#ifndef SAFE_TREE_HPP
    #define SAFE_TREE_HPP


#include <memory>

#include <tuple>
#include <iostream>
#include <deque>
#include <array>
#include <tuple>



#include "generic_tree.hpp"
#include "TSXGuard.hpp"


/* SafeTrees require Nodes with the following methods
 1.     An int getKey() method to return the key
 2.     A copy constructor
 3.     NodeType* getChild(int i)
 4.     void setChild(int i, NodeType * n)
 5.     NodeType** getChildren(): returns pointer to array of children,
 6.     static constexpr int maxChildren(): max amount of children for each node, 
 7.     NodeType* find(int key): search tree and return pointer to node with desired key
 8.     bool traversalDone(int key): returns true if the node with key is the caller, can also
        consider other factors like if it is a terminal node
 9.     int nextChild(int key): return index of next child to search for key
*/

static constexpr unsigned char VALIDATION_FAILED = 0xee;

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

        bool deleted;

        bool node_from_orig_tree;


        // mark previous for deletion and move new value
        // returns old value
        static SafeNode<NodeType>* move(SafeNode<NodeType>*& to_be_replaced, SafeNode<NodeType>* replacement) {

            auto temp = to_be_replaced;

            if (to_be_replaced) {
                // mark for deletion
                to_be_replaced->deleted = true;
            }
            

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

            copy = new NodeType(*original);

            auto copy_children = copy->getChildren();

            for (int i = 0; i < NodeType::maxChildren(); i++) {
                copy_children[i] = children_pointers_snapshot[i];
            }
        }

        NodeType* internal_copy() {
            return copy;
        }

    
    public:
        SafeNode(const SafeNode& other) = delete;
        SafeNode& operator=(const SafeNode&) = delete;
        SafeNode(){};
        ~SafeNode() {
            if (node_from_orig_tree) {
                if (deleted && original != copy) {
                    delete copy;
                }

                if (conn_point.copyWasConnected()) {
                    delete original;
                }
            }  
        }
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

            const auto original_children = original->getChildren();

            if (node_from_orig_tree) {
                for (int i = 0; i < NodeType::maxChildren(); i++) {
                        // keep backup of the original child pointers
                        children_pointers_snapshot[i] = original_children[i];
                }
            }
            
            for (int i = 0; i < NodeType::maxChildren(); i++)
            {
                // the modified children
                // are initialized to null
                children[i] = nullptr;
                modified[i] = false;
            }



            
        };



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

        // safeRef: get reference to safe copy of node,
        // can be modified freely
        // as only one thread has access to it
        NodeType* safeRef() {
            if (node_from_orig_tree && copy == original) {
                make_copy();
            }
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
            if (copy == original) {
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
                copy->setChild(child_pos, new_safe->safeRef());
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
            if (copy == original) {
                make_copy();
            }

            // copy has happened
            modified[child_pos] = true;

            auto child = children[child_pos];

            if (!child) {
                children[child_pos] = newVal;
                copy->setChild(child_pos, newVal? newVal->internal_copy() : nullptr);
                return nullptr;
            }

            // update children of safe node
            // old node is deleted
            SafeNode<NodeType>* old_node = move(children[child_pos], newVal);
            
            // connect copy pointer, replace with new
            copy->setChild(child_pos, newVal? newVal->internal_copy() : nullptr);

            return old_node;
        }

        int getKey() const {
            return original->getKey();
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

enum INSERT_POSITIONS {AT_ROOT = -1, UNDEFINED = -2};

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

template <class NodeType>
struct ConnPointData {
    ConnectionPointer<NodeType> con_ptr;    
    NodeType* connection_point;
    bool found;
    TreePathStackWithIndex<NodeType> path;
    NodeType* conn_point;
    NodeType** root_of_structure;
};

class Transaction {
    private:
        int retries;
        TSX::SpinLock &lock;
        int non_explicit_retries;
        TSX::TSXStats &stats;

    public:
        Transaction(int &retries,TSX::SpinLock &global_lock, TSX::TSXStats &stats, int non_explicit_retries = 20): 
        retries(retries), 
        lock(global_lock), 
        non_explicit_retries(non_explicit_retries), 
        stats(stats) {
            if (retries == 0) {
                lock.lock();
            }
        }

        bool go_to_fallback() {
            return retries == 0;
        }

        int get_non_explicit_retries() {
            return non_explicit_retries;
        }

        TSX::SpinLock& get_lock() {
            return lock;
        }

        TSX::TSXStats& get_stats() {
            return stats;
        }

        ~Transaction() {
            if (go_to_fallback()) {
                lock.unlock();
            }

            retries--;
        }
};

template <class NodeType>
class ConnPoint
{
    using Stack = TreePathStackWithIndex<NodeType>;
    friend class SafeNode<NodeType>;

    private:
        // returns if copy connection was successful
        bool& connect_success;

        std::deque<SafeNode<NodeType>*> validation_set;

        // the connection point is the node whose child pointer will
        // be modified to connect the copied tree
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


        // the root node of the copied tree
        SafeNode<NodeType>* head;

        // determines if any modification
        // was done to the tree
        bool tree_was_modified;

        // won't run the operation 
        // using tsx if lock is already locked
        bool already_locked;
        
        // parameter for the TSXGuard
        // how many times should non explicit 
        // aborts cause a retry
        int non_explicit_retries;

        using SafeNodeType = SafeNode<NodeType>;


        
        
        // lookup but returns node, nullptr when not found
        bool connPointConnected() {
            const auto conn_point_key = connection_point->getKey();
            
            auto should_be_conn_point = find<NodeType>(*_root, conn_point_key);

            // if (should_be_conn_point != connection_point) {
            //     std::cout << "root " << *_root << " key " << conn_point_key << std::endl;
            //    std::cout << "should be " << should_be_conn_point << " is " << connection_point << std::endl;
            // }

            return should_be_conn_point == connection_point;
        }

        // wrap_safe: wrap a node in a SafeNode,
        // node is from the original tree,
        // returns nullptr if nullptr is given.
        SafeNode<NodeType>* wrap_safe(NodeType* some_node) {
            if (!some_node) {
                return nullptr;
            }
            
            return new SafeNode<NodeType>(*this, some_node);
        }

        



    public:
        // no copying or moving
        ConnPoint& operator=(const ConnPoint&) = delete;
        ConnPoint(const ConnPoint&) = delete;

        ConnPoint(ConnPointData<NodeType> data, bool& connected_successfully, Transaction& t):
        connect_success(connected_successfully),
        connection_point(data.connection_point), connection_pointer(nullptr),
        _root(data.root_of_structure),
        conn_pointer_snapshot(data.con_ptr.snapshot),
        child_to_exchange(data.con_ptr.child_index),
        path_to_conn_point(data.path), 
        copy_connected(false),
        _lock(t.get_lock()),
        _stats(t.get_stats()),
        head(nullptr),
        tree_was_modified(false),
        already_locked(t.go_to_fallback()),
        non_explicit_retries(t.get_non_explicit_retries())
        {
            if (child_to_exchange != INSERT_POSITIONS::AT_ROOT) {
                connection_pointer =  &((*connection_point).getChildren()[child_to_exchange]);
            } else {
                connection_pointer = nullptr;
            }

            connect_success = false;
        };

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
            
            // CLEANUP DISABLED

            // const bool clean_all = !copy_connected;

            // for (auto it = validation_set.begin(); it != validation_set.end(); ++it)  {
            //     it->safeNode->deleted = clean_all;
            //     delete it->safeNode;
            // }

        }

        void DEBUG_PRINT_COPIED_NODES() {
            auto a = getRoot();

            while (a && a->getChild(1)) {
                std::cout << a->safeRef() << std::endl;
                a = a->getChild(1);
            }
        }



        bool connect_atomically() noexcept {
            unsigned char err_status = 0;
            
            TSX::TSXGuardWithStats guard(non_explicit_retries,_lock,err_status,_stats, already_locked);


            if (err_status != VALIDATION_FAILED) {

                if (!validate_copy()) {
                    return false;
                }

                connect_copy();
                return true;
            }

            return false;

        }

        NodeType* getConnPoint() {
            return connection_point;
        }

        NodeType* getSavedConnPointer() {
            return conn_pointer_snapshot;
        }

        // create_safe: use to create an entirely new SafeNode not based on an original node,
        // returns nullptr if nullptr is given.
        SafeNode<NodeType>* create_safe(NodeType* some_node) {
            if (!some_node) {
                return nullptr;
            }

            auto newNode = new SafeNode<NodeType>(*this, some_node, false);

            for (auto elem = newNode->modified.begin(); elem != newNode->modified.end(); elem++ ) {
                    *elem = true;
            }
            
            return newNode;  
        }


        // insertNewHeadAt: Wraps newly created node in a SafeNode and sets it to
        // be connected as the root node of the original tree.
        // Useful for creating an entirely new tree.
        // Returns new head.
        SafeNode<NodeType>* insertAtRoot(NodeType* node) {


            // also set connection pointer
            connection_pointer = nullptr;

            head = !node  ? nullptr : create_safe(node);

            return head;
        }


        // setRoot: replace copied tree root 
        // with a SafeNode
        SafeNode<NodeType>* setRoot(SafeNode<NodeType>* newSafeNode) {
            tree_was_modified = true;
            head = newSafeNode;
            return head;
        }
        

        // get head
        SafeNode<NodeType>* getRoot() {
            if (!head) {
                tree_was_modified = true;
                head = wrap_safe(conn_pointer_snapshot);
                head->safeRef();
            }
            return head;
        }


        bool copyWasConnected() {
            return copy_connected;
        }


        void add_to_validation_set(SafeNode<NodeType>* a_node) {
            validation_set.push_back(a_node);
        }

        // connect created tree copy by exchanging proper parent pointer
        void connect_copy() noexcept {
            NodeType* copied_tree_head = head ? head->internal_copy() : nullptr; 

            if (!connection_point) {
                connection_pointer = _root;
            }

            *connection_pointer = copied_tree_head;
            
            // originals can be deleted
            copy_connected = true;
        }


        // validate copy and abort transaction
        // on failure
        bool validate_copy() {
            if (!connection_point) {
                //std::cout << "NO CONN" << std::endl;
                if (conn_pointer_snapshot != *_root) {
                   
                    // std::cout << "root was " <<  conn_pointer_snapshot << " with key " <<
                    // (conn_pointer_snapshot? conn_pointer_snapshot->getKey():-1) <<
                    // " and now is " << *_root << " with key " << (*_root ? (*_root)->getKey(): -1) <<  std::endl;
                    //std::cout << "NOT ROOT" << std::endl;
                    TSX::TSXGuard::abort<VALIDATION_FAILED>();
                    return false;
                }
            } else {
                
                if (conn_pointer_snapshot != connection_point->getChild(child_to_exchange)) {
                    // std::cout << conn_pointer_snapshot << " different from " << connection_point->getChild(child_to_exchange) << std::endl;
                   // std::cout << "WRONG CONN POINTER" << std::endl;
                    TSX::TSXGuard::abort<VALIDATION_FAILED>();
                    return false;
                }

                // 1. check if root of copied tree is still connected
                if (!connPointConnected()) {
                    //std::cout << "NOT CONNECTED" << std::endl;
                    TSX::TSXGuard::abort<VALIDATION_FAILED>();
                    return false;
                }
            }



            //3. check children of copied nodes
            for (auto it = validation_set.begin(); it != validation_set.end(); ++it) {

                    if ((*it)->node_from_orig_tree) {
                        // the original node and its current children
                        const auto original_node = (*it)->original;
                        auto current_children = original_node->getChildren();

                        // the saved children which shouldn't have changed
                        auto &saved_children = (*it)->children_pointers_snapshot;


                        for (int i = 0; i < NodeType::maxChildren(); i++) {
                            if (saved_children[i] != current_children[i]) {
                               //std::cout << "WRONG CHILDREN" << std::endl;
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
        // sets it as the new connection point,then connects the old 
        // connection point as the root of the copied tree.
        // Used to connect copied tree at a higher tree level.
        SafeNode<NodeType>* pop_path() {

            // connection point should be not null
            // to pop, else it makes no sense
            assert(connection_point && "connection point should be not null to pop");

            assert(child_to_exchange != -1 && "A copied tree should exist to pop from the path.Consider creating one through a function like \
            copyChild or place the connection point higher in the tree.");


            // empty path return nullptr
            // to signify that root has been reached
            if (path_to_conn_point.Empty()) {

                // no other nodes in path so:

                // The connection point is the root
                // of the tree instead of a node
                connection_point = nullptr;


                return nullptr;
            }

            // pop the node
            NodeType* popped_node;

            int popped_node_next_index;
            

            // pop node
            std::tie(popped_node,popped_node_next_index) = path_to_conn_point.pop();

            // for the new connection point
            // the pointer to change
            // should point to the previous
            // connection point
            conn_pointer_snapshot = connection_point;
            
            // calculate which child should be changed to insert copied tree
            // by using the previous connection point's key
            const int next_child_index = popped_node_next_index;


            // change connection point to popped node
            connection_point = popped_node;

            // also update connection pointer
            connection_pointer = &((*connection_point).getChildren()[next_child_index]);
            

            // now for the head of the tree

            // using the previous connection point
            // make a safeNode to turn to new head
            SafeNode<NodeType>* newHead = wrap_safe(conn_pointer_snapshot);

            // and connect it
            newHead->setChild(child_to_exchange, head);
            
            // ACHTUNG
            // connection with previous connection point should remain unchanged
            newHead->children_pointers_snapshot[child_to_exchange] = conn_pointer_snapshot;
            
            
            // now head can be updated
            head = newHead; 
            // as well which child to update for the next pop
            child_to_exchange = next_child_index;

            return head;
        }


};


template <class NodeType>
NodeType* find(NodeType* root, int desired_key) {
    auto curr = root;
    for (; curr && !curr->traversalDone(desired_key); curr = curr->getChild(curr->nextChild(desired_key))) {
    // search for node with key
    }

    return curr;
}

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
    NodeType* prev = nullptr;


    NodeType* curr = orig_head;

    // search for node with key
    for (; curr && !curr->traversalDone(key); prev = curr, curr = curr->getChild(curr->nextChild(key))) {
        // search for node with key, adding path to stack and keeping the prev
        result.path.push(curr,0);                                           // std::cout << curr->getKey() << "->";
    };

    //std::cout << "find conn returned " << (prev ? prev->getKey(): -1) << std::endl; 

    // was a node found
    result.found = curr && curr->getKey() == key;

    // set prev as the connection point
    // will be null if operation happens at head
    result.connection_point = prev;

    // the child to which the pointer to change corresponds
    // should be AT_ROOT if there's no connection_point
    result.con_ptr.child_index = result.connection_point ? (prev ? prev->nextChild(key): INSERT_POSITIONS::UNDEFINED) : INSERT_POSITIONS::AT_ROOT;

    // the original value of the pointer to change
    result.con_ptr.snapshot = result.connection_point ? curr : orig_head;

    return result;
}


#endif
