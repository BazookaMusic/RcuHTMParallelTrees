#ifndef GENERIC_TREE_HPP
    #define GENERIC_TREE_HPP

#include <tuple>


template <class T>
class SearchTreeNode
{
    public:
        int key;
        T val;
    
    public:
        SearchTreeNode(){};

};

template <class T>
class SearchTree
{
    private:
        SearchTreeNode<T> *root;

    public:
        SearchTree(): root(nullptr){};

        virtual SearchTreeNode<T> *getChild(int i) = 0;
        virtual void setChild(int i, SearchTreeNode<T> *newNode) = 0;
        //returns number of children
        virtual int *nChildren(int i) = 0;

};



const int STACK_DEPTH = 64;
class StackOverflowError {
    private:
    int maxDepth, attemptedDepth;

    public:
    StackOverflowError(int maxDepth, int attemptedDepth) {
        this->maxDepth = maxDepth;
        this->attemptedDepth = attemptedDepth;
        };
};



template <class NodeType>
class SearchTreeStack{
 private:
        NodeType **stack;
        int currentIndex; 
 public:
        //SearchTreeStack used to traverse tree structures
        SearchTreeStack() {
                this->stack = new NodeType*[STACK_DEPTH];
                this->currentIndex = -1;
            };
        
        ~SearchTreeStack(){
            delete [] stack;
        }

        NodeType *bottom() {
            if (currentIndex == -1) {
                return nullptr;
            }

            return stack[0];
        }
        

        //push pushes a node into the stack
        void push(NodeType *node) {
            currentIndex++;

            if (currentIndex > STACK_DEPTH) {
                throw StackOverflowError(currentIndex,STACK_DEPTH);
            }

            stack[currentIndex] = node;
        };
        //pop removes an element from the top of the stack
        //and returns the element
        NodeType *pop() {
            if (currentIndex < 0) {
                return nullptr;
            } 

            return stack[currentIndex--];
        };
        //Empty returns true if stack is empty, else false
        bool Empty() {
            return currentIndex == -1;
        };

};



// template <class NodeType>
// class SearchTreeStackWithIndex{
//     private:
//         NodeType **stack;
//         int currentIndex; 
//     public:
//         //SearchTreeStack used to traverse tree structures
//         SearchTreeStackWithIndex() {
//                 this->stack = new std::pair<NodeType*,int>[STACK_DEPTH];
//                 this->currentIndex = -1;
//             };
        
//         ~SearchTreeStackWithIndex(){
//             delete [] stack;
//         }

//         inline std::pair<NodeType*,int> *bottom() {
//             if (currentIndex == -1) {
//                 return nullptr;
//             }

//             return stack[0];
//         }
        

//         //push pushes a node into the stack
//         //and stores the index of the next child
//         inline void push(NodeType *node, int index) {
//             currentIndex++;

//             if (currentIndex > STACK_DEPTH) {
//                 throw StackOverflowError(currentIndex,STACK_DEPTH);
//             }

//             stack[currentIndex] = std::pair<NodeType*,int>(node,index);
//         };
//         //pop removes an element from the top of the stack
//         //and returns the element
//         std::pair<NodeType*,int> pop() {
//             if (currentIndex < 0) {
//                 return std::pair<NodeType*,int>(nullptr,-1);
//             } 

//             return stack[currentIndex--];
//         };
//         //Empty returns true if stack is empty, else false
//         bool Empty() {
//             return currentIndex == -1;
//         };

// };


#endif