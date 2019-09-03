#ifndef SAFE_TREE_HPP
    #define SAFE_TREE_HPP


#include <memory>

#include "generic_tree.hpp"


template <class NodeType>
class SafeNode
{
    private:
        NodeType *original, *copy;
        int nChildren;
        SafeNode **children;
    
    public:
        SafeNode(){};
        ~SafeNode() {
            delete [] children;
        }
        SafeNode(NodeType *_original): original(_original), 
        nChildren(original->nChildren()), children(new SafeNode*[nChildren]){
            if (original) {
                copy = new NodeType();
                *copy = *original;
            }

            for (int i = 0; i < nChildren; i++)
            {
                children[i] = nullptr;
            }
             
        };

        //getOriginal used to access original
        //node (any modifications are not guaranteed thread safe)
        inline NodeType *getOriginal() {
            return original;
        }

        //get reference to safe copy of node,
        //can be modified freely
        //as only one thread has access to it
        inline NodeType *safeRef() {
            return copy;
        }


        //getChild, returns either an already linked SafeNode
        //or creates a new SafeNode when accessing an unsafe node
        inline SafeNode<NodeType> *getChild(int i) {
            // no oob errors
            assert(i > 0 && i < nChildren);

            //child has already been copied
            if (children[i]) {
                return children[i];
            }

            //child not copied yet,
            //copy and use the copy from now on
            children[i] = new SafeNode<NodeType>(original->getChild(i));
            return children[i];
        }

        //getChild, returns either an already linked SafeNode
        //or creates a new SafeNode when accessing an unsafe node
        inline SafeNode<NodeType> *setChild(int i,SafeNode<NodeType> *newVal) {
            // no oob errors
            assert(i > 0 && i < nChildren);

            //child not copied yet,
            //copy and use the copy from now on
            children[i] = new SafeNode<NodeType>(newVal);
        }




};


//will be used
template <class NodeType>
class InsPoint
{
    typedef SearchTreeStack<NodeType> Stack;

    private:
    SafeNode<NodeType> *head;
    NodeType **insPoint;
    int child_to_exchange;
    std::unique_ptr<SearchTreeStack<NodeType>> path_to_ins_point;


    public:
        InsPoint(NodeType **someNode, std::unique_ptr<Stack>& path): insPoint(someNode) {
            assert(someNode);

            if (*someNode) {
                head = new SafeNode<NodeType>(*insPoint);
            }
            path_to_ins_point = std::move(path);
        };

        SafeNode<NodeType> *getHead() {
            return head;
        }


        void connectCopy() {    
            if (insPoint && head) {
                // exchange old with new
                *insPoint = head->safeRef();
            }
        }




};

#endif