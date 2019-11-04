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
 1.     A default empty constructor
 2.     An int getKey() method to return the key
 3.     A copy constructor
 4.     NodeType* getChild(int i)
 5.     void setChild(int i, NodeType * n)
 6.     NodeType** getChildren(): returns pointer to array of children,
 7.     static constexpr int maxChildren(): max amount of children for each node, 
 8.     NodeType* find(int key): search tree and return pointer to node with desired key
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
        std::array<bool,NodeType::maxChildren()> modified{};
        bool deleted;


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

    
    public:
        SafeNode(const SafeNode& other) = delete;
        SafeNode& operator=(const SafeNode&) = delete;
        SafeNode(){};
        ~SafeNode() {

            if (deleted) {
                delete copy;
            }

            if (conn_point.copyWasConnected()) {
                delete original;
            }

           
        }
        SafeNode(ConnPoint<NodeType>& _conn_point, NodeType* _original): 
        conn_point(_conn_point), original(_original),
        deleted(false) {

            // constructor will only be called internally and
            // thus original is guaranteed non-Null
            copy = new NodeType();
            *copy = *original;
            // keep old reference and new reference
            // for validation
            //conn_point.hash_insert(original);
            conn_point.add_to_copied_nodes(this);
            
            for (int i = 0; i < NodeType::maxChildren(); i++)
            {
                children[i] = nullptr;
            }

            
        };



        static constexpr int children_length() {
            return NodeType::maxChildren();
        }

        // getOriginal: used to access original
        // node (any modifications are not guaranteed thread safe)
        NodeType* getOriginal() const {
            return original;
        }

        NodeType peekOriginal() const {
            return *original;
        }

        // safeRef: get reference to safe copy of node,
        // can be modified freely
        // as only one thread has access to it
        NodeType* safeRef() {
            return copy;
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
            
            
            //child has already been copied
            if (modified[child_pos]) {
                return children[child_pos];
            }

            // child has been modified
            modified[child_pos] = true;

            SafeNode<NodeType>* new_safe = nullptr;

            const auto original_child = copy->getChild(child_pos);

            //child not copied yet,
            //copy and use the copy from now on
            if (original_child) {
                new_safe = new SafeNode<NodeType>(conn_point , original_child);
                children[child_pos] = new_safe;
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

            // copy has happened
            modified[child_pos] = true;

            auto child = children[child_pos];

            if (!child) {
                children[child_pos] = newVal;
                copy->setChild(child_pos, newVal? newVal->safeRef() : nullptr);
                return nullptr;
            }

            // update children of safe node
            // old node is deleted
            SafeNode<NodeType>* old_node = move(children[child_pos], newVal);
            
            // connect copy pointer, replace with new
            copy->setChild(child_pos, newVal? newVal->safeRef() : nullptr);

            return old_node;;
        }

        
        // setNewChild: sets the child with index child_pos
        // to be a new SafeNode.
        // Accepts a pointer to an unsafe node.
        // Returns pointer to deleted node to
        // be used for manual memory management (eg. deleting the entire subtree)
        SafeNode<NodeType>* setNewChild(int child_pos, NodeType* newVal) {
            // no oob errors
            assert(child_pos >= 0 && child_pos < NodeType::maxChildren());

            // copy has happened
            modified[child_pos] = true;

            auto child = children[child_pos];

            SafeNode<NodeType>* new_safe_node = nullptr;

            if (newVal) {
                // update children of safe node
                // by updating child unique pointer
                // old SafeNode is deleted
                new_safe_node = conn_point.create_safe(newVal);
            } 

            if (!child) {
                children[child_pos] = new_safe_node;
                copy->setChild(child_pos, new_safe_node? new_safe_node->safeRef() : nullptr);
                return nullptr;
            }

            // update children of safe node
            // old node is deleted
            SafeNode<NodeType>* old_node = move(children[child_pos], new_safe_node);;


            // connect copy pointer, replace with new
            copy->setChild(child_pos, new_safe_node? new_safe_node->safeRef() : nullptr);

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

    ConnectionPointer(NodeType* snapshot, int child_index): snapshot(snapshot),child_index(child_index) {}
}

template<class NodeType>
struct ConnPointData {
    using Stack = TreePathStackWithIndex<NodeType>;

    // the node to become the connection point
    NodeType* connection_point;

    // the root of the structure
    // outside the recursive datastructure
    NodeType** root_of_structure;

    // all the data for the pointer to be changed
    ConnectionPointer con_ptr;

    // path to, not including, the node
    // with the pointer to be changed
    Stack& path;

    // reference to a global, fallback lock
    TSX::SpinLock &lock;
    
    // transactional stats
    TSX::TSXStats &stats;

    ConnPointData(NodeType* connection_point, NodeType** root_of_structure,  
                  ConnectionPointer con_ptr, Stack& path, TSX::SpinLock &lock, TSX::TSXStats &stats):
                  connection_point(connection_point), root_of_structure(root_of_structure), 
                  con_ptr(con_ptr), path(path), lock(lock), stats(stats) {}
}

template <class NodeType>
class ConnPoint
{
    using Stack = TreePathStackWithIndex<NodeType>;

    private:
        bool& connect_result;
        std::deque<NodeData<NodeType> > copied_nodes;

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
        NodeType* conn_point_copy;


        // if a node is popped from path,
        // to be set as the new connection point,
        // the old head will be its child at index
        // child_to_exchange
        int child_to_exchange;

        // path to connection point
        Stack& path_to_conn_point;

        // copy was successfully connected
        bool copy_connected;

        // flag to determine if any modification needs to
        // be made to original tree
        bool tree_was_modified;

        // The global lock
        TSX::SpinLock& _lock;

        // tsx stats
        TSX::TSXStats &_stats;


        // the root node of the copied tree
        SafeNode<NodeType>* head;


        
        // lookup but returns node, nullptr when not found
        bool connPointConnected() {
            const auto conn_point_key = connection_point->getKey();

            auto should_be_conn_point = (*_root)->find(conn_point_key);

            if (should_be_conn_point != connection_point) {
               // std::cout << "should be " << should_be_conn_point << " is " << connection_point << std::endl;
            }

            return should_be_conn_point == connection_point;
        }



    public:
        // no copying or moving
        ConnPoint& operator=(const ConnPoint&) = delete;
        ConnPoint(const ConnPoint&) = delete;

        ConnPoint(ConnPointData data, bool& connected_successfully):
        connect_result(connected_successfully),
        connection_point(data.connection_point), connection_pointer(nullptr),
        _root(data.root_of_structure),
        conn_point_copy(data.con_ptr.snapshot),
        child_to_exchange(data.con_ptr.child_index),
        path_to_conn_point(path), 
        copy_connected(false),
        tree_was_modified(false),
        _lock(data.lock),
        _stats(data.stats),
        head(wrap_safe(conn_point_copy)),
        {
            if (child_to_exchange != -1) {
                connection_pointer =  &((*connection_point).getChildren()[child_to_exchange]);
            } else {
                connection_pointer = nullptr;
            }

            // init at not connected
            connect_result = false;
        };

        ~ConnPoint() {

            // if ConnPoint is deleted
            // but its copy
            // isn't connected
            // everything should be cleaned up
            

            // in transaction
            connect_result = connect_atomically();
            
            // CLEANUP DISABLED

            // const bool clean_all = !copy_connected;

            // for (auto it = copied_nodes.begin(); it != copied_nodes.end(); ++it)  {
            //     it->safeNode->deleted = clean_all;
            //     delete it->safeNode;
            // }

        }

        bool connect_atomically() {
            unsigned char err_status = 0;
            
            TSX::TSXGuardWithStats guard(0,_lock,err_status,_stats);

            if (!TSX::transaction_pending() && !_lock.isLocked() && err_status != VALIDATION_FAILED) {
                std::cerr << "ERR:TRANS DID NOT START" << std::endl;
                exit(-1);
            }

            if (err_status != VALIDATION_FAILED) {
                validate_copy();
                connect_copy();
                return true;
            }

            return false;

        }

        NodeType* getConnPoint() {
            return connection_point;
        }


        // create_safe: use to create an entirely new SafeNode not based on an original node,
        // returns nullptr if nullptr is given.
        SafeNode<NodeType>* create_safe(NodeType* some_node) {
            if (!some_node) {
                return nullptr;
            }

            auto newNode = new SafeNode<NodeType>(*this, some_node);

            for (auto elem = newNode->modified.begin(); elem != newNode->modified.end(); elem++ ) {
                    *elem = true;
            }
            
            return newNode;  
        }

        // wrap_safe: wrap a node in a SafeNode,
        // returns nullptr if nullptr is given.
        SafeNode<NodeType>* wrap_safe(NodeType* some_node) {
            if (!some_node) {
                return nullptr;
            }
            
            return new SafeNode<NodeType>(*this, some_node);
        }


        // safeTreeAtRoot: Wraps root node in a SafeNode and sets it as the head of the copied tree.
        // Useful for creating an entirely new sub-tree at tree root level.
        // Returns new head.
        SafeNode<NodeType>* safeTreeAtRoot() {

            assert(_root);

            tree_was_modified = true;
            
            head = wrap_safe(*_root);
            return head;
        }

        /*
        // newSafeTreeAt: Wraps newly created node in a SafeNode and sets it to be
        // connected as child with index child_pos of the connection point.
        // Useful for creating an entirely new sub-tree.
        // Returns new head.
        SafeNode<NodeType>* newSafeTreeAt(int child_pos, NodeType* node) {

            tree_was_modified = true;

            // also set connection pointer
            connection_pointer = &((*connection_point).getChildren()[child_pos]);

            child_to_exchange = child_pos;

            head = !node  ? nullptr : create_safe(node);

            return head;
        }

        */

        // insertNewHeadAt: Wraps newly created node in a SafeNode and sets it to
        // be connected as the root node of the original tree.
        // Useful for creating an entirely new tree.
        // Returns new head.
        SafeNode<NodeType>* createNewTree(NodeType* node) {

            tree_was_modified = true;

            // also set connection pointer
            connection_pointer = nullptr;

            head = !node  ? nullptr : create_safe(node);

            return head;
        }



        

        /*
        // Wraps child of connection point in a SafeNode and sets it as the head of the copied tree.
        // Useful to create copy of pre-existing tree. 
        // Child should be not null and connection_point should also be not null.
        // Returns head on success, nullptr if the connection point is null or the child is null.
        SafeNode<NodeType>* setChildAsHead(int child_pos) {

            tree_was_modified = true;

            assert(child_pos < NodeType::maxChildren());
            
            // head not modified
            if (!connection_point) {
                return nullptr;
            }

            // also set connection pointer
            connection_pointer = &((*connection_point).getChildren()[child_pos]);

            child_to_exchange = child_pos;

            head = wrap_safe(conn_point_copy);

            return head;
        }

        SafeNode<NodeType>* copyChild(int child_pos) {
            assert(connection_point);
            return wrap_safe(conn_point_copy);
        }

        */

         SafeNode<NodeType>* setHead(SafeNode<NodeType>* newSafeNode) {
            tree_was_modified = true;
            head = newSafeNode;
            return head;
        } 
        

        // get head
        SafeNode<NodeType>* getHead() {
            return head;
        }

       



        bool copyWasConnected() {
            return copy_connected;
        }


        void add_to_copied_nodes(SafeNode<NodeType>* a_node) {
            // create an array of previous children and add to
            // "vector" of nodes

            NodeData<NodeType> saved_node;

            NodeType** orig_children = a_node->copy->getChildren();

            for (int i = 0; i < NodeType::maxChildren(); i++) {
                saved_node.childrenSnapShot[i] = orig_children[i];
            }

            saved_node.safeNode = a_node;


            copied_nodes.push_back(saved_node);
        }

        // connect created tree copy by exchanging proper parent pointer
        void connect_copy() {

            // if tree wasn't modified 
            // no need to do anything
            if (!tree_was_modified) {
                return;
            }

            NodeType* copied_tree_head = head ? head->safeRef() : nullptr; 

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
                if (conn_point_copy != *_root) {
                   
                    // std::cout << "root was " <<  conn_point_copy << " with key " <<
                    // (conn_point_copy? conn_point_copy->getKey():-1) <<
                    // " and now is " << *_root << " with key " << (*_root ? (*_root)->getKey(): -1) <<  std::endl;

                    TSX::TSXGuard::abort<VALIDATION_FAILED>();
                    return false;
                }
            } else {
                // if (head->getOriginal()->getKey() == 4) {
                //      std::cout << "ORIGINAL CONN POINT:" << conn_point_copy << " NOW CONN POINT:" 
                //     << *_root << " on key " << head->getOriginal()->getKey() << std::endl;
                // }
                
                if (conn_point_copy != connection_point->getChild(child_to_exchange)) {
                    // std::cout << conn_point_copy << " different from " << connection_point->getChild(child_to_exchange) << std::endl;
                    TSX::TSXGuard::abort<VALIDATION_FAILED>();
                    return false;
                }

                // 1. check if root of copied tree is still connected
                if (!connPointConnected()) {
                    TSX::TSXGuard::abort<VALIDATION_FAILED>();
                    return false;
                }

                // // 2. check if connection point children are unchanged
                // for (int i = 0; i < NodeType::maxChildren(); i++) {
                //     if (conn_point_children[i] != connection_point->getChild(i)) {
                //         TSX::TSXGuard::abort<VALIDATION_FAILED>();
                //         return false;
                //     }
                // } 
            }


            

            // if no changes done leave
            if (!tree_was_modified) {
                std::cerr << "not mod" << std::endl;
                return true;
            }


            //3. check children of copied nodes
            for (auto it = copied_nodes.begin(); it != copied_nodes.end(); ++it) {


                    // the original node and its current children
                    const auto original_node = it->safeNode->original;
                    auto current_children = original_node->getChildren();

                    // the saved children which shouldn't have changed
                    auto saved_children = it->childrenSnapShot;

                    // if (it->safeNode && it->safeNode->original->getKey() == 101) {
                    //     for (int i = 0; i < NodeType::maxChildren(); i++) {
                    //         std::cout << "child of 101 is " << (current_children[i] ? current_children[i]->getKey() : -1) << " and saved child is " 
                    //         << (saved_children[i] ? saved_children[i]->getKey() : -1) << std::endl;
                    //         std::cout << "101 is " << it->safeNode->original << std::endl;
                    //     }
                    // }

                    for (int i = 0; i < NodeType::maxChildren(); i++) {
                        if (saved_children[i] != current_children[i]) {
                            //std::cout << "wrong child"<<std::endl;
                            TSX::TSXGuard::abort<VALIDATION_FAILED>();
                            return false;
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
            NodeType* prev_connection_point;

            int popped_node_next_index;
            

            // pop node
            std::tie(popped_node,popped_node_next_index) = path_to_conn_point.pop();

            // keep old connection point
            prev_connection_point = connection_point;
            
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
            SafeNode<NodeType>* newHead = wrap_safe(prev_connection_point);

            // and connect it
            newHead->setChild(child_to_exchange, head);
            
            
            // now head can be updated
            head = newHead; 
            // as well which child to update for the next pop
            child_to_exchange = next_child_index;

            return head;
        }


};

#endif
