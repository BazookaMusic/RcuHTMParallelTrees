#ifndef SAFE_TREE_HPP
    #define SAFE_TREE_HPP


#include <memory>
#include <unordered_map>
#include <tuple>


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
    private:
        InsPoint<NodeType>& ins_point;
        NodeType* original;
        NodeType* copy;
        int nChildren;
        SafeNode** children;

        
        void deep_delete() {
            if (!this) {
                return;
            }

            delete copy;
            ~SafeNode();
        }

        // delete previous and move new value
        static void move(SafeNode<NodeType*>& to_be_replaced, SafeNode<NodeType*> replacement) {
            to_be_replaced.deep_delete();
            to_be_replaced = replacement;
        }
    
    public:
        SafeNode(){};
        ~SafeNode() {
            delete [] children;
        }
        SafeNode(InsPoint<NodeType>& _ins_point, NodeType* _original): 
        ins_point(_ins_point), original(_original),
        nChildren(original->nChildren()), children(new SafeNode*[nChildren]) {
            if (original) {
                copy = new NodeType();
                *copy = *original;
                // keep old reference and new reference
                // for validation
                ins_point.hash_insert(original);
            }

            for (int i = 0; i < nChildren; i++)
            {
                children[i] = nullptr;
            }

            
        };


        int children_length() {
            return nChildren;
        }

        //getOriginal used to access original
        //node (any modifications are not guaranteed thread safe)
        NodeType* getOriginal() {
            return original;
        }

        //get reference to safe copy of node,
        //can be modified freely
        //as only one thread has access to it
        NodeType* safeRef() {
            return copy;
        }


        //getChild, returns either an already linked SafeNode
        //or creates a new SafeNode when accessing an unsafe node
        SafeNode<NodeType>* getChild(int child_pos) {
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

            //child not copied yet,
            //copy and use the copy from now on
            if (original->getChild(child_pos)) {
                new_safe = new SafeNode<NodeType>(ins_point ,original->getChild(child_pos));
                children[child_pos] = new_safe;
            }
           

            return new_safe;
        }

        // setChild, sets the child with index child_pos
        // to be the SafeNode newVal.
        // Deletes previous node.
        void setChild(int child_pos, SafeNode<NodeType>* newVal) {
            // no oob errors
            assert(child_pos >= 0 && child_pos < nChildren);

            

            // update children of safe node
            // old node is deleted
            move(children[child_pos], newVal);

            // connect copy pointer, replace with new
            copy->setChild(child_pos,newVal? newVal->safeRef() : nullptr);
           
        }

        
        // setChild, sets the child with index child_pos
        // to be a new SafeNode.
        // Accepts a pointer to an unsafe node.
        // Deletes previous node.
        void setNewChild(int child_pos, NodeType* newVal) {
            // no oob errors
            assert(child_pos >= 0 && child_pos < nChildren);

            SafeNode<NodeType>* new_safe_node = nullptr;

            if (newVal) {
                // wrap
               

                // update children of safe node
                // by updating child unique pointer
                // old SafeNode is deleted
                new_safe_node = make_safe(newVal);
                move(children[child_pos], new_safe_node);
            }

            // connect copy pointer
            copy->setChild(child_pos, new_safe_node? new_safe_node->safeRef() : nullptr);
        }

        // use to create a SafeNode from a normal node,
        // returns nullptr if nullptr is given.
        SafeNode<NodeType> make_safe(NodeType* some_node) {
            if (!some_node) {
                return nullptr;
            }

            return new SafeNode<NodeType>(*this, some_node); 
        }

        // make copy of SafeNode
        SafeNode<NodeType>* make_copy(SafeNode<NodeType>* a_safe_node) {
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

    private:
        std::unordered_map<NodeType*,NodeType> orig_copy_map;
        SafeNode<NodeType>* head;
        int child_to_exchange;
        std::unique_ptr<Stack> path_to_ins_point;
        



    public:
        InsPoint(NodeType* ins_node, std::unique_ptr<Stack>& path): 
        orig_copy_map(std::unordered_map<NodeType*,NodeType>{}), head(new SafeNode<NodeType>(*this, ins_node))
        {
            path_to_ins_point = std::move(path);
        };
        

        // static void recursive_delete(SafeNode<NodeType>* a_node) {
        //     if (!a_node) {
        //         return;
        //     }

        //     for (int i = 0; i < a_node->children_length(); i++ ) {
        //         recursive_delete(a_node->children[i]);
        //     }

        //     delete a_node;
        // }

        // ~InsPoint() {
        //     recursive_delete(head);
        // }

        // get head
        SafeNode<NodeType>* get_head() {
            return head;
        }

        


        void hash_insert(NodeType* orig) {
            orig_copy_map.insert(std::make_pair(orig,*orig)); 

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