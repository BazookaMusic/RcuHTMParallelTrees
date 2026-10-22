#define USER_NODE_POOL USER_MEM_POOL
#define TREE_TYPE GENERAL_TREE

#include <iostream>
#include "../../../include/SafeTree.hpp"



using namespace SafeTree;

template <class ContentType>
class StackItem {
    private:
        ContentType item;
        StackItem* next;
    public:
        StackItem(ContentType item, StackItem* next): item(item), next(next) {}
        StackItem(StackItem& item): item(item.item), next(item.next){}

        StackItem& operator=(StackItem other) {
            item = other.item;
            next = other.next;

            return *this;
        }

        ~StackItem(){}

        StackItem* getChild(int i) {
            (void)i;
            return next;
        }

        void setChild(int i, StackItem* new_item) {
            (void)i;
            next = new_item;
        }

        StackItem** getChildPointer(int i) {
            (void)i;
            return &next;
        }

        static constexpr int maxChildren() {
            return 1;
        }

        ContentType getItem() const {
            return item;
        }
};

template <class ContentType>
class Stack {
    private:
        StackItem<ContentType>* top_item;
        using Item =  StackItem<ContentType>;

    
    public:
        Stack(): top_item(nullptr) {}
        StackItem<ContentType>* top() {
            return top_item;
        }

        const StackItem<ContentType>* pop() {
            // the popped item can only be returned after the
            // operation has been committed, returning from inside
            // the transaction block would skip the retry on a
            // failed validation
            const StackItem<ContentType>* old_top = nullptr;

            TM_SAFE_OPERATION_START(30) {
                PathTracker<Item> tracker(&top_item);

                auto conn_point_snapshot = tracker.connectHere();

                ConnPoint<Item> conn(conn_point_snapshot);

                auto top = conn.getRoot();
                old_top = top? top->peekOriginal() : nullptr;

                if (top) {
                    auto after_top = top->getChild(0);

                    conn.setRoot(after_top);
                }
            } TM_SAFE_OPERATION_END

            return old_top;

        }

        void push(ContentType content) {
            TM_SAFE_OPERATION_START(30) {

                PathTracker<Item> tracker(&top_item);

                auto conn_point_snapshot = tracker.connectHere();

                ConnPoint<Item> conn(conn_point_snapshot);

                auto top = conn.getRoot();

                #ifdef USER_NODE_POOL
                    auto node_to_be_inserted = conn.create_safe(ConnPoint<Item>::create_new_node(content, nullptr));
                #else
                    auto node_to_be_inserted = conn.create_safe(new StackItem<ContentType>(content, nullptr));
                #endif

                conn.setRoot(node_to_be_inserted);

                node_to_be_inserted->setChild(0, top);
            } TM_SAFE_OPERATION_END
        }
};