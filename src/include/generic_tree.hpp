#ifndef GENERIC_TREE_HPP
    #define GENERIC_TREE_HPP

#include <tuple>
#include <deque>
#include <sstream>



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
class StackOverflowError: public std::exception {
    private:
     int maxDepth, attemptedDepth;

    public:
     StackOverflowError(int maxDepth, int attemptedDepth): maxDepth(maxDepth),attemptedDepth(attemptedDepth) {};

	 const char * what () const throw ()
     {
         std::ostringstream stringStream;
         stringStream << "Stack Overflow: Max Depth was:" << maxDepth << " and tried to insert " << attemptedDepth << " elements";
        
     	 return stringStream.str().data();
     }

    
};

template <class NodeType>
class TreePathStack{
 private:
        NodeType* stack[STACK_DEPTH];
        int currentIndex; 
 public:
        //TreePathStack used to traverse tree structures
        TreePathStack(): currentIndex(-1) {};
        
        ~TreePathStack(){
        }

        NodeType *bottom() {
            if (currentIndex == -1) {
                return nullptr;
            }

            return stack[0];
        }

        NodeType** stack_contents() {
            return stack;
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

/*

Infinite stack
for when path size is not enough 

*/
template <class NodeType>
class NodeStack{
 private:
        std::deque<NodeType*> stack;
        int currentIndex; 
 public:
        //TreePathStack used to traverse tree structures
        NodeStack(): currentIndex(-1) {};
        

        NodeType* bottom() {
            if (currentIndex == -1) {
                return nullptr;
            }

            return stack[0];
        }
        

        //push pushes a node into the stack
        void push(NodeType *node) {
            stack[++currentIndex] = node;
        };
        //pop removes an element from the top of the stack
        //and returns the element
        NodeType* pop() {
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




/* NOT NEEDED YET
template <class NodeType>
class TreePathStackWithIndex{
    private:
        NodeType **stack;
        int currentIndex; 
    public:
        //TreePathStack used to traverse tree structures
        TreePathStackWithIndex() {
                this->stack = new std::pair<NodeType*,int>[STACK_DEPTH];
                this->currentIndex = -1;
            };
        
        ~TreePathStackWithIndex(){
            delete [] stack;
        }

        inline std::pair<NodeType*,int> *bottom() {
            if (currentIndex == -1) {
                return nullptr;
            }

            return stack[0];
        }
        

        //push pushes a node into the stack
        //and stores the index of the next child
        inline void push(NodeType *node, int index) {
            currentIndex++;

            if (currentIndex > STACK_DEPTH) {
                throw StackOverflowError(currentIndex,STACK_DEPTH);
            }

            stack[currentIndex] = std::pair<NodeType*,int>(node,index);
        };
        //pop removes an element from the top of the stack
        //and returns the element
        std::pair<NodeType*,int> pop() {
            if (currentIndex < 0) {
                return std::pair<NodeType*,int>(nullptr,-1);
            } 

            return stack[currentIndex--];
        };
        //Empty returns true if stack is empty, else false
        bool Empty() {
            return currentIndex == -1;
        };

};

*/


#endif