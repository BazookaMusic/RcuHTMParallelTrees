#ifndef HELPER_DATA_STRUCTURES_HPP
    #define HELPER_DATA_STRUCTURES_HPP

#include <tuple>
#include <deque>
#include <vector>
#include <sstream>
#include <string>



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


template <class NodeType, int CAP>
class NodeStack{
    private:
        NodeType* stack[CAP];
        int currentIndex; 
        

    public:
        //TreePathStack used to traverse tree structures
        NodeStack(): currentIndex(-1) {
        };

        void clear() {
            currentIndex = -1;
        }
        

        NodeType* bottom() {
            if (currentIndex == -1) {
                return nullptr;
            }

            return stack[0];
        }

        NodeType* top() {
            if (currentIndex == -1) {
                return nullptr;
            }

            return stack[currentIndex];
        }
        

        //push pushes a node into the stack
        //and stores the index of the next child
        void push(NodeType *node) {
            currentIndex++;

            if (currentIndex + 1 > CAP) {
                std::cerr << "Too small path stack" << std::endl;
            } else {
                stack[currentIndex] = node;
            }
     
        };

        //pop removes an element from the top of the stack
        //and returns the element
        NodeType* pop() {
            if (currentIndex < 0) {
                return nullptr;
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
                std::cout << cont << std::endl;
            }
        }

};

template <class NodeType>
struct NodeAndNextPointer {
    NodeType* node;
    int next_child;

    NodeAndNextPointer(){}
    NodeAndNextPointer(NodeType* node, int next_child):node(node), next_child(next_child){}
    NodeAndNextPointer(NodeAndNextPointer&& other): node(other.node), next_child(other.next_child) {}
};

template <class NodeType, int CAP>
class TreePathStackWithIndex{
    private:
        NodeAndNextPointer<NodeType> stack[CAP];
        int currentIndex; 
        

    public:
        //TreePathStack used to traverse tree structures
        TreePathStackWithIndex(): currentIndex(-1) {
        };

        void move_to(TreePathStackWithIndex& other_stack) {
            for (int i = 0; i < currentIndex; i++) {
                other_stack.stack[i] = std::move(stack[i]);
            }
            other_stack.currentIndex = currentIndex;
        }


        int size() const {
            return currentIndex + 1;
        }

        const NodeAndNextPointer<NodeType>& operator[](int i) {
            return stack[i];
        }
        

        NodeAndNextPointer<NodeType> bottom() const {
            if (currentIndex == -1) {
                return NodeAndNextPointer<NodeType>{nullptr,-1};
            }

            return stack[0];
        }

        NodeAndNextPointer<NodeType> top() const {
            if (currentIndex == -1) {
                return NodeAndNextPointer<NodeType>{nullptr,-1};
            }

            return stack[currentIndex];
        }
        

        //push pushes a node into the stack
        //and stores the index of the next child
        void push(NodeType *node, int index) {
            currentIndex++;

            if (currentIndex + 1 > CAP) {
                std::cerr << "Too small path stack" << std::endl;
                exit(-1);
            } else {
                stack[currentIndex].node = node;
                stack[currentIndex].next_child = index;
            }
     
        }

        //pop removes an element from the top of the stack
        //and returns the element
        NodeAndNextPointer<NodeType> pop() {
            if (currentIndex < 0) {
                return NodeAndNextPointer<NodeType>{nullptr ,-1};
            } 

            return std::move(stack[currentIndex--]);
        }
        //Empty returns true if stack is empty, else false
        bool Empty() {
            return currentIndex == -1;
        }

        void print_contents() {
            for (int i = 0; i <= currentIndex; ++i)  {
                std::cout << stack[i].node << "    " << stack[i].next_child << std::endl;
            }
        }


};


template <class T, std::size_t CAP>
class PreAllocVec {
    private:
        T arr[CAP];
        int index;
    
    public:
        PreAllocVec(): index(-1) {
        }

        int size() {
            return index + 1;
        }

        void push_back(T item) {
            ++index;
            if (index == CAP) {
                std::cerr << "SetError: Set overflow" << std::endl;
                exit(-1);
            }

            arr[index] = item;
        } 

        T get(int i) {
            return arr[i];
        }

        void reset() {
            index = -1;
        }


};


#endif
