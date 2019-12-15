#ifndef GENERIC_TREE_HPP
    #define GENERIC_TREE_HPP

#include <tuple>
#include <deque>
#include <vector>
#include <sstream>
#include <string>


template <class T>
class SearchTreeNode
{
    public:
        int key;
        T val;
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



const int STACK_DEPTH = 1000;
class StackOverflowError: public std::exception {
    private:
     const char * err;

    public:
     StackOverflowError(int maxDepth, int attemptedDepth) {
        std::ostringstream stringStream;

        stringStream << "Stack Overflow: Max Depth was:" << maxDepth << " and tried to insert " << attemptedDepth << " elements";

        auto str = stringStream.str();

        err = str.c_str();


     };

	 const char * what() const throw ()
     {

         return err;

     }
    
};

template <class NodeType>
class TreePathStack{
 private:
        std::vector<NodeType*> stack;
        int currentIndex; 
 public:
        //TreePathStack used to traverse tree structures
        TreePathStack(): currentIndex(-1) {
            stack.reserve(STACK_DEPTH);
        };
        
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

            if (currentIndex > stack.capacity() - 1) {
                stack.push_back(node);
                return;
            }

            stack[currentIndex] = node;
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
        //peek returns a reference to the top
        //of the stack
        NodeType* peek() {
            if (currentIndex < 0) {
                return nullptr;
            } 

            return stack[currentIndex];
        };

        //Empty returns true if stack is empty, else false
        bool Empty() {
            return currentIndex == -1;
        };

};

template <class NodeType>
struct NodeAndNextPointer {
    NodeType* node;
    int next_child;
};

template <class NodeType>
class TreePathStackWithIndex{
    private:
        std::vector<NodeAndNextPointer<NodeType>> stack;
        int currentIndex; 
    public:
        //TreePathStack used to traverse tree structures
        TreePathStackWithIndex(): stack(std::vector<NodeAndNextPointer<NodeType>>{}), currentIndex(-1) {
                stack.reserve(STACK_DEPTH);
            };


        NodeAndNextPointer<NodeType> bottom() {
            if (currentIndex == -1) {
                return NodeAndNextPointer<NodeType>{nullptr,-1};
            }

            return stack[0];
        }

        NodeAndNextPointer<NodeType> top() {
            if (currentIndex == -1) {
                return NodeAndNextPointer<NodeType>{nullptr,-1};
            }

            return stack[currentIndex];
        }
        

        //push pushes a node into the stack
        //and stores the index of the next child
        void push(NodeType *node, int index) {
            currentIndex++;

            if (currentIndex != -1 && static_cast<unsigned long>(currentIndex) > stack.capacity() - 1) {
                stack.resize((stack.capacity() >> 1) + stack.capacity() + 1);
            }

            stack[currentIndex] = NodeAndNextPointer<NodeType>{node,index};
        };

        //pop removes an element from the top of the stack
        //and returns the element
        NodeAndNextPointer<NodeType> pop() {
            if (currentIndex < 0) {
                return NodeAndNextPointer<NodeType>{nullptr ,-1};
            } 

            return stack[currentIndex--];
        }
        //Empty returns true if stack is empty, else false
        bool Empty() {
            return currentIndex == -1;
        }

        void print_contents() {
            while (!Empty()) {
                auto cont = pop();
                std::cout << cont.node << "    " << cont.next_child << std::endl;
            }
        }

};
#endif
