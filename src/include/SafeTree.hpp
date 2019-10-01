#ifndef SAFE_TREE_HPP
    #define SAFE_TREE_HPP


#include <memory>

#include <tuple>
#include <iostream>
#include <deque>

#include "generic_tree.hpp"

/* SafeTrees require Nodes with the following methods
 1.     A default empty constructor
 2.     A copy constructor
 3.     NodeType* getChild(int i)
 4.     void setChild(int i, NodeType * n)
 5.     int nextChild(int key) : returns index of pointer to follow to find key k
 6.     NodeType** getChildren(): returns pointer to array of children,
 7.     static constexpr int maxChildren(): max amount of children for each node, 
 8.     bool operator == (const NodeType &ref) const: an equality operator to compare nodes (can check for equality only on mutable values)
 9.     bool operator == (const T &ref) const: an equality operator to compare values
*/



template <class NodeType>
class ConnPoint;


template <class NodeType>
class SafeNode
{
    friend class ConnPoint<NodeType>;
    private:
        ConnPoint<NodeType>& ins_point;
        NodeType* const original;
        NodeType* copy;
        SafeNode** const children;
        bool deleted;


        // mark previous for deletion and move new value
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
        SafeNode(){};
        ~SafeNode() {
            delete [] children;

            if (deleted) {
                delete copy;
            }

            if (ins_point.copyWasConnected()) {
                delete original;
            }
        }
        SafeNode(ConnPoint<NodeType>& _ins_point, NodeType* _original): 
        ins_point(_ins_point), original(_original),
        children(new SafeNode*[NodeType::maxChildren()]),
        deleted(false) {

            // constructor will only be called internally and
            // thus original is guaranteed non-Null
            copy = new NodeType();
            *copy = *original;
            // keep old reference and new reference
            // for validation
            //ins_point.hash_insert(original);
            ins_point.add_to_copied_nodes(this);
            
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
        NodeType* getOriginal() {
            return original;
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
            if (children[child_pos]) {
                return children[child_pos];
            }

            SafeNode<NodeType>* new_safe = nullptr;

            const auto original_child = original->getChild(child_pos);

            //child not copied yet,
            //copy and use the copy from now on
            if (original_child) {
                new_safe = new SafeNode<NodeType>(ins_point , original_child);
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

            auto child = children[child_pos];

            if (!child) {
                children[child_pos] = newVal;
                copy->setChild(child_pos, newVal? newVal->safeRef() : nullptr);
                return nullptr;
            }

            SafeNode<NodeType>* old_node = child;

            // update children of safe node
            // old node is deleted
            move(children[child_pos], newVal);

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

            auto child = children[child_pos];

            SafeNode<NodeType>* new_safe_node = nullptr;

            if (newVal) {
                // update children of safe node
                // by updating child unique pointer
                // old SafeNode is deleted
                new_safe_node = ins_point.make_safe(newVal);
            } 

            if (!child) {
                children[child_pos] = new_safe_node;
                copy->setChild(child_pos, new_safe_node? new_safe_node->safeRef() : nullptr);
                return nullptr;
            }

            SafeNode<NodeType>* old_node = child;

            // update children of safe node
            // old node is deleted
            move(children[child_pos], new_safe_node);

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
class ConnPoint
{
    typedef TreePathStack<NodeType> Stack;

    private:
        std::deque<NodeData<NodeType>> copied_nodes;

        // the root node of the copied tree
        SafeNode<NodeType>* head;


        // the connection point is the node whose child pointer will
        // be modified to connect the copied tree
        NodeType* connection_point;
        // if null, the insertion should be done at
        // the root of the data structure

        // the point of insertion of the new tree
        NodeType** connection_pointer;

        // the root of the data structure to modify
        NodeType** _root;


        // if a node is popped from path,
        // to be set as the new connection point,
        // the old head will be its child at index
        // child_to_exchange
        int child_to_exchange;

        // path to connection point
        Stack& path_to_ins_point;

        // copy was successfully connected
        bool copy_connected;



    public:
        // no copying or moving
        ConnPoint& operator=(const ConnPoint&) = delete;
        ConnPoint(const ConnPoint&) = delete;

        explicit ConnPoint(NodeType* ins_node, NodeType** root_of_structure, Stack& path): 
        head(nullptr),
        connection_point(ins_node), connection_pointer(nullptr),
         _root(root_of_structure),
        path_to_ins_point(path), 
        copy_connected(false)
        {};

        ~ConnPoint() {

            // if ConnPoint is deleted
            // but its copy
            // isn't connected
            // everything should be cleaned up
            const bool clean_all = !copy_connected;

            for (auto it = copied_nodes.begin(); it != copied_nodes.end(); ++it)  {
                it->safeNode->deleted = clean_all;
                delete it->safeNode;
            }

        }

        NodeType* getConnPoint() {
            return connection_point;
        }


        // make_safe: use to create a SafeNode from a normal node,
        // returns nullptr if nullptr is given.
        SafeNode<NodeType>* make_safe(NodeType* some_node) {
            if (!some_node) {
                return nullptr;
            }

            return new SafeNode<NodeType>(*this, some_node); 
        }


        // newHead: Wraps node in a SafeNode and sets it as the head of the copied tree.
        // Useful for creating an entirely new sub-tree at tree root level.
        // Returns new head.
        SafeNode<NodeType>* setRootAsHead() {

            assert(_root);

            head = make_safe(*_root);
            return head;
        }

        // insertNewHeadAt: Wraps node in a SafeNode and sets it to be
        // connected as child with index child_pos of the connection point.
        // Useful for creating an entirely new sub-tree.
        // Returns new head.
        SafeNode<NodeType>* insertNewHeadAt(int child_pos, NodeType* node) {

            // also set connection pointer
            connection_pointer = &((*connection_point).children[child_pos]);

            child_to_exchange = child_pos;

            head = !node  ? nullptr : new SafeNode<NodeType>(*this, node);

            return head;
        }


        // Wraps child of connection point in a SafeNode and sets it as the head of the copied tree.
        // Useful to create copy of pre-existing tree. 
        // Child should be not null and connection_point should also be not null.
        // Returns head on success, nullptr if the connection point is null or the child is null.
        SafeNode<NodeType>* setChildAsHead(int child_pos) {

            assert(child_pos < NodeType::maxChildren());
            
            // head not modified
            if (!connection_point) {
                return nullptr;
            }

            return insertNewHeadAt(child_pos, connection_point->getChild(child_pos));
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

            std::array<NodeType*, NodeType::maxChildren()> children{};

            NodeType** orig_children = a_node->original->getChildren();

           

            for (int i = 0; i < NodeType::maxChildren(); i++) {
                children[i] = orig_children[i];
            }

            saved_node.childrenSnapShot = children;
            saved_node.safeNode = a_node;



            copied_nodes.push_back(saved_node);
        }

        // connect created tree copy by exchanging proper parent pointer
        void connect_copy() {
            NodeType* copied_tree_head = !head ? head->safeRef() : nullptr; 

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
            // procedure:
            // 1. check if root of copied tree is still connected
            // 2. 
            
            // SHOULD ABORT TRANSACTIONS

            // 1. check if root of copied tree is still connected
            if (connection_point && !(*_root)->find(connection_point->key())) {
                return false;
            }



            // children of modified nodes should not have changed
            for (auto it = copied_nodes.begin(); it != copied_nodes.end(); ++it) {

                    // the original node and its current children
                    const auto original_node = it->safeNode->original;
                    auto current_children = original_node->getChildren();

                    // the saved children which shouldn't have changed
                    auto saved_children = it->childrenSnapShot;

                    for (int i = 0; i < NodeType::maxChildren(); i++) {
                        if (saved_children[i] != current_children[i]) {
                            return false;
                        }
                    }
                
            }

            // all checks ok
            return true;
        }

        SafeNode<NodeType>* pop_path() {

            // connection point should be not null
            // to pop, else it makes no sense
            assert(connection_point);

            // empty path return nullptr
            // to signify that root has been reached
            if (path_to_ins_point->Empty()) {

                // no other nodes in path so:

                // The connection point is the root
                // of the tree instead of a node
                connection_point = nullptr;


                return nullptr;
            }

            // pop the node
            NodeType* popped_node, prev_connection_point;
            

            // pop node
            popped_node = path_to_ins_point->pop();

            // keep old connection point
            prev_connection_point = connection_point;
            
            // calculate which child should be changed to insert copied tree
            // by using the previous connection point's key
            const int next_child_index = popped_node->next_child(prev_connection_point->key());


            // change connection point to popped node
            connection_point = popped_node;

            // also update connection pointer
            connection_pointer = &((*connection_point).children[next_child_index]);
            

            // now for the head of the tree

            // keep old head
            auto oldHead = head;

            // using the previous connection point
            // make a safeNode to turn to new head
            SafeNode<NodeType> newHead = make_safe(prev_connection_point);

            // for the previous head, calculate at which index
            // it should be inserted


            // and set it
            newHead->setChild(child_to_exchange, head);
            
            
            // now head can be updated
            head = newHead; 
            // as well which child to update for the next pop
            child_to_exchange = next_child_index;
        }


};

#endif