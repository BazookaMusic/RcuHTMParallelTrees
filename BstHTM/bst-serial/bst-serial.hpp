template <class ValueType>
class BSTNodeSerial {
    private:
        int key;
        ValueType val;
        BSTNodeSerial* children[2];
    public:
        BSTNodeSerial(int key, ValueType val, BSTNodeSerial* left, BSTNodeSerial* right): key(key), val(val) {
            children[0] = left;
            children[1] = right;
        }


        BSTNodeSerial* getChild(int i) {
            return children[i];
        }

        void setChild(int i, BSTNodeSerial* new_child) {
            children[i] = new_child;
        }

        int nextChild(int target_key) const {
            return target_key < key ? 0 : 1;
        }

        bool traversalDone(int target_key) const {
            return key == target_key;
        }

        int getKey() const {
            return key;
        }

        ValueType getValue() const {
            return val;
        }

        void setKey(int new_key) {
           key = new_key;
        }

        void setValue(ValueType new_val) {
            val = new_val;
        }

};

template <class ValueType>
struct FindResult {
    BSTNodeSerial<ValueType>* prev;
    BSTNodeSerial<ValueType>* curr;
};

template <class ValueType>
class BSTSerial
{
private:
    using TreeNode = BSTNodeSerial<ValueType>;
    TreeNode* root;
    int size;

    void delete_rec(TreeNode* node) {
        if (!node) return;

        delete_rec(node->getChild(0));
        delete_rec(node->getChild(1));

        delete node;
    }
        


public:
    explicit BSTSerial(): root(nullptr), size(0) {}
    ~BSTSerial() {
        delete_rec(root);
    }

    FindResult<ValueType> find(int target_key) {
        auto curr = root;
        TreeNode* prev = nullptr;

        for (curr = root, prev = nullptr; curr && !curr->traversalDone(target_key); prev = curr, curr = curr->getChild(curr->nextChild(target_key)));

        FindResult<ValueType> res;
        res.curr = curr;
        res.prev = prev;

        return res;
    }

    int getSize() {
        return size;
    }

    bool contains(int target_key) {
        auto res = find(target_key);

        return res.curr != nullptr;
    }

    bool insert(int target_key, ValueType val) {
        auto find_key = find(target_key);

        if (find_key.curr != nullptr) {
            return false;
        }

        ++size;

        // empty tree
        if (find_key.prev == nullptr) {
            root = new TreeNode(target_key,val, nullptr, nullptr);
            return true;
        }

        auto point_of_insertion = find_key.prev;

        point_of_insertion->setChild(point_of_insertion->nextChild(target_key),new TreeNode(target_key,val, nullptr,nullptr));

        return true;
    }

    bool remove(int target_key) {
        auto find_key = find(target_key);

        if (find_key.curr == nullptr) {
            return false;
        }

        --size;

        auto point_of_insertion = find_key.curr;

        // no right children
        if (!point_of_insertion->getChild(1)) {
            auto prev =  find_key.prev;

            // remove at root
            if (!prev) {
                root = point_of_insertion->getChild(0);
                return true;
            }

            // not at root
            prev->setChild(prev->nextChild(target_key),point_of_insertion->getChild(0));
        } else {

            TreeNode* before_replacement = nullptr;
            auto replacement = point_of_insertion->getChild(1);

            for (; replacement->getChild(0); before_replacement = replacement, replacement = replacement->getChild(0));

            point_of_insertion->setKey(replacement->getKey());
            point_of_insertion->setValue(replacement->getValue());

            if (!before_replacement) {
                point_of_insertion->setChild(1, replacement->getChild(1));
            } else {
                before_replacement->setChild(0, replacement->getChild(1));
            }
        }

        return true;
    }
};
