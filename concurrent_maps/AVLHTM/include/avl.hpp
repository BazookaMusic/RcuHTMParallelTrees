#ifndef INCLUDE_AVLTree_HPP
#define INCLUDE_AVLTree_HPP

#define USER_NODE_POOL USER_MEM_POOL


#include <cassert>
#include <limits>
#include <tuple>
#include "../../../include/SafeTree.hpp"

using namespace SafeTree;


constexpr int THREAD_AMOUNT_MAX = 100;
TSX::TSXStats stats[THREAD_AMOUNT_MAX];

template <class ValueType>
class AVLTree;
    

template <class ValueType>
class AVLNode {
    friend class AVLTree<ValueType>;
    private:
        int key;
        ValueType value; 
        AVLNode* children[2];
        int height;



        using SafeAVLNode = SafeNode<AVLNode<ValueType>>;

        // return how balanced the current node is
        static int node_balance(AVLNode *node) {
            if (!node) {
                return 0;
            }

            int r_height,l_height;


            r_height = node->getChild(1) ? node->getChild(1)->height : 0;
            l_height = node->getChild(0)  ? node->getChild(0)->height : 0;

            return l_height - r_height;
        }

        // returns the max of the heights of two subtrees
        // or 0 if nullptr
        static inline unsigned int max_height(AVLNode *l, AVLNode *r) {
            int l_height = l ? l->height: 0;
            int r_height = r ? r->height: 0;

            if (l_height > r_height) {
                return l_height;
            }

            return r_height;
        }

        // right avl rotation
        // notice how all it takes is to use
        // SafeNodes instead of AVL Nodes but the code
        // stays the same as the serial version
        static SafeAVLNode* right_rotate(SafeAVLNode* z) {
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
            auto newRoot = z->getChild(0);  // use the SafeNode to get children

            assert(newRoot != nullptr);
            // left's right will be moved to old root's left
            auto T2 = newRoot->getChild(1);

            // connect old root right of new root
            newRoot->setChild(1,z);
            // connect left's right
            z->setChild(0,T2);

            // copy to change
            auto z_safe = z->rwRef();   // for values get a reference
            auto newRoot_safe = newRoot->rwRef();

            z_safe->height = max_height(z_safe->getL(), z_safe->getR()) + 1;
            newRoot_safe->height = max_height(newRoot_safe->getChild(0), newRoot_safe->getChild(1)) + 1;

            return newRoot;
        }

        // left rotation
        static SafeAVLNode* left_rotate(SafeAVLNode *z) {
            /*	     z                                y
                /  \                             /   \
                T1     y     Left Rotate(z)       z      x
                    /  \   - - - - - - - ->      / \    / \
                    T2   x                      T1  T2 T3  T4
                        / \
                    T3  T4                                             */

            
            assert(z != nullptr);
            auto newRoot = z->getChild(1);
            assert(newRoot != nullptr);
            auto temp = newRoot->getChild(0);


            newRoot->setChild(0, z);
            z->setChild(1,temp);

            auto z_safe = z->rwRef();
            auto newRoot_safe = newRoot->rwRef();

            z_safe->height = max_height(z_safe->getChild(0), z_safe->getChild(1)) + 1;
            newRoot_safe->height = max_height(newRoot_safe->getChild(0), newRoot_safe->getChild(1)) + 1;
            return newRoot;
        }

    public:
        using KeyType = int;

        AVLNode(int key, ValueType val, AVLNode* left_child, AVLNode* right_child): key(key), value(val), height(1) {
            children[0] = left_child;
            children[1] = right_child;
        }

        // to satisfy search tree interface
        bool hasKey(int key_requested) {
            return key == key_requested;
        }

        int nextChild(int desired_key) const {
            return desired_key < key ? 0 : 1;
        }

        bool traversalDone(int desired_key) const {
            return key == desired_key;
        }

        int nextChild(const AVLNode* target) const {
            return target->key < key ? 0 : 1;
        }

        // and the basic required methods
         static constexpr int maxChildren() {
            return 2;
        }
   
        AVLNode** getChildPointer(int i) {
            assert(i >= 0 && i < 2);
            return &children[i];
        }

        AVLNode* getChild(int i) {
            assert(i >= 0 && i < 2);
            return children[i];
        }

        
        void setChild(int i, AVLNode* node) {
            assert(i >= 0 && i < 2);
            children[i] = node;
        }

        // helpers for the avl tree

        int getKey() const {
            return key;
        }

        int getValue() const {
            return value;
        }

        void setKey(int new_key) {
            key = new_key;
        }

        void setValue(ValueType new_val) {
            value = new_val;
        }

        AVLNode* getL() {
            return children[0];
        }

        AVLNode* getR() {
            return children[1];
        }


        AVLNode* setL(AVLNode* n) {
            children[0] = n;
        }

        AVLNode* setR(AVLNode* n) {
            children[1] = n;
        }

        int getHeight() {
            return height;
        }

        AVLNode** getChildren() {
            return children;
        }

};

template <class ValueType>
struct Result {
    bool found;
    ValueType val;

};

template <class ValueType>
class AVLTree {
    friend class AVLNode<ValueType>;
    private:
        AVLNode<ValueType>* root;
        TSX::SpinLock &_lock;
        using TreeNode = AVLNode<ValueType>;
        const int trans_retries = 30;
        

        // helpers

        // sum of keys of tree
        // for validation
        static std::size_t key_sum_helper(TreeNode* node) {
            if (!node) {
                return 0;
            } 

            return node->getKey() + key_sum_helper(node->getChild(0)) + key_sum_helper(node->getChild(1));
        }

        // number of nodes of tree
        static int count_nodes(TreeNode* node) {
            if (!node) {
                return 0;
            }

            return 1 + count_nodes(node->getChild(0)) + count_nodes(node->getChild(1));
        }


        // copy the tree
        void node_copy(AVLNode<ValueType>* curr, AVLNode<ValueType>* copy_curr = nullptr) {
            if (!curr) return;

            if (!copy_curr) {
                copy_curr = new AVLNode<ValueType>*(curr);
            }

            copy_curr->setChild(0, curr->getChild(0));
            copy_curr->setChild(1, curr->getChild(1));

            node_copy(curr->getChild(0), copy_curr->getChild(0));
            node_copy(curr->getChild(1), copy_curr->getChild(1));
        }

        // print the tree pre-order
        void print_contents(AVLNode<ValueType>* root) {
            if (!root) {
                return;
            }

            std::cout << root->key << " ";
            print_contents(root->getChild(0));
            print_contents(root->getChild(1));
        }

        //print in-order
        void print_sorted_contents(AVLNode<ValueType>* root) {
            if (!root) {
                return;
            }

            print_sorted_contents(root->getChild(0));
            std::cout << root->key << " ";
            print_sorted_contents(root->getChild(1));
        }

        // print up to a certain height (depth)
        void print_depth(AVLNode<ValueType>* root,int depth) {
            if (depth <= 0 || !root) {
                return;
            }

            std::cout << root->key << " ";
            print_depth(root->getChild(0), depth-1);
            print_depth(root->getChild(1), depth-1);
        }

        // tree's longest branch
        int longest_branch(AVLNode<ValueType>* root) {
            if (!root) {
                return 0;
            }

            const int left_branch_length = longest_branch(root->getChild(0));
            const int right_branch_length = longest_branch(root->getChild(1));

            return  left_branch_length > right_branch_length? 1 + left_branch_length: 
                    1 + right_branch_length;
        }
        
        // tree's averge branch size
        void averageBranchHelper(AVLNode<ValueType>* root,int& total_leaves, int& total_length, int curr_branch_length = 1) {
            if (!root) {
                return;
            }

            if (!root->getChild(0) && !root->getChild(1)) {
                total_leaves += 1;
                total_length += curr_branch_length;
            }
            
            averageBranchHelper(root->getChild(0), total_leaves, total_length,  curr_branch_length + 1);
            averageBranchHelper(root->getChild(1), total_leaves, total_length,  curr_branch_length + 1);
        }

        int averageBranchLength(AVLNode<ValueType>* root) {
            int total_leaves = 0;
            int total_length = 0;

            averageBranchHelper(root,total_leaves,total_length);

            return total_leaves ? total_length / total_leaves: -1;
        }

        // bst validator
        bool isBstHelper(TreeNode* node,int min, int max) {
            if (!node) {
                return true;
            }

            auto nodekey = node->key;

            if ( nodekey < min || nodekey > max) {
                return false;
            }

            return isBstHelper(node->getChild(0), min, nodekey) && isBstHelper(node->getChild(1), nodekey, max);
        }

        // avl balance validator
        bool isBalancedHelper(TreeNode* node) {
            if (!node) {
                return true;
            }

            auto l_height = node->getL() ? node->getL()->height: 0;
            auto r_height = node->getR() ? node->getR()->height: 0;

            return abs(l_height - r_height) < 2 && isBalancedHelper(node->getL()) && isBalancedHelper(node->getR());
        }

        // recursively delete
        void rec_delete(TreeNode* node) {
            if (!node) return;

            rec_delete(node->getChild(0));
            rec_delete(node->getChild(1));

            delete node;
        }

        // apply rebalancing for an insertion operation
        SafeNode<TreeNode>* rebalance_ins(SafeNode<TreeNode>* n, int k, bool& rotation_happened) {

             // reads and writes are safe
            auto n_values = n->rwRef();

            // calculate node height
            n_values->height = TreeNode::max_height(n_values->getL(), n_values->getR()) + 1;

            
            // calculate node's balance
            auto balance = TreeNode::node_balance(n_values);

            rotation_happened = true;

            // for tree modifications (change children etc) use SafeNode

            if (balance > 1 && k < n_values->getL()->key) { // right rotate
                n = TreeNode::right_rotate(n);
            } else if (balance < -1 && k > n_values->getR()->key) { //left rotate
                n = TreeNode::left_rotate(n);
            } else if (balance > 1 && k > n_values->getL()->key) { // left right rotate
                n->setChild(0 ,TreeNode::left_rotate(n->getChild(0)));
                n_values->height = TreeNode::max_height(n_values->getL(), n_values->getR()) + 1;
                n = TreeNode::right_rotate(n);
            } else if (balance < -1 && k < n_values->getR()->key) { // right Left Rotate
                n->setChild(1,TreeNode::right_rotate(n->getChild(1)));
                n_values->height = TreeNode::max_height(n_values->getL(), n_values->getR()) + 1;
                n = TreeNode::left_rotate(n);
            } else {
                rotation_happened = false;
            }

            // calculate new height
            n_values->height = TreeNode::max_height(n_values->getL(), n_values->getR()) + 1;

            return n;

        }

        // the insert operation
        bool insert_impl(const int k, ValueType val, int t_id) {

            (void)t_id;
            
            TM_SAFE_OPERATION_START(30) {

                /* FIND PHASE */

                
                auto conn_point_snapshot = find_conn_point<TreeNode>(k,&root);


                if (conn_point_snapshot.found()) {
                    return false;
                }


                    /* FIND PHASE END */

                ConnPoint<TreeNode> conn(conn_point_snapshot);

                /* INSERT */

                // build new node
                #ifdef USER_NODE_POOL
                    auto node_to_be_inserted = conn.create_safe(ConnPoint<TreeNode>::create_new_node(k,val,nullptr,nullptr));
                #else
                    auto node_to_be_inserted = conn.create_safe(new AVLNode<ValueType>(k,val,nullptr,nullptr));
                #endif


                // insert it
                conn.setRoot(node_to_be_inserted);

                // identical to bst up to this point


                // if not inserting at root
                if (conn_point_snapshot.connection_point()) {
                    
                    bool rotation_happened = false;
                    
                    

                    /* REBALANCE */
                    
                    // go up path and rebalance
                    for (SafeNode<TreeNode>* n = conn.pop_path(); n != nullptr; n = conn.pop_path()) {
                        auto n_values = n->rwRef();
                        int height_old = n_values->height;

                        n = rebalance_ins(n, k , rotation_happened); // apply rebalancing to all
                                                                    // required nodes
                                                                    // rotation returns the new root to place
                                                                    // below the connection point

                        conn.setRoot(n);    // change the root of the
                                            // tree of copies to make the change visible

                        if (height_old == n_values->height && !rotation_happened) {
                            break;
                        }

                    }
                }
            } TM_SAFE_OPERATION_END

            // OPERATION END can be omitted if
            // not using EARLY ABORT COMPILATION FLAGS
            
            
       
            return true;

        }

    // rebalance for removes
    SafeNode<TreeNode>* rebalance_rem(SafeNode<TreeNode>* n, bool& rotation_happened) {
        auto n_value = n->rwRef();

		// calculate node height
        // can peek children from a rwRef
        // could also use n->peekChild
		n_value->height = TreeNode::max_height(n_value->getL(), n_value->getR()) + 1;

		// calculate node's balance
		int balance = TreeNode::node_balance(n_value);

        rotation_happened = true;

		if (balance > 1 && TreeNode::node_balance(n_value->getL()) >= 0) {  // right rotate
			n = TreeNode::right_rotate(n);
		} else if (balance < -1 && TreeNode::node_balance(n_value->getR()) <= 0) { //left rotate
			n = TreeNode::left_rotate(n);
	  	} else if (balance > 1 && TreeNode::node_balance(n_value->getL()) < 0) { // left right rotate
			n->setChild(0, TreeNode::left_rotate(n->getChild(0)));
			n_value->height = TreeNode::max_height(n_value->getL(), n_value->getR()) + 1;
			n = TreeNode::right_rotate(n);
		} else if (balance < -1 && TreeNode::node_balance(n_value->getR()) > 0 ) { // right Left Rotate
			n->setChild(1, TreeNode::right_rotate(n->getChild(1)));
			n_value->height = TreeNode::max_height(n_value->getL(), n_value->getR()) + 1;
			n = TreeNode::left_rotate(n);
		} else {
            rotation_happened = false;
        }

        return n;
    }

    SafeNode<TreeNode>* rebalance_rem(SafeNode<TreeNode>* n) {
        auto r_h = false;
        return rebalance_rem(n, r_h);
    }


    bool remove_impl(const int k, const int t_id) {
        (void)t_id;
        
        TM_SAFE_OPERATION_START(30) {

            /* FIND PHASE */

                
            auto conn_point_snapshot = find_conn_point<TreeNode>(k,&root);


            if (!conn_point_snapshot.found()) {
                return false;
            }


            /* FIND PHASE END */

            ConnPoint<TreeNode> conn(conn_point_snapshot);

            /* REMOVE */

            auto node_to_be_deleted = conn.getRoot();

            // just read them to see if they exist
            auto l_child = node_to_be_deleted->peekChild(0);
            auto r_child = node_to_be_deleted->peekChild(1);


            // one or no children, at point of insertion
            if (!l_child && !r_child) {
                conn.setRoot(nullptr);
                node_to_be_deleted = nullptr;
            }
            else if (!l_child || !r_child) {
                auto temp = l_child ? node_to_be_deleted->getChild(0): node_to_be_deleted->getChild(1);

                // just set it as new root
                // of copied tree, if it exists
                node_to_be_deleted = nullptr;
                conn.setRoot(temp);
            } else {
                // search for smallest of the right subtree

                NodeStack<SafeNode<TreeNode>, 10000> del_stack;

                auto smallest = node_to_be_deleted->getChild(1);

                while (smallest->getChild(0)) {
                    del_stack.push(smallest);
                    smallest = smallest->getChild(0);
                }

                auto node_to_be_deleted_values = node_to_be_deleted->rwRef();
                auto smallest_ref = smallest->rwRef();

                node_to_be_deleted_values->setKey(smallest_ref->getKey());
                node_to_be_deleted_values->setValue(smallest_ref->getValue());

                // directly below node
                // at its right
                // delete and set its right child as the next child of
                // the proper node
                if (del_stack.Empty()) {
                    node_to_be_deleted->setChild(1, conn.wrap_no_validate(smallest_ref->getChild(1)));
                } else {
                    auto parent_of_smallest = del_stack.pop();
                    
                    // delete node which was removed
                    parent_of_smallest->setChild(0, conn.wrap_no_validate(smallest_ref->getChild(1)));

                    // rebalance its parent
                    auto new_child = rebalance_rem(parent_of_smallest);

                    // while there is a path to 
                    // the node to be deleted
                    while(!del_stack.Empty()) {
                        // remove from path
                        auto temp_child = del_stack.pop();

                        // connect already rebalanced
                        temp_child->setChild(0, new_child);

                        // create new rebalanced
                        new_child = rebalance_rem(temp_child);
                    }

                    // finally add as child of node to be deleted
                    node_to_be_deleted->setChild(1, new_child);
                }

            }

            // tree is now balanced up to the right
            // subtree of the new node

            // now rebalance the root of the copied tree
            if (node_to_be_deleted) {
                conn.setRoot(rebalance_rem(node_to_be_deleted));
            }

            int height_old;

            bool rotation_happened = false;
            // now rebalance upwards
            for (SafeNode<TreeNode>* n = conn.pop_path(); n != nullptr; n = conn.pop_path()) {
                auto n_values = n->rwRef();
                height_old = n_values->height;

                n = rebalance_rem(n, rotation_happened);

                conn.setRoot(n);

                if (height_old == n_values->height && !rotation_happened) {
                    break;
                } 
            }
        } TM_SAFE_OPERATION_END

        return true;
    }




    public:

    AVLTree(TreeNode* root, TSX::SpinLock &lock): root(root), _lock(lock){
        #ifdef USER_NODE_POOL
            ConnPoint<TreeNode>::init_node_pool();
        #endif

        for (int i = 0; i < THREAD_AMOUNT_MAX; i++) {
            stats[i].reset();
        }
    }
    ~AVLTree() {
        #ifndef USER_NODE_POOL
            rec_delete(root);
        #endif

        #ifdef USER_NODE_POOL
            ConnPoint<TreeNode>::reset_node_pool();
        #endif
    }

    bool insert(const int k, ValueType val, int t_id) {
        return insert_impl(k,val, t_id);
    }

    Result<ValueType> lookup(int desired_key) {
        auto node = find<TreeNode>(root,desired_key);
        
        auto found = node != nullptr;

        return {found, found ? node->getValue(): ValueType() };
    }

    // remove key value pair with key k
    bool remove(int k, int t_id) {
        return remove_impl(k,t_id);
    }


    int size() {
        return count_nodes(root);
    }

    AVLNode<ValueType>* getRoot() {
        return root;
    }

    void setRoot(AVLNode<ValueType>* node) {
        root = node;
    }

    /* VALIDATORS */

    std::size_t key_sum() {
        return key_sum_helper(root);
    }

    bool isSorted() {
        if (!root) {
            return true;
        }

        return isBstHelper(root,std::numeric_limits<int>::min(),std::numeric_limits<int>::max());
    }

    bool isBalanced() {
        return isBalancedHelper(root);
    }

    /* END OF VALIDATORS */


    /* HELPERS */


    void print() {
        print_contents(root);
        std::cout << std::endl;
        std::cout << "Longest Branch is: " << longest_branch(root) << std::endl;
    }

    void longest_branch() {
        std::cout << "Longest branch is: " << longest_branch(root) << std::endl;
    }


    void average_branch() {
        std::cout << "Average branch is: " << averageBranchLength(root) << std::endl;
    }


    void print_sorted() {
        print_sorted_contents(root);
        std::cout << std::endl;
        std::cout << "Longest Branch is: " << longest_branch(root) << std::endl;
    }

    void print_up_to(int d) {
        print_depth(root,d);
        std::cout << std::endl;
        std::cout << "Longest Branch is: " << longest_branch(root) << std::endl;
    }

    void stat_report(int n_threads) {
        TSX::TSXStats t_stats;
        for (int i = 0; i < n_threads; i++) {
            t_stats += stats[i];
        }
        std::cout << std::endl << std::endl;
        t_stats.print_stats();
        std::cout << std::endl << std::endl;
    }


    void lite_stat(int n_threads, long long n_ops = -1) {
        TSX::TSXStats total_stats;
        for (int i = 0; i < n_threads; i++) {
            total_stats += stats[i];
        }

        if (n_ops > 0) {
            std::cout << std::endl;
            std::cout << "ABORTS/OP: " << (total_stats.tx_aborts * 100.0)/(n_ops) << "%" << std::endl;
        }
        total_stats.print_lite_stats();
    }

    /* END OF HELPERS */


};



#endif