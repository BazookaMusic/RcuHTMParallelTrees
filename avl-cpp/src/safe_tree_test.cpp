#include <chrono>
#include <sstream>
#include <iostream>
#include <ctime>
#include <thread>

#include "include/avl.hpp"
#include "include/catch.hpp"
#include "include/SafeTree.hpp"
#include "include/urcu.hpp"

#include "utils/test_helpers.cpp"

typedef AVLNode<int> TestNode;

bool InsPointCreationTest() {
    auto a = new AVLNode<int>(1,2,nullptr,nullptr);

    std::unique_ptr<SearchTreeStack<AVLNode<int>>>
                      myPath(new SearchTreeStack<AVLNode<int>>());

    InsPoint<AVLNode<int>> ins = InsPoint<AVLNode<int>>(a,myPath);

    // check if original pointer is properly saved
    if (ins.get_head()->getOriginal() == a)
    {
        return true;
    }

    return false;

}

bool tree_validate(TestNode* root, SafeNode<TestNode>* safe_root) {
    
    if ( (!root && safe_root) || (root && !safe_root)) {
        return false;
    }

    if (!root && !safe_root) {
        return true;
    }

    bool isValid = true;

    for (int i = 0; i < root->nChildren(); i++) {
        isValid &= tree_validate(root->getChild(i), safe_root->getChild(i));
    }

    if (!(root == safe_root->getOriginal())) {
        std::cout<< "DIFFERENT NODES" << root << " " << safe_root << std::endl;
    }

    return root == safe_root->getOriginal();
}

bool TreeTraversalTest() {
    auto tree = AVLTree<int>();
    
    for (int i = 0; i < 1000000; i++) {
        tree.insert(i,i);
    }
   

    std::unique_ptr<SearchTreeStack<AVLNode<int>>>
                      myPath(new SearchTreeStack<AVLNode<int>>());

    InsPoint<AVLNode<int>> ins = InsPoint<AVLNode<int>>(tree.getRoot(),myPath);

    auto root = ins.get_head()->getOriginal();

    if (root != tree.getRoot()) {
        return false;
    }

    return tree_validate(root, ins.get_head());
}



TEST_CASE("InsPointCreationTest Test", "[rcu]") {
    std::cout << "InsPoint Creation TEST" <<std::endl;
    REQUIRE(InsPointCreationTest());
    std::cout << "InsPoint Traversal TEST" <<std::endl;
    REQUIRE(TreeTraversalTest());

    
}





