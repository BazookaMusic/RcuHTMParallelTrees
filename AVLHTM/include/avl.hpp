#ifndef INCLUDE_AVLTree_HPP
#define INCLUDE_AVLTree_HPP

#define USER_NODE_POOL USER_MEM_POOL


#include <cassert>
#include <limits>
#include <tuple>
#include "SafeTree.hpp"



TSX::TSXStats stats[20];

template <class ValueType>
class AVLTree;
    

template <class ValueType>
class alignas(64) AVLNode {
    friend class AVLTree<ValueType>;
    private:
        int key;
        ValueType value; 
        AVLNode* children[2];
        int height;

        using SafeAVLNode = SafeNode<AVLNode<ValueType>>;


        static int node_balance(AVLNode *node) {
            if (!node) {
                return 0;
            }

            int r_height,l_height;


            r_height = node->getChild(1) ? node->getChild(1)->height : 0;
            l_height = node->getChild(0)  ? node->getChild(0)->height : 0;

            return l_height - r_height;
        }

        static inline unsigned int max_height(AVLNode *l, AVLNode *r) {
            int l_height = l ? l->height: 0;
            int r_height = r ? r->height: 0;

            if (l_height > r_height) {
                return l_height;
            }

            return r_height;
        }

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
            auto newRoot = z->getChild(0);

            assert(newRoot != nullptr);
            // left's right will be moved to old root's left
            auto T2 = newRoot->getChild(1);

            // connect old root right of new root
            newRoot->setChild(1,z);
            // connect left's right
            z->setChild(0,T2);

            // copy to change
            auto z_safe = z->safeRef();
            auto newRoot_safe = newRoot->safeRef();

            z_safe->height = max_height(z_safe->getL(), z_safe->getR()) + 1;
            newRoot_safe->height = max_height(newRoot_safe->getChild(0), newRoot_safe->getChild(1)) + 1;

            return newRoot;
        }

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

            auto z_safe = z->safeRef();
            auto newRoot_safe = newRoot->safeRef();

            z_safe->height = max_height(z_safe->getChild(0), z_safe->getChild(1)) + 1;
            newRoot_safe->height = max_height(newRoot_safe->getChild(0), newRoot_safe->getChild(1)) + 1;
            return newRoot;
        }

    public:
        AVLNode(int key, ValueType val, AVLNode* left_child, AVLNode* right_child): key(key), value(val), height(1) {
            children[0] = left_child;
            children[1] = right_child;
        }

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

        AVLNode** getChildPointer(int i) {
            assert(i >= 0 && i < 2);
            return &children[i];
        }

        AVLNode* getChild(int i) {
            assert(i >= 0 && i < 2);
            return children[i];
        }



        AVLNode* getL() {
            return children[0];
        }

        AVLNode* getR() {
            return children[1];
        }


        void setChild(int i, AVLNode* node) {
            assert(i >= 0 && i < 2);
            children[i] = node;
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

        static constexpr int maxChildren() {
            return 2;
        }

        int nextChild(int desired_key) const {
            return desired_key < key ? 0 : 1;
        }

        bool traversalDone(int desired_key) const {
            return key == desired_key;
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

        // helpers

        static int count_nodes(TreeNode* node) {
            if (!node) {
                return 0;
            }

            return 1 + count_nodes(node->getChild(0)) + count_nodes(node->getChild(1));
        }


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

        void print_contents(AVLNode<ValueType>* root) {
            if (!root) {
                return;
            }

            std::cout << root->key << " ";
            print_contents(root->getChild(0));
            print_contents(root->getChild(1));
        }

        void print_sorted_contents(AVLNode<ValueType>* root) {
            if (!root) {
                return;
            }

            print_sorted_contents(root->getChild(0));
            std::cout << root->key << " ";
            print_sorted_contents(root->getChild(1));
        }

        void print_depth(AVLNode<ValueType>* root,int depth) {
            if (depth <= 0 || !root) {
                return;
            }

            std::cout << root->key << " ";
            print_depth(root->getChild(0), depth-1);
            print_depth(root->getChild(1), depth-1);
        }

        int longest_branch(AVLNode<ValueType>* root) {
            if (!root) {
                return 0;
            }

            const int left_branch_length = longest_branch(root->getChild(0));
            const int right_branch_length = longest_branch(root->getChild(1));

            return  left_branch_length > right_branch_length? 1 + left_branch_length: 
                    1 + right_branch_length;
        }
        
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

        bool isBalancedHelper(TreeNode* node) {
            if (!node) {
                return true;
            }

            auto l_height = node->getL() ? node->getL()->height: 0;
            auto r_height = node->getR() ? node->getR()->height: 0;

            return abs(l_height - r_height) < 2 && isBalancedHelper(node->getL()) && isBalancedHelper(node->getR());
        }

        void rec_delete(TreeNode* node) {
            if (!node) return;

            rec_delete(node->getChild(0));
            rec_delete(node->getChild(1));

            delete node;
        }

        SafeNode<TreeNode>* rebalance_ins(SafeNode<TreeNode>* n, int k, bool rotation_happened) {
             // reads and writes are safe
            auto n_values = n->safeRef();

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

            n_values->height = TreeNode::max_height(n_values->getL(), n_values->getR()) + 1;

            return n;

        }

        bool insert_impl(const int k, ValueType val, int t_id) {
            bool success = false;
            int retries = hard_retries;

            while(!success) {
                Transaction t(retries,_lock, stats[t_id],soft_retries);

                /* FIND PHASE */

                
                auto conn_point_data = find_conn_point<TreeNode>(k,&root);


                if (conn_point_data.found()) {
                    return false;
                }


                 /* FIND PHASE END */

                ConnPoint<TreeNode> conn(conn_point_data, success, t);

                /* INSERT */

                // build new node
                #ifdef USER_NODE_POOL
                    auto node_to_be_inserted = conn.create_safe(ConnPoint<TreeNode>::create_new_node(k,val,nullptr,nullptr));
                #else
                    auto node_to_be_inserted = conn.create_safe(new AVLNode<ValueType>(key,val,nullptr,nullptr));
                #endif


                // insert it
                conn.setRoot(node_to_be_inserted);


                // if not inserting at root
                if (conn_point_data.connection_point()) {
                    
                    bool rotation_happened = false;
                    int height_old = 0;
                    

                    /* REBALANCE */
                    
                    // go up path and rebalance
                    for (SafeNode<TreeNode>* n = conn.pop_path(); n != nullptr; n = conn.pop_path()) {
                        auto n_values = n->safeRef();
                        height_old = n_values->height;

                        n = rebalance_ins(n, k , rotation_happened);

                        conn.setRoot(n);

                        if (height_old == n_values->height && !rotation_happened) {
                            break;
                        }

                    }
                }
            }

            return true;

        }



    public:

    AVLTree(TreeNode* root, TSX::SpinLock &lock): root(root), _lock(lock){
        #ifdef USER_NODE_POOL
            ConnPoint<TreeNode>::init_node_pool();
        #endif
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

    int size() {
        return count_nodes(root);
    }

    AVLNode<ValueType>* getRoot() {
        return root;
    }

    void setRoot(AVLNode<ValueType>* node) {
        root = node;
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


    Result<ValueType> lookup(int desired_key) {
        Result<ValueType> res;
        auto node = find<TreeNode>(root,desired_key);
        
        res.found = node != nullptr;
        if (res.found) res.val = node->getValue();
        
        return res;
    }

    int find_conn(int desired_key) {
        auto conn_point_data = find_conn_point<TreeNode>(desired_key,&root);
        return conn_point_data.con_ptr.child_index;
    }

    const int hard_retries = 5;
    const int soft_retries = 100;

    bool remove(int k, int t_id) {
        (void)k;
        (void)t_id;
        return true;
    }


    // bool remove(int key, int t_id) {
    //     bool success = false;
    //     int retries = hard_retries;

    //     while(!success) {
    //         Transaction t(retries,_lock, stats[t_id],soft_retries);

    //         auto conn_point_data = find_conn_point<TreeNode>(key,&root);

    //         if (!conn_point_data.found()) {
    //             return false;
    //         }

    //         ConnPoint<TreeNode> conn(conn_point_data, success, t);

    //         // the connection point is above the
    //         // node which will be deleted

    //         // thus the node to be deleted will be
    //         // the root of the copied tree
    //         auto node_to_be_deleted = conn.getRoot();

    //         // read only, to see if they exist
    //         auto its_left_child = node_to_be_deleted->peekChild(0); 
    //         auto its_right_child = node_to_be_deleted->peekChild(1);

    //         if (!its_right_child && !its_left_child) {
    //             conn.setRoot(nullptr);
    //         } else if (!its_right_child) {
    //             // get left child and replace node to be deleted with it
    //             auto left_child = node_to_be_deleted->getChild(0);
    //             conn.setRoot(left_child);
    //         } else {
    //             // get right child and search for the lowest leaf
    //             // going left
    //             SafeNode<TreeNode>* prev = nullptr;
    //             auto curr = node_to_be_deleted->getChild(1);
    //             for (; curr && curr->getChild(0); prev = curr, curr = curr->getChild(0));

    //             auto node_to_replace_root = curr;


               
    //             // to modify a node's internals we need a safe reference
    //             // that causes a copy of the node
    //             TreeNode* node_to_be_deleted_copy = node_to_be_deleted->safeRef();
    //             // no need to copy node we will be removing
    //             // just look at the original to get its key and value
    //             const TreeNode* node_to_replace_view = node_to_replace_root->peekOriginal();

    //             auto new_key = node_to_replace_view->key;
    //             auto new_value = node_to_replace_view->getValue();

    //             node_to_be_deleted_copy->setKey(new_key);
    //             node_to_be_deleted_copy->setValue(new_value);

    //             // new node is fine, now
    //             // remove old node

    //             // to remove it, its right child should take its place

    //             // get the right child
    //             // and wrap it to be able to
    //             // insert it into the copied tree
    //             auto right_child = conn.create_safe(node_to_replace_root->peekChild(1));
    //             if (prev) {
    //                 prev->setChild(0,right_child);
    //             } else {
    //                 node_to_be_deleted->setChild(1,right_child);
    //             }
                
    //         }

    //     }

    //     return true;
    // }


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

    void lite_stat(int n_threads) {
        TSX::TSXStats total_stats;
        for (int i = 0; i < n_threads; i++) {
            total_stats += stats[i];
        }

        total_stats.print_lite_stats();
    }
};



#endif