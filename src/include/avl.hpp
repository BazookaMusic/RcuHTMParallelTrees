#ifndef AVL_HPP
    #define AVL_HPP

#include <iostream>
#include <cassert>
#include "generic_tree.hpp"


template <class T> class AVLNode;
template <class T> class SearchTreeStack;

enum Error {None, AlreadyInserted, KeyFound, KeyNotFound};


template <class T>
struct OperationResult {
    AVLNode<T> *result;
    enum Error err;

};





template <class T>
class AVLNode
{
 public:
    int key;
    T val;
    unsigned int height;
    AVLNode<T> *children[2]; 

 public:
    AVLNode() {};
    AVLNode(int _key, T _value, AVLNode<T> *left, AVLNode<T> *right) {
        key = _key;
        val = _value;
        height = 1;
        setL(left);
        setR(right);
    };

    static int nChildren() {
        return 2;
    }




    static int nodeBalance(AVLNode *node) {
        if (!node) {
            return 0;
        }

        int r_height,l_height;


        r_height = node->getR() ? node->getR()->height : 0;
        l_height = node->getL()  ? node->getL()->height : 0;

        return l_height - r_height;
    };

    static unsigned int maxheight(AVLNode *l, AVLNode *r) {
        int l_height = l ? l->height: 0;
        int r_height = r ? r->height: 0;

        if (l_height > r_height) {
            return l_height;
        }

        return r_height;
    };

    AVLNode* getL() {return children[0];};

    AVLNode* getR() {return children[1];};

    void setL(AVLNode *newNode) {children[0] = newNode;};

    void setR(AVLNode *newNode) {children[1] = newNode;};

    AVLNode* getChild(const int n) {
        assert(n >=0 && n < nChildren() );
        return children[n];
    }

    void setChild(const int n, AVLNode * node_to_set) {
        assert(n >=0 && n < nChildren() );
        children[n] = node_to_set;
    }


    AVLNode *rightRotate(AVLNode *z) {
        /*  T1, T2, T3 and T4 are subtrees.
            z                                      y
            / \                                   /   \
           y   T4      Right Rotate (z)          x      z
           /   \          - - - - - - - - ->    /  \    /  \
           x    T3                             T1  T2  T3  T4
           / \
         T1   T2 */

        assert(z != nullptr);
        // left becomes root
        auto newRoot = z->getL();

        assert(newRoot != nullptr);
        // left's right will be moved to old root's left
        auto T2 = newRoot->getR();

        // connect old root right of new root
        newRoot->setR(z);
        // connect left's right
        z->setL(T2);

        z->height = maxheight(z->getL(), z->getR()) + 1;
        newRoot->height = maxheight((newRoot->getL()), newRoot->getR()) + 1;

        return newRoot;

    };

    AVLNode *leftRotate(AVLNode *z) {
        /*	     z                                y
        	   /  \                             /   \
            T1     y     Left Rotate(z)       z      x
        	     /  \   - - - - - - - ->      / \    / \
        	    T2   x                      T1  T2 T3  T4
        		    / \
        		  T3  T4                                             */

        
        assert(z != nullptr);
        auto newRoot = z->getR();
        assert(newRoot != nullptr);
        auto temp = newRoot->getL();


        newRoot->setL(z);
        z->setR(temp);

        z->height = maxheight(z->getL(), z->getR()) + 1;
        newRoot->height = maxheight(newRoot->getL(), newRoot->getR()) + 1;
        return newRoot;
    };
    //Insert insert key value pair in avl tree, implementation
    OperationResult<T> _insert(const int k,const T val) {
        // search for key or insertion
        // position
        //save path in stack
        SearchTreeStack<AVLNode<T>> insertStack;

        for (auto curr = this; curr != nullptr;) {
            // if key was found, do not insert again
            if (curr->key == k) {
                return OperationResult<T> {.result = nullptr, .err = AlreadyInserted};
            }

            // catch overflow
            insertStack.push(curr);


            if (k < curr->key) {
                curr = curr->getL();
            } else {
                curr = curr->getR();
            }

        }

        auto rotationHappened = false;

        AVLNode *prev, *n;
        prev = n = nullptr;
        unsigned int heightOld = 0;

        //std::cout << this << std::endl;
        //std::cout << "stack pops" << std::endl;
        for (n = insertStack.pop() , prev = nullptr; n != nullptr; prev = n, n = insertStack.pop()) {
            //insert new node or rebalanced node
            //std::cout << n << std::endl;
            if (!prev) {
                // actual insertion
                if (k < n->key) {
                    n->setL(new AVLNode(k,val,nullptr,nullptr));
                } else {
                    //std::cout << "right insert" << std::endl;
                    n->setR(new AVLNode(k,val,nullptr,nullptr));
                }
            } else if (rotationHappened) {
                if (k < n->key) {
                    n->setL(prev);
                    rotationHappened = false;
                } else {
                    n->setR(prev);
                    rotationHappened = false;
                }
            }

            heightOld = n->height;
            // calculate node height
            n->height = maxheight(n->getL(), n->getR()) + 1;

            // calculate node's balance
            auto balance = nodeBalance(n);

            if (balance > 1 && k < n->getL()->key) { // right rotate
                n = rightRotate(n);
                rotationHappened = true;
            } else if (balance < -1 && k > n->getR()->key) { //left rotate
                n = leftRotate(n);
                rotationHappened = true;
            } else if (balance > 1 && k > n->getL()->key) { // left right rotate
                n->setL(leftRotate(n->getL()));
                n->height = maxheight(n->getL(), n->getR()) + 1;
                n = rightRotate(n);
                rotationHappened = true;
            } else if (balance < -1 && k < n->getR()->key) { // right Left Rotate
                n->setR(rightRotate(n->getR()));
                n->height = maxheight(n->getL(), n->getR()) + 1;
                n = leftRotate(n);
                rotationHappened = true;
            }

            n->height = maxheight(n->getL(), n->getR()) + 1;

            if (heightOld == n->height && !rotationHappened) {
                prev = n;
                n = insertStack.pop();
                break;
            }
        }

        

        if (n) {
            return OperationResult<T> {.result = nullptr, .err = None};
        }  else {
            return OperationResult<T> {.result = prev, .err = None};
        }


    }

    
    AVLNode *_deleteNodeOneOrNoChildren(const int k,AVLNode *curr,AVLNode *parentOfDeleted) {
        AVLNode *temp;

        temp = curr->getL() ? curr->getL() : curr->getR(); // either has left or right child
        

        if (!parentOfDeleted) { // root of tree
            // no children empty tree
            if (!temp) {
                return nullptr;
            }

            // one child becomes root
            return new AVLNode(temp->key, temp->val, temp->getL(), temp->getR());

        } else if( k < parentOfDeleted->key) { // delete through parent
            parentOfDeleted->setL(temp);
        } else {
             parentOfDeleted->setR(temp);
        }

        return parentOfDeleted;
    }


    OperationResult<T> _remove(int k) {

	// search for key or insertion
	// position
	//save path in stack
	auto curr = this;
	SearchTreeStack<AVLNode<T>> deleteStack;

	AVLNode *parentOfDeleted = nullptr;


	for (;curr;) {
		// if key was found, do not insert again
		if (curr->key == k) {
			break;
		}

		deleteStack.push(curr);

		parentOfDeleted = curr;
		if (k < curr->key) {

			curr = curr->getL();
		} else {
			curr = curr->getR();
		}

	}

	// not found case
	if (!curr) {
		return OperationResult<T> {.result = nullptr, .err = KeyNotFound};
	}

	// found case

	 AVLNode *temp = nullptr;

	// one or no children
	if (curr->getL() == nullptr || curr->getR() == nullptr) {
		parentOfDeleted = _deleteNodeOneOrNoChildren(k, curr, parentOfDeleted);

		// tree is empty
		if (!parentOfDeleted) {
			return OperationResult<T> {.result = nullptr, .err = None};
		}
	} else {
		// node with two children: Get the inorder
		// successor (smallest in the right subtree)

		// add current node to stack for rebalancing
		deleteStack.push(curr);

		// find inorder successor
		AVLNode *parentOfSuccessor = curr;
		for (temp = curr->getR(); temp->getL() != nullptr; parentOfSuccessor = temp, temp = temp->getL()) {
			// keep path in rebalancing stack
			deleteStack.push(temp);
		}

		// delete successor, obviously has one or no children
		parentOfSuccessor = _deleteNodeOneOrNoChildren(temp->key, temp, parentOfSuccessor);

		if (!parentOfSuccessor) {
			/* DELETE NODE 
            
            FIX ME
            
            */
           delete curr->getR();
           curr->setR(nullptr); 
		}

		// modify node to be deleted with successor's data
		curr->key = temp->key;
		curr->val = temp->val;
	}

	//traverse path backwards
	AVLNode *n, *prev;
	bool rotationHappened = false;
	bool brokeSooner = false;
    unsigned int heightOld;
    int balance;

	for (n = deleteStack.pop() , prev = nullptr; n != nullptr; prev = n, n = deleteStack.pop()) {
		rotationHappened = false;

		if (prev) {
			if (prev->key < n->key) {
				n->setL(prev);
			} else {
				n->setR(prev);
			}
		}

		heightOld = n->height;

		// calculate node height
		n->height = maxheight(n->getL(), n->getR()) + 1;

		// calculate node's balance
		balance = nodeBalance(n);

		
		if (balance > 1 && nodeBalance(n->getL()) >= 0) {  // right rotate
			rotationHappened = true;

			n = rightRotate(n);
		} else if (balance < -1 && nodeBalance(n->getR()) <= 0) { //left rotate
			rotationHappened = true;
			n = leftRotate(n);
		} else if (balance > 1 && nodeBalance(n->getL()) < 0) { // left right rotate
			rotationHappened = true;
			n->setL(leftRotate(n->getL()));
			n->height = maxheight(n->getL(), n->getR()) + 1;
			n = rightRotate(n);
		} else if (balance < -1 && nodeBalance(n->getR()) > 0 ){ // right Left Rotate
			rotationHappened = true;
			n->setR(rightRotate(n->getR()));
			n->height = maxheight(n->getL(), n->getR()) + 1;
			n = leftRotate(n);
		}

		if (heightOld == n->height && !rotationHappened) {
			prev = n;
            n = deleteStack.pop();
			brokeSooner = true;
			break;
		}

	}

	if (brokeSooner) { // return nullptr to point out that root doesn't change
		return OperationResult<T> {.result = nullptr, .err = None};
	}

	return OperationResult<T> {.result = prev, .err = None};

}


};


template <class T>
class AVLTree
{
private:
    static bool _AVLVerify(AVLNode<T> *root) {
        // helper to verify if tree is avl tree
        if (!root) {
            return true;
        }

        auto balance = AVLNode<T>::nodeBalance(root);

        return (balance <= 1 && balance >= -1) && _AVLVerify(root->getL()) && _AVLVerify(root->getR());
    }

    
    static void _inorder(AVLNode<T> *node) {
        if (node) {
            std::cout << node->key << std::endl;
            _inorder(node->getL());
            _inorder(node->getR());
        } 
    }

public:
    AVLNode<T> *root; 

    AVLTree(): root(nullptr){};

    AVLNode<T> *getRoot() {
        return root;
    }

     AVLNode<T> **getRootPointer() {
        return &root;
    }

    bool insert(int k, T val)  {
        if (!root) {
            root = new AVLNode<T>(k,val,nullptr,nullptr);
            return true;
        }

        auto op_result = root->_insert(k,val);

        if (op_result.err != None)
           return false;
        
        // root change

        if (op_result.result != nullptr) root = op_result.result;

        return true;

    }

    bool remove(int k)  {

        auto op_result = root->_remove(k);

        if (op_result.err != None)
           return false;
        
        // root change

        if (op_result.result != nullptr) root = op_result.result;

        return true;

    }

    // lookup
    bool lookup(int k) {
        AVLNode<T> *curr = root;
        for (; curr != nullptr; curr =  k < curr->key ? curr->getL() : curr->getR() ) {
            if (curr->key == k) {
                return true;
            }
        }


        return false;
    }

    // returns true if AVL Tree constraints hold
    bool isAVL() {
        return _AVLVerify(this->root);
    }


    //prints tree in order
    void inorder() {
        _inorder(root);
    }


};

#endif








