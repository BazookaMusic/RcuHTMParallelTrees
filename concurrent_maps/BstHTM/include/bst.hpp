#ifndef INCLUDE_BST_HPP
#define INCLUDE_BST_HPP

#define USER_NODE_POOL USER_MEM_POOL


#include <cassert>
#include <limits>
#include <tuple>
#include "../../../include/SafeTree.hpp"


using namespace SafeTree;


constexpr int THREAD_AMOUNT_MAX = 100;
TSX::TSXStats stats[THREAD_AMOUNT_MAX];

template <class ValueType>
class BST;
    

template <class ValueType>
class BSTNode {
    friend class BST<ValueType>;
    private:
        int key;
        ValueType value; 
        BSTNode* children[2];


        using SafeBSTNode = SafeNode<BSTNode<ValueType>>;

    public:
        BSTNode(int key, ValueType val, BSTNode* left_child, BSTNode* right_child): key(key), value(val) {
            children[0] = left_child;
            children[1] = right_child;
        }

        bool hasKey(int key_requested) {
            return key == key_requested;
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

        BSTNode** getChildPointer(int i) {
            assert(i >= 0 && i < 2);
            return &children[i];
        }

        BSTNode* getChild(int i) {
            assert(i >= 0 && i < 2);
            return children[i];
        }



        BSTNode* getL() {
            return children[0];
        }

        BSTNode* getR() {
            return children[1];
        }


        void setChild(int i, BSTNode* node) {
            assert(i >= 0 && i < 2);
            children[i] = node;
        }

        BSTNode* setL(BSTNode* n) {
            children[0] = n;
        }

        BSTNode* setR(BSTNode* n) {
            children[1] = n;
        }

        BSTNode** getChildren() {
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

        int nextChild(const BSTNode* target) const {
            return target->key < key ? 0 : 1;
        }   
};

template <class ValueType>
struct Result {
    bool found;
    ValueType val;

};

template <class ValueType>
class BST {
    friend class BSTNode<ValueType>;
    private:
        BSTNode<ValueType>* root;
        TSX::SpinLock &_lock;
        using TreeNode = BSTNode<ValueType>;
        const int trans_retries = 30;
        

        // helpers

        static std::size_t key_sum_helper(TreeNode* node) {
            if (!node) {
                return 0;
            } 

            return node->getKey() + key_sum_helper(node->getChild(0)) + key_sum_helper(node->getChild(1));
        }

        static int count_nodes(TreeNode* node) {
            if (!node) {
                return 0;
            }

            return 1 + count_nodes(node->getChild(0)) + count_nodes(node->getChild(1));
        }


        void node_copy(BSTNode<ValueType>* curr, BSTNode<ValueType>* copy_curr = nullptr) {
            if (!curr) return;

            if (!copy_curr) {
                copy_curr = new BSTNode<ValueType>*(curr);
            }

            copy_curr->setChild(0, curr->getChild(0));
            copy_curr->setChild(1, curr->getChild(1));

            node_copy(curr->getChild(0), copy_curr->getChild(0));
            node_copy(curr->getChild(1), copy_curr->getChild(1));
        }

        void print_contents(BSTNode<ValueType>* root) {
            if (!root) {
                return;
            }

            std::cout << root->key << " ";
            print_contents(root->getChild(0));
            print_contents(root->getChild(1));
        }

        void print_sorted_contents(BSTNode<ValueType>* root) {
            if (!root) {
                return;
            }

            print_sorted_contents(root->getChild(0));
            std::cout << root->key << " ";
            print_sorted_contents(root->getChild(1));
        }

        void print_depth(BSTNode<ValueType>* root,int depth) {
            if (depth <= 0 || !root) {
                return;
            }

            std::cout << root->key << " ";
            print_depth(root->getChild(0), depth-1);
            print_depth(root->getChild(1), depth-1);
        }

        int longest_branch(BSTNode<ValueType>* root) {
            if (!root) {
                return 0;
            }

            const int left_branch_length = longest_branch(root->getChild(0));
            const int right_branch_length = longest_branch(root->getChild(1));

            return  left_branch_length > right_branch_length? 1 + left_branch_length: 
                    1 + right_branch_length;
        }
        
        void averageBranchHelper(BSTNode<ValueType>* root,int& total_leaves, int& total_length, int curr_branch_length = 1) {
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

        int averageBranchLength(BSTNode<ValueType>* root) {
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


        void rec_delete(TreeNode* node) {
            if (!node) return;

            rec_delete(node->getChild(0));
            rec_delete(node->getChild(1));

            delete node;
        }

        bool insert_impl(const int k, ValueType val, int t_id) {

            int retries = trans_retries;
            
            TM_SAFE_OPERATION_START {

                TSX::Transaction t(retries,_lock, stats[t_id]);

                /* FIND PHASE */

                
                auto conn_point_snapshot = find_conn_point<TreeNode>(k,&root);


                if (conn_point_snapshot.found()) {
                    return false;
                }


                    /* FIND PHASE END */

                ConnPoint<TreeNode> conn(conn_point_snapshot, t);

                /* INSERT */

                // build new node
                #ifdef USER_NODE_POOL
                    auto node_to_be_inserted = conn.create_safe(ConnPoint<TreeNode>::create_new_node(k,val,nullptr,nullptr));
                #else
                    auto node_to_be_inserted = conn.create_safe(new BSTNode<ValueType>(k,val,nullptr,nullptr));
                #endif


                // insert it
                conn.setRoot(node_to_be_inserted);
                
            } TM_SAFE_OPERATION_END

            // OPERATION END can be omitted if
            // not using EARLY ABORT COMPILATION FLAGS
            
            
       
            return true;

        }




    bool remove_impl(const int k, const int t_id) {
        
        int retries = trans_retries;

        TM_SAFE_OPERATION_START {
            TSX::Transaction t(retries,_lock, stats[t_id]);

            /* FIND PHASE */

                
            auto conn_point_snapshot = find_conn_point<TreeNode>(k,&root);


            if (!conn_point_snapshot.found()) {
                return false;
            }


            ConnPoint<TreeNode> conn(conn_point_snapshot, t);

            // the connection point is above the
            // node which will be deleted

            // thus the node to be deleted will be
            // the root of the copied tree
            auto node_to_be_deleted = conn.getRoot();

            // read only, to see if they exist
            auto its_left_child = node_to_be_deleted->peekChild(0); 
            auto its_right_child = node_to_be_deleted->peekChild(1);

            if (!its_right_child && !its_left_child) {
                conn.setRoot(nullptr);
            } else if (!its_right_child) {
                // get left child and replace node to be deleted with it
                auto left_child = node_to_be_deleted->getChild(0);
                conn.setRoot(left_child);
            } else {
                // get right child and search for the lowest leaf
                // going left
                SafeNode<TreeNode>* prev = nullptr;
                auto curr = node_to_be_deleted->getChild(1);
                for (; curr && curr->getChild(0); prev = curr, curr = curr->getChild(0));

                auto node_to_replace_root = curr;


               
                // to modify a node's internals we need a safe reference
                // that causes a copy of the node
                TreeNode* node_to_be_deleted_copy = node_to_be_deleted->rwRef();
                // no need to copy node we will be removing
                // just look at the original to get its key and value
                const TreeNode* node_to_replace_view = node_to_replace_root->peekOriginal();

                auto new_key = node_to_replace_view->getKey();
                auto new_value = node_to_replace_view->getValue();

                node_to_be_deleted_copy->setKey(new_key);
                node_to_be_deleted_copy->setValue(new_value);

                // new node is fine, now
                // remove old node

                // to remove it, its right child should take its place

                // get the right child
                // and wrap it to be able to
                // insert it into the copied tree
                auto right_child = conn.create_safe(node_to_replace_root->peekChild(1));
                if (prev) {
                    prev->setChild(0,right_child);
                } else {
                    node_to_be_deleted->setChild(1,right_child);
                }
            }
        } TM_SAFE_OPERATION_END

        return true;
    }




    public:

    BST(TreeNode* root, TSX::SpinLock &lock): root(root), _lock(lock){
        #ifdef USER_NODE_POOL
            ConnPoint<TreeNode>::init_node_pool();
        #endif

        for (int i = 0; i < THREAD_AMOUNT_MAX; i++) {
            stats[i].reset();
        }
    }
    ~BST() {
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

    BSTNode<ValueType>* getRoot() {
        return root;
    }

    void setRoot(BSTNode<ValueType>* node) {
        root = node;
    }

    std::size_t key_sum() {
        return key_sum_helper(root);
    }

    bool isSorted() {
        if (!root) {
            return true;
        }

        return isBstHelper(root,std::numeric_limits<int>::min(),std::numeric_limits<int>::max());
    }



    Result<ValueType> lookup(int desired_key) {
        auto node = find<TreeNode>(root,desired_key);
        
        const auto found = node != nullptr;
    
        return {found, found ? node->getValue(): ValueType()};
    }

    int find_conn(int desired_key) {
        auto conn_point_snapshot = find_conn_point<TreeNode>(desired_key,&root);
        return conn_point_snapshot.con_ptr.child_index;
    }


    bool remove(int k, int t_id) {
        return remove_impl(k,t_id);
    }


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


};



#endif