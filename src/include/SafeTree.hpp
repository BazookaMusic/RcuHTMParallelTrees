#ifndef SAFE_TREE_HPP
    #define SAFE_TREE_HPP


#include <memory>

#include <tuple>
#include <iostream>
#include <deque>

#include "generic_tree.hpp"

/* SafeTrees require Nodes with the following methods
 1. A default empty constructor
 2. A copy constructor
 3. NodeType* getChild(int i)
 4. void setChild(int i, NodeType * n)
 5. int nextChild(int key) : returns index of pointer to follow to find key k
 6. int nChildren() : amount of children per node
*/



template <class NodeType>
class InsPoint;


template <class NodeType>
class SafeNode
{
    friend class InsPoint<NodeType>;
    private:
        InsPoint<NodeType>& ins_point;
        NodeType* const original;
        NodeType* copy;
        const int nChildren;
        SafeNode** const children;
        bool deleted;

        
        void node_delete() {
            if (!this) {
                return;
            }

            delete copy;
            ~SafeNode();
        }

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

            for (int i = 0; i < root->nChildren; i++)
            {
                sub_tree_delete(root->children[i]);
            }
            
            root->deleted = true;
            delete root;
        }

    
    public:
        SafeNode(){};
        ~SafeNode() {
            delete [] children;

            if (deleted) {
                delete copy;
            }
        }
        SafeNode(InsPoint<NodeType>& _ins_point, NodeType* _original): 
        ins_point(_ins_point), original(_original),
        nChildren(original->nChildren()), children(new SafeNode*[nChildren]),
        deleted(false) {
            if (original) {
                copy = new NodeType();
                *copy = *original;
                // keep old reference and new reference
                // for validation
                //ins_point.hash_insert(original);
                ins_point.add_to_copied_nodes(this);
            }

            for (int i = 0; i < nChildren; i++)
            {
                children[i] = nullptr;
            }

            
        };



        int children_length() {
            return nChildren;
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
            assert(child_pos >= 0 && child_pos < nChildren);
            
            
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
        std::unique_ptr<SafeNode<NodeType>> setChild(int child_pos, SafeNode<NodeType>* newVal) {
            // no oob errors
            assert(child_pos >= 0 && child_pos < nChildren);

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

            return old_node? std::unique_ptr<SafeNode<NodeType>>(old_node): nullptr;;
        }

        
        // setNewChild: sets the child with index child_pos
        // to be a new SafeNode.
        // Accepts a pointer to an unsafe node.
        // Returns unique pointer to deleted node to
        // be used for manual memory management (eg. deleting the entire subtree)
        std::unique_ptr<SafeNode<NodeType>> setNewChild(int child_pos, NodeType* newVal) {
            // no oob errors
            assert(child_pos >= 0 && child_pos < nChildren);

            auto child = children[child_pos];

            SafeNode<NodeType>* new_safe_node = nullptr;

            if (newVal) {
                // update children of safe node
                // by updating child unique pointer
                // old SafeNode is deleted
                new_safe_node = make_safe(newVal);
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

            return old_node? std::unique_ptr<SafeNode<NodeType>>(old_node): nullptr;;

        }

        // clipTree: delete entire subtree with
        // the child at position child_pos as root
        void clipTree(int child_pos) {
            assert(child_pos >= 0 && child_pos < nChildren);

            if (children[child_pos]) {
                children[child_pos]->subtreeDelete();
            }

            children[child_pos] = nullptr;
        }
        
        // subtreeDelete: delete subtree rooted at this node
        void subtreeDelete() {
            sub_tree_delete(this); 
        }

        // make_safe: use to create a SafeNode from a normal node,
        // returns nullptr if nullptr is given.
        SafeNode<NodeType>* make_safe(NodeType* some_node) {
            if (!some_node) {
                return nullptr;
            }

            return new SafeNode<NodeType>(ins_point, some_node); 
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
class InsPoint
{
    typedef SearchTreeStack<NodeType> Stack;
    using NodePair = std::pair<SafeNode<NodeType>*,NodeType>;

    private:
        std::deque<NodePair> copied_nodes;
        SafeNode<NodeType>* head;
        int child_to_exchange;
        Stack& path_to_ins_point;


    public:
        InsPoint(NodeType* ins_node, Stack& path): 
        head(new SafeNode<NodeType>(*this, ins_node)),
        path_to_ins_point(path)
        {};

        ~InsPoint() {
            //cleanup_tree(head);
            for (auto it = copied_nodes.begin(); it != copied_nodes.end(); ++it)  {
                delete it->first;
            }

        }
        

        // get head
        SafeNode<NodeType>* get_head() {
            return head;
        }

        

        /*
        void hash_insert(NodeType* orig) {
            orig_copy_map.insert(std::make_pair(orig,*orig)); 

        } */

        void add_to_copied_nodes(SafeNode<NodeType>* a_node) {
            copied_nodes.push_back(std::make_pair(a_node, *(a_node->original)));
        }

        // connect created tree copy by exchanging proper parent pointer
        void connect_copy(NodeType* ins_point_node, NodeType** tree_root) {
            if (path_to_ins_point.Empty()) {
                // root should be exchanged
                *tree_root = head->safeRef(); 
            } else {
                // get which pointer to swap
                int child_pointer_to_swap = ins_point_node->next_child(head->key());

                // swap it
                ins_point_node->setChild(child_pointer_to_swap, head->safeRef());
            }
        }

        SafeNode<NodeType>* pop_path() {
            // empty path return nullptr
            // to signify that root has been reached
            if (path_to_ins_point->Empty()) {
                return nullptr;
            }

            // pop the node
            NodeType* prev_node;
            

            // unpack pair
            prev_node = path_to_ins_point->pop();

            // wrap it
            SafeNode<NodeType>* now_safe = new SafeNode<NodeType>(*this, prev_node);

            // connect it
            now_safe->setChild(now_safe->next_child(head->key()), head);

            // head changes to prev node
            head = now_safe;
        }

        

};

#endif