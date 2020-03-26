#define USER_NODE_POOL USER_MEM_POOL
#define TREE_TYPE GENERAL_TREE

#include <iostream>
#include "../../../include/SafeTree.hpp"



using namespace SafeTree;

template <class ContentType>
class QueueItem {
    private:
        ContentType item;
        QueueItem* next;
    public:
        QueueItem(ContentType item, QueueItem* next): item(item), next(next) {}
        QueueItem(QueueItem& item): item(item.item), next(item.next){}

        QueueItem& operator=(QueueItem other) {
            item = other.item;
            next = other.next;

            return *this;
        }

        ~QueueItem(){}

        QueueItem* getChild(int i) {
            (void)i;
            return next;
        }

        void setChild(int i, QueueItem* new_item) {
            (void)i;
            next = new_item;
        }

        QueueItem** getChildPointer(int i) {
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
class Queue {
    private:
        QueueItem<ContentType>* top_item;
        using Item =  QueueItem<ContentType>;

        // last_elem: connection point at the last element
        // of the list
        ConnPointData<QueueItem<ContentType>> last_elem() {
            PathTracker<Item> tracker(&top_item);

            auto curr = tracker.getNode();
            while ((curr = tracker.getNode()))
            {
                tracker.moveToChild(0);
            }

            return tracker.connectHere();
        }

    
    public:
        Queue(): top_item(nullptr) {}
        QueueItem<ContentType>* next() {
            return top_item;
        }

        const QueueItem<ContentType>* dequeue() {
            // transaction block
            TM_SAFE_OPERATION_START(30) {

                // PathTracker makes root the connection point
                PathTracker<Item> tracker(&top_item);

                auto conn_point_snapshot = tracker.connectHere();

                ConnPoint<Item> conn(conn_point_snapshot);

                // remove the root element
                // replace with the next element
                auto top = conn.getRoot();
                auto const old_top = top? top->peekOriginal() : nullptr;

                if (!top) {
                    return nullptr;
                }

                auto after_top = top->getChild(0);

                // set the next element
                conn.setRoot(after_top);

                return old_top;
            } TM_SAFE_OPERATION_END

            return nullptr;

        }

        void enqueue(ContentType content) {
            TM_SAFE_OPERATION_START(30) {

                // last elem is connection point
                auto conn_point_snapshot = last_elem();

                ConnPoint<Item> conn(conn_point_snapshot);

                auto top = conn.getRoot();

                #ifdef USER_NODE_POOL
                    auto node_to_be_inserted = conn.create_safe(ConnPoint<Item>::create_new_node(content, nullptr));
                #else
                    auto node_to_be_inserted = conn.create_safe(new QueueItem<ContentType>(content, nullptr));
                #endif

                // merely append the new node
                conn.setRoot(node_to_be_inserted);

                node_to_be_inserted->setChild(0, top);
            } TM_SAFE_OPERATION_END
        }
};