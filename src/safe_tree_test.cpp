#include <chrono>
#include <sstream>
#include <iostream>
#include <ctime>
#include <thread>
#include <deque>

#include "include/avl.hpp"
#include "include/catch.hpp"
#include "include/SafeTree.hpp"
#include "include/urcu.hpp"


#include "utils/test_helpers.cpp"


using TestNode = AVLNode<int>; 
const int INSERT_N = 1000000;



bool InsPointCreationTest() {
    auto a = new AVLNode<int>(1,2,nullptr,nullptr);

    auto myPath = TreePathStack<AVLNode<int>>();

    InsPoint<AVLNode<int>> ins(a,myPath);

    // check if original pointer is properly saved
    if (ins.getHead()->getOriginal() == a)
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

void tree_trav_copy(SafeNode<TestNode>* safe_root) {
    if (!safe_root) {return;}

    tree_trav_copy(safe_root->getChild(0));
    tree_trav_copy(safe_root->getChild(1));
}

void tree_copy(TestNode* root, TestNode* real_root, std::deque<std::pair<TestNode*,TestNode>>& deq) {
    if (!root) {
        return;
    }

    auto left = root->getL();
    auto right = root->getR();

    auto copy = new TestNode(root->key,root->val,left,right);

    if (!real_root) {
        real_root = copy;
    }

    deq.push_back(std::make_pair(root,*root));

    tree_copy(left, real_root, deq);
    tree_copy(right, real_root, deq);

}



bool TreeTraversalTest() {
    auto tree = AVLTree<int>();
    
    for (int i = 0; i < INSERT_N; i++) {
        tree.insert(i,i);
    }
   

    auto myPath = TreePathStack<AVLNode<int>>();

    InsPoint<AVLNode<int>> ins(tree.getRoot(),myPath);

    auto root = ins.getHead()->getOriginal();

    if (root != tree.getRoot()) {
        return false;
    }

    tree_trav_copy(ins.getHead());
    return true;
   // return tree_validate(root, ins.getHead());
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
    

    auto myPath = TreePathStack<AVLNode<int>>();

    InsPoint<AVLNode<int>> ins(tree.getRoot(),myPath);

    auto root = ins.getHead()->getOriginal();

    if (root != tree.getRoot()) {
        return false;
    }


    return tree_build(ins.getHead());
}



bool TreeCopyTest() {
    auto tree = AVLTree<int>();
    
    for (int i = 0; i < INSERT_N; i++) {
        tree.insert(i,i);
    }

    auto myPath = TreePathStack<AVLNode<int>>();

    InsPoint<AVLNode<int>> ins(tree.getRoot(),myPath);

    auto root = ins.getHead()->getOriginal();

    if (root != tree.getRoot()) {
        return false;
    }

   std::deque<std::pair<TestNode*,TestNode>> deq;

    tree_copy(root, nullptr, deq);

     for (auto it = deq.begin(); it != deq.end(); ++it)  {
        delete it->first;
    }


    return true;
}


TEST_CASE("InsPointCreationTest Test", "[rcu]") {
    std::cout << "InsPoint Creation Test" <<std::endl;
    REQUIRE(InsPointCreationTest());
    std::cout << "InsPoint Traversal Test" <<std::endl;
    REQUIRE(TreeTraversalTest());
    std::cout << "InsPoint New Tree Test" <<std::endl;
    REQUIRE(TreeBuildTest());
    //REQUIRE_NOTHROW(TreeCopyTest());
}





