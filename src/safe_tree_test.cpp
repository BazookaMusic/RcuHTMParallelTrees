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
#include "include/tsl/robin_map.h"

using TestNode = AVLNode<int>; 
const int INSERT_N = 10000000;



bool InsPointCreationTest() {
    auto a = new AVLNode<int>(1,2,nullptr,nullptr);

    auto myPath = SearchTreeStack<AVLNode<int>>();

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

void tree_copy(TestNode* root, TestNode* real_root, tsl::robin_map<TestNode*,TestNode>& map) {
    if (!root) {
        return;
    }

    auto left = root->getL();
    auto right = root->getR();

    auto copy = new TestNode(root->key,root->val,left,right);

    if (!real_root) {
        real_root = copy;
    }

    map.insert(std::make_pair(root,*root));

    tree_copy(left, real_root, map);
    tree_copy(right, real_root, map);

}



bool TreeTraversalTest() {
    auto tree = AVLTree<int>();
    
    for (int i = 0; i < INSERT_N; i++) {
        tree.insert(i,i);
    }
   

    auto myPath = SearchTreeStack<AVLNode<int>>();

    InsPoint<AVLNode<int>> ins = InsPoint<AVLNode<int>>(tree.getRoot(),myPath);

    auto root = ins.get_head()->getOriginal();

    if (root != tree.getRoot()) {
        return false;
    }


    return tree_validate(root, ins.get_head());
}

void build(SafeNode<TestNode>* safe_root) {

   
    for(int i = 0; i < INSERT_N; i++){
        safe_root->setNewChild(0, new TestNode(1,2,nullptr,nullptr));
        safe_root->setNewChild(1, new TestNode(1,2,nullptr,nullptr));
        safe_root = safe_root->getChild(0);
    }
    
    
}

bool tree_build( SafeNode<TestNode>* safe_root) {

    safe_root->clipTree(0);
    safe_root->clipTree(1);
     
    build(safe_root);


    return true;

}


bool TreeBuildTest() {
    auto tree = AVLTree<int>();
    
    for (int i = 0; i < 1; i++) {
        tree.insert(i,i);
    }
    

    auto myPath = SearchTreeStack<AVLNode<int>>();

    InsPoint<AVLNode<int>> ins = InsPoint<AVLNode<int>>(tree.getRoot(),myPath);

    auto root = ins.get_head()->getOriginal();

    if (root != tree.getRoot()) {
        return false;
    }


    return tree_build(ins.get_head());
}



bool TreeCopyTest() {
    auto tree = AVLTree<int>();
    
    for (int i = 0; i < INSERT_N; i++) {
        tree.insert(i,i);
    }

    auto myPath = SearchTreeStack<AVLNode<int>>();

    InsPoint<AVLNode<int>> ins = InsPoint<AVLNode<int>>(tree.getRoot(),myPath);

    auto root = ins.get_head()->getOriginal();

    if (root != tree.getRoot()) {
        return false;
    }

    tsl::robin_map<TestNode*,TestNode> a_map;

    tree_copy(root, nullptr, a_map);


    return true;
}


TEST_CASE("InsPointCreationTest Test", "[rcu]") {
    std::cout << "InsPoint Creation TEST" <<std::endl;
    REQUIRE(InsPointCreationTest());
    std::cout << "InsPoint Traversal TEST" <<std::endl;
    //REQUIRE(TreeTraversalTest());
    REQUIRE(TreeBuildTest());
    //REQUIRE_NOTHROW(TreeCopyTest());
}





