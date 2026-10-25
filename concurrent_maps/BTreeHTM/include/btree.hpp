#ifndef INCLUDE_BTREE_HPP
#define INCLUDE_BTREE_HPP

#define USER_NODE_POOL USER_MEM_POOL

#define TREE_TYPE GENERAL_TREE


#include <cassert>
#include <limits>
#include <iostream>
#include "../../../include/SafeTree.hpp"


using namespace SafeTree;

/*
    A B-Tree of minimum degree MIN_DEGREE (t) built on top of SafeTree.

    - every node holds between t-1 and 2t-1 keys (the root between 1 and 2t-1)
    - an internal node with n keys has n+1 children
    - all leaves are at the same depth

    Concurrency follows the RCU/HTM scheme of the framework: nodes that are
    reachable from the root are never modified in place. An update traverses
    the tree, builds private copies of the nodes it needs to change and then
    atomically swaps a single child pointer (the connection point) inside a
    hardware transaction which also validates that nothing it read changed
    (or with an LLX/SCX when compiled with COMMIT_TYPE=LLX_SCX_COMMIT).

    Updates are bottom-up, so the tree of copies is only as tall as the
    modification needs to be: an insert into a non-full leaf copies just
    that leaf and connects it to the leaf's parent, the copy grows upwards
    (pop_path) only while splits (or underflows on remove) propagate.
    The root is replaced only when a modification actually reaches it.

    Moving pointers to untouched subtrees between private copies is done
    directly on the copies (rwRef()). Those pointers come from snapshots of
    nodes which are part of the validation set, so they are validated at
    commit time.
*/

template <class KeyType, class ValueType, int MIN_DEGREE>
class BTree;


template <class Key, class ValueType, int MIN_DEGREE = 4>
class BTreeNode: public NodeBase {
    static_assert(MIN_DEGREE >= 2, "B-Tree minimum degree should be at least 2");

    friend class BTree<Key, ValueType, MIN_DEGREE>;

    public:
        using KeyType = Key;

        static constexpr int maxKeys() {
            return 2 * MIN_DEGREE - 1;
        }

        static constexpr int minKeys() {
            return MIN_DEGREE - 1;
        }

        static constexpr int maxChildren() {
            return 2 * MIN_DEGREE;
        }

    private:
        int count;
        KeyType keys[2 * MIN_DEGREE - 1];
        ValueType values[2 * MIN_DEGREE - 1];
        BTreeNode* children[2 * MIN_DEGREE];

        // insertEntry: insert key/value at key_pos and child at child_pos,
        // node must not be full
        void insertEntry(int key_pos, KeyType key, ValueType val, int child_pos, BTreeNode* child) {
            assert(count < maxKeys());
            assert(key_pos >= 0 && key_pos <= count);
            assert(child_pos >= 0 && child_pos <= count + 1);

            for (int i = count; i > key_pos; i--) {
                keys[i] = keys[i - 1];
                values[i] = values[i - 1];
            }

            for (int i = count + 1; i > child_pos; i--) {
                children[i] = children[i - 1];
            }

            keys[key_pos] = key;
            values[key_pos] = val;
            children[child_pos] = child;

            ++count;
        }

        // removeEntry: remove key/value at key_pos and child at child_pos
        void removeEntry(int key_pos, int child_pos) {
            assert(count > 0);
            assert(key_pos >= 0 && key_pos < count);
            assert(child_pos >= 0 && child_pos <= count);

            for (int i = key_pos; i < count - 1; i++) {
                keys[i] = keys[i + 1];
                values[i] = values[i + 1];
            }

            for (int i = child_pos; i < count; i++) {
                children[i] = children[i + 1];
            }

            children[count] = nullptr;

            --count;
        }

        // mergeWith: append separator and the contents of right to this node
        void mergeWith(KeyType sep_key, ValueType sep_val, const BTreeNode* right) {
            assert(count + 1 + right->count <= maxKeys());

            keys[count] = sep_key;
            values[count] = sep_val;

            for (int i = 0; i < right->count; i++) {
                keys[count + 1 + i] = right->keys[i];
                values[count + 1 + i] = right->values[i];
            }

            for (int i = 0; i <= right->count; i++) {
                children[count + 1 + i] = right->children[i];
            }

            count += 1 + right->count;
        }

    public:
        BTreeNode(): count(0) {
            for (int i = 0; i < maxChildren(); i++) {
                children[i] = nullptr;
            }
        }

        BTreeNode(KeyType key, ValueType val): count(1) {
            keys[0] = key;
            values[0] = val;

            for (int i = 0; i < maxChildren(); i++) {
                children[i] = nullptr;
            }
        }

        int size() const {
            return count;
        }

        bool isLeaf() const {
            return children[0] == nullptr;
        }

        KeyType getKey(int i) const {
            assert(i >= 0 && i < count);
            return keys[i];
        }

        ValueType getValue(int i) const {
            assert(i >= 0 && i < count);
            return values[i];
        }

        // findIndex: index of the first key which is not smaller than key,
        // which is also the index of the child to follow
        int findIndex(KeyType key) const {
            int i = 0;
            while (i < count && keys[i] < key) {
                ++i;
            }

            return i;
        }

        BTreeNode** getChildPointer(int i) {
            assert(i >= 0 && i < maxChildren());
            return &children[i];
        }

        BTreeNode* getChild(int i) {
            assert(i >= 0 && i < maxChildren());
            return children[i];
        }

        void setChild(int i, BTreeNode* node) {
            assert(i >= 0 && i < maxChildren());
            children[i] = node;
        }

        bool hasKey(KeyType key) const {
            const int i = findIndex(key);
            return i < count && keys[i] == key;
        }

        bool traversalDone(KeyType key) const {
            return isLeaf() || hasKey(key);
        }

        int nextChild(KeyType key) const {
            return findIndex(key);
        }

        int nextChild(const BTreeNode* target) const {
            return findIndex(target->keys[0]);
        }
};


template <class ValueType>
struct BTreeResult {
    bool found;
    ValueType val;
};


template <class KeyType, class ValueType, int MIN_DEGREE = 4>
class BTree {
    public:
        using TreeNode = BTreeNode<KeyType, ValueType, MIN_DEGREE>;

    private:
        using SafeTreeNode = SafeNode<TreeNode>;

        TreeNode* root;
        TSX::SpinLock &_lock;
        static constexpr int TRANS_RETRIES = 30;


        template <typename ...Args>
        static TreeNode* new_node(Args&& ...args) {
            #ifdef USER_NODE_POOL
                return ConnPoint<TreeNode>::create_new_node(std::forward<Args>(args)...);
            #else
                return new TreeNode(std::forward<Args>(args)...);
            #endif
        }

        // split_insert: insert key/value at pos (and right_child right of it)
        // into a full node. The node keeps the lower half, a new node with the
        // upper half is returned and the median is returned through up_key/up_val
        static TreeNode* split_insert(TreeNode* node, int pos, KeyType key, ValueType val,
                                      TreeNode* right_child, KeyType& up_key, ValueType& up_val) {
            assert(node->count == TreeNode::maxKeys());

            KeyType all_keys[2 * MIN_DEGREE];
            ValueType all_values[2 * MIN_DEGREE];
            TreeNode* all_children[2 * MIN_DEGREE + 1];

            for (int i = 0, j = 0; i <= node->count; i++) {
                if (i == pos) {
                    all_keys[i] = key;
                    all_values[i] = val;
                } else {
                    all_keys[i] = node->keys[j];
                    all_values[i] = node->values[j];
                    ++j;
                }
            }

            for (int i = 0, j = 0; i <= node->count + 1; i++) {
                if (i == pos + 1) {
                    all_children[i] = right_child;
                } else {
                    all_children[i] = node->children[j];
                    ++j;
                }
            }

            // 2t keys: t stay left, the median goes up, t-1 go right
            TreeNode* right = new_node();

            node->count = MIN_DEGREE;
            for (int i = 0; i < MIN_DEGREE; i++) {
                node->keys[i] = all_keys[i];
                node->values[i] = all_values[i];
            }

            for (int i = 0; i < TreeNode::maxChildren(); i++) {
                node->children[i] = i <= MIN_DEGREE ? all_children[i] : nullptr;
            }

            right->count = MIN_DEGREE - 1;
            for (int i = 0; i < MIN_DEGREE - 1; i++) {
                right->keys[i] = all_keys[MIN_DEGREE + 1 + i];
                right->values[i] = all_values[MIN_DEGREE + 1 + i];
            }

            for (int i = 0; i < MIN_DEGREE; i++) {
                right->children[i] = all_children[MIN_DEGREE + 1 + i];
            }

            up_key = all_keys[MIN_DEGREE];
            up_val = all_values[MIN_DEGREE];

            return right;
        }

        // fix_underflow: the child at index ci of parent has too few keys,
        // borrow a key from a sibling or merge with it
        static void fix_underflow(SafeTreeNode* parent, int ci) {
            TreeNode* p = parent->rwRef();
            // child was already copied and linked by pop_path
            TreeNode* child = p->getChild(ci);

            // borrow from left sibling
            if (ci > 0 && p->getChild(ci - 1)->count > TreeNode::minKeys()) {
                TreeNode* left = parent->getChild(ci - 1)->rwRef();
                const int last = left->count - 1;

                child->insertEntry(0, p->keys[ci - 1], p->values[ci - 1], 0, left->children[last + 1]);
                p->keys[ci - 1] = left->keys[last];
                p->values[ci - 1] = left->values[last];
                left->removeEntry(last, last + 1);

                return;
            }

            // borrow from right sibling
            if (ci < p->count && p->getChild(ci + 1)->count > TreeNode::minKeys()) {
                TreeNode* right = parent->getChild(ci + 1)->rwRef();

                child->insertEntry(child->count, p->keys[ci], p->values[ci], child->count + 1, right->children[0]);
                p->keys[ci] = right->keys[0];
                p->values[ci] = right->values[0];
                right->removeEntry(0, 0);

                return;
            }

            // merge, always into the left node of the pair
            // (siblings are wrapped so that they get validated)
            if (ci > 0) {
                TreeNode* left = parent->getChild(ci - 1)->rwRef();
                left->mergeWith(p->keys[ci - 1], p->values[ci - 1], child);
                p->removeEntry(ci - 1, ci);
            } else {
                TreeNode* right = parent->getChild(ci + 1)->rwRef();
                child->mergeWith(p->keys[ci], p->values[ci], right);
                p->removeEntry(ci, ci + 1);
            }
        }

        bool insert_impl(const KeyType k, ValueType val, int t_id) {
            (void)t_id;

            return safe_operation(TRANS_RETRIES, [&]() -> bool {

                /* FIND PHASE */

                PathTracker<TreeNode> tracker(&root);

                TreeNode* curr;
                while ((curr = tracker.getNode())) {
                    const int i = curr->findIndex(k);

                    if (i < curr->count && curr->keys[i] == k) {
                        return false;
                    }

                    if (curr->isLeaf()) {
                        break;
                    }

                    tracker.moveToChild(i);
                }

                // connect at the parent of the leaf
                ConnPointData<TreeNode> conn_point_snapshot = tracker.connectHere();

                /* FIND PHASE END */

                ConnPoint<TreeNode> conn(conn_point_snapshot);

                /* INSERT */

                SafeTreeNode* current = conn.getRoot();

                if (!current) {
                    // empty tree
                    conn.setRoot(conn.create_safe(new_node(k, val)));
                } else {
                    TreeNode* leaf = current->rwRef();
                    const int pos = leaf->findIndex(k);

                    if (leaf->count < TreeNode::maxKeys()) {
                        leaf->insertEntry(pos, k, val, pos + 1, nullptr);
                    } else {
                        KeyType up_key;
                        ValueType up_val;

                        SafeTreeNode* split_off = conn.create_safe(split_insert(leaf, pos, k, val, nullptr, up_key, up_val));

                        /* SPLIT PROPAGATION */

                        // go up the path while nodes split
                        while (split_off) {
                            SafeTreeNode* parent = conn.pop_path();

                            if (!parent) {
                                // the root was split, make a new one
                                SafeTreeNode* new_root = conn.create_safe(new_node(up_key, up_val));
                                new_root->setChild(0, current);
                                new_root->setChild(1, split_off);
                                conn.setRoot(new_root);
                                break;
                            }

                            TreeNode* p = parent->rwRef();
                            const int ci = p->findIndex(up_key);
                            const KeyType median_key = up_key;
                            const ValueType median_val = up_val;

                            if (p->count < TreeNode::maxKeys()) {
                                p->insertEntry(ci, median_key, median_val, ci + 1, split_off->rwRef());
                                split_off = nullptr;
                            } else {
                                split_off = conn.create_safe(split_insert(p, ci, median_key, median_val, split_off->rwRef(), up_key, up_val));
                            }

                            current = parent;
                        }
                    }
                }
                return true;
            });
        }

        bool remove_impl(const KeyType k, int t_id) {
            (void)t_id;

            return safe_operation(TRANS_RETRIES, [&]() -> bool {

                /* FIND PHASE */

                PathTracker<TreeNode> tracker(&root);

                // depth of node containing k and of the leaf
                // where the actual removal happens
                int target_depth = -1;
                int depth = 0;

                TreeNode* curr;
                while ((curr = tracker.getNode())) {
                    const int i = curr->findIndex(k);

                    if (target_depth < 0 && i < curr->count && curr->keys[i] == k) {
                        target_depth = depth;
                    }

                    if (curr->isLeaf()) {
                        break;
                    }

                    // when k has been found in an internal node
                    // continue towards its predecessor
                    tracker.moveToChild(target_depth < 0 || target_depth == depth ? i : curr->count);
                    ++depth;
                }

                if (target_depth < 0) {
                    return false;
                }

                // connect at the parent of the leaf
                ConnPointData<TreeNode> conn_point_snapshot = tracker.connectHere();

                /* FIND PHASE END */

                ConnPoint<TreeNode> conn(conn_point_snapshot);

                /* REMOVE */

                SafeTreeNode* current = conn.getRoot();
                TreeNode* leaf = current->rwRef();

                KeyType pred_key = KeyType();
                ValueType pred_val = ValueType();

                if (target_depth == depth) {
                    const int pos = leaf->findIndex(k);
                    leaf->removeEntry(pos, pos + 1);
                } else {
                    // k is in an internal node, it will be
                    // replaced with its predecessor
                    const int last = leaf->count - 1;
                    pred_key = leaf->keys[last];
                    pred_val = leaf->values[last];
                    leaf->removeEntry(last, last + 1);
                }

                /* REBALANCE */

                // go up while there is an underflow or until
                // the node containing k has been copied
                while (depth > 0 && (depth > target_depth || current->rwRef()->count < TreeNode::minKeys())) {
                    SafeTreeNode* parent = conn.pop_path();
                    assert(parent);
                    --depth;

                    TreeNode* p = parent->rwRef();
                    // index of current in parent, computed before p changes
                    const int ci = p->findIndex(k);

                    if (depth == target_depth) {
                        assert(p->keys[ci] == k);
                        p->keys[ci] = pred_key;
                        p->values[ci] = pred_val;
                    }

                    if (current->rwRef()->count < TreeNode::minKeys()) {
                        fix_underflow(parent, ci);
                    }

                    current = parent;
                }

                // root without keys
                if (depth == 0 && current->rwRef()->count == 0) {
                    TreeNode* only_child = current->rwRef()->getChild(0);
                    conn.setRoot(only_child ? conn.create_safe(only_child) : nullptr);
                }

                return true;
            });
        }

        // helpers

        static std::size_t key_sum_helper(TreeNode* node) {
            if (!node) {
                return 0;
            }

            std::size_t sum = 0;

            for (int i = 0; i < node->count; i++) {
                sum += node->keys[i];
            }

            for (int i = 0; i <= node->count; i++) {
                sum += key_sum_helper(node->children[i]);
            }

            return sum;
        }

        static int count_keys(TreeNode* node) {
            if (!node) {
                return 0;
            }

            int sum = node->count;

            for (int i = 0; i <= node->count; i++) {
                sum += count_keys(node->children[i]);
            }

            return sum;
        }

        static int count_nodes(TreeNode* node) {
            if (!node) {
                return 0;
            }

            int sum = 1;

            for (int i = 0; i <= node->count; i++) {
                sum += count_nodes(node->children[i]);
            }

            return sum;
        }

        // keys are strictly increasing and within (min,max)
        static bool isSortedHelper(TreeNode* node, const KeyType* min, const KeyType* max) {
            if (!node) {
                return true;
            }

            for (int i = 0; i < node->count; i++) {
                if ((min && !(*min < node->keys[i])) || (max && !(node->keys[i] < *max))) {
                    return false;
                }

                if (i > 0 && !(node->keys[i - 1] < node->keys[i])) {
                    return false;
                }
            }

            for (int i = 0; i <= node->count; i++) {
                const KeyType* child_min = i > 0 ? &node->keys[i - 1] : min;
                const KeyType* child_max = i < node->count ? &node->keys[i] : max;

                if (!isSortedHelper(node->children[i], child_min, child_max)) {
                    return false;
                }
            }

            return true;
        }

        // key counts are within bounds, internal nodes have
        // count + 1 children, all leaves are at the same depth
        static bool isValidHelper(TreeNode* node, bool is_root, int depth, int& leaf_depth) {
            const int min_keys = is_root ? 1 : TreeNode::minKeys();

            if (node->count < min_keys || node->count > TreeNode::maxKeys()) {
                return false;
            }

            if (node->isLeaf()) {
                for (int i = 0; i < TreeNode::maxChildren(); i++) {
                    if (node->children[i]) {
                        return false;
                    }
                }

                if (leaf_depth < 0) {
                    leaf_depth = depth;
                }

                return leaf_depth == depth;
            }

            for (int i = 0; i < TreeNode::maxChildren(); i++) {
                if ((i <= node->count) != (node->children[i] != nullptr)) {
                    return false;
                }
            }

            for (int i = 0; i <= node->count; i++) {
                if (!isValidHelper(node->children[i], false, depth + 1, leaf_depth)) {
                    return false;
                }
            }

            return true;
        }

        static void averageBranchHelper(TreeNode* node, int& total_leaves, int& total_length, int curr_branch_length = 1) {
            if (!node) {
                return;
            }

            if (node->isLeaf()) {
                total_leaves += 1;
                total_length += curr_branch_length;
                return;
            }

            for (int i = 0; i <= node->count; i++) {
                averageBranchHelper(node->children[i], total_leaves, total_length, curr_branch_length + 1);
            }
        }

        static void print_contents(TreeNode* node, int depth) {
            if (!node) {
                return;
            }

            for (int i = 0; i < depth; i++) {
                std::cout << "  ";
            }

            for (int i = 0; i < node->count; i++) {
                std::cout << node->keys[i] << " ";
            }
            std::cout << std::endl;

            for (int i = 0; i <= node->count; i++) {
                print_contents(node->children[i], depth + 1);
            }
        }

        static void rec_delete(TreeNode* node) {
            if (!node) {
                return;
            }

            for (int i = 0; i <= node->count; i++) {
                rec_delete(node->children[i]);
            }

            delete node;
        }


    public:
        BTree(TreeNode* root, TSX::SpinLock &lock): root(root), _lock(lock) {
            #ifdef USER_NODE_POOL
                ConnPoint<TreeNode>::init_node_pool();
            #endif
        }

        ~BTree() {
            #ifndef USER_NODE_POOL
                rec_delete(root);
            #endif

            #ifdef USER_NODE_POOL
                ConnPoint<TreeNode>::reset_node_pool();
            #endif
        }

        BTree(const BTree&) = delete;
        BTree& operator=(const BTree&) = delete;

        bool insert(const KeyType k, ValueType val, int t_id) {
            return insert_impl(k, val, t_id);
        }

        bool remove(const KeyType k, int t_id) {
            return remove_impl(k, t_id);
        }

        BTreeResult<ValueType> lookup(const KeyType desired_key) {
            TreeNode* curr = root;

            while (curr) {
                const int i = curr->findIndex(desired_key);

                if (i < curr->count && curr->keys[i] == desired_key) {
                    return {true, curr->values[i]};
                }

                curr = curr->children[i];
            }

            return {false, ValueType()};
        }

        // amount of keys in the tree
        int size() {
            return count_keys(root);
        }

        int node_count() {
            return count_nodes(root);
        }

        TreeNode* getRoot() {
            return root;
        }

        TreeNode** getRootP() {
            return &root;
        }

        void setRoot(TreeNode* node) {
            root = node;
        }

        std::size_t key_sum() {
            return key_sum_helper(root);
        }

        bool isSorted() {
            return isSortedHelper(root, nullptr, nullptr);
        }

        // checks all the B-Tree invariants
        bool isValid() {
            if (!root) {
                return true;
            }

            int leaf_depth = -1;
            return isValidHelper(root, true, 0, leaf_depth) && isSorted();
        }

        // height in levels, all branches have the same length
        int height() {
            int h = 0;
            for (TreeNode* curr = root; curr; curr = curr->children[0]) {
                ++h;
            }

            return h;
        }

        void print() {
            print_contents(root, 0);
            std::cout << "Height is: " << height() << std::endl;
        }

        void longest_branch() {
            std::cout << "Longest branch is: " << height() << std::endl;
        }

        void average_branch() {
            int total_leaves = 0;
            int total_length = 0;

            averageBranchHelper(root, total_leaves, total_length);

            std::cout << "Average branch is: " << (total_leaves ? total_length / total_leaves : -1) << std::endl;
        }

        void lite_stat(int n_threads, long long n_ops = -1) {
            std::cout << "THREADS: " << n_threads;
            if (n_ops > 0) {
                std::cout << " OPS: " << n_ops;
            }
            std::cout << " KEYS: " << size() << " NODES: " << node_count() << " HEIGHT: " << height() << std::endl;
        }
};


#endif
