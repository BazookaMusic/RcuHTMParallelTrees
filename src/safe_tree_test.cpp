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
#include "include/TSXGuard.hpp"



#include "utils/test_helpers.cpp"


using TestNode = AVLNode<int>; 
const int INSERT_N = 1000000;

TSX::SpinLock lock;




bool ConnPointCreationTest() {
    auto a = new AVLNode<int>(1,2,nullptr,nullptr);

    auto myPath = TreePathStackWithIndex<AVLNode<int>>();

    ConnPoint<AVLNode<int>> ins(nullptr,&a,myPath,lock);

    // check if original pointer is properly saved
    if (ins.getConnPoint() == nullptr)
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

    for (int i = 0; i < TestNode::maxChildren(); i++) {
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

    auto copy = new TestNode(root->getKey(),root->val,left,right);

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
   

    auto myPath = TreePathStackWithIndex<AVLNode<int>>();

    ConnPoint<AVLNode<int>> ins(nullptr,tree.getRootPointer(),myPath,lock);

    auto root = ins.safeTreeAtRoot();

    if (root->getOriginal() != tree.getRoot()) {
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
    
    for (int i = 0; i < INSERT_N; i++) {
        tree.insert(i,i);
    }
    

    auto myPath = TreePathStackWithIndex<AVLNode<int>>();

    ConnPoint<AVLNode<int>> ins(nullptr,tree.getRootPointer(),myPath,lock);

    auto root = ins.safeTreeAtRoot();

    if (root->getOriginal() != tree.getRoot()) {
        return false;
    }


    return tree_build(ins.getHead());
}

bool inTheMiddleTest() {
     auto tree = AVLTree<int>();
    
    for (int i = 0; i < 1000; i++) {
        tree.insert(i,i);
    }

    auto myPath = TreePathStackWithIndex<AVLNode<int>>();

    const auto left_child_orig = tree.getRoot()->getL();
    const auto left_right_child_orig = left_child_orig->getR();

    const auto left_right_left_orig = left_right_child_orig->getL();

    myPath.push(left_child_orig,1);
    myPath.push(left_right_child_orig,0);
    
    ConnPoint<AVLNode<int>> ins(left_right_child_orig,tree.getRootPointer(),myPath,lock);

    // ins point is left child of left right child
    auto copy_root = ins.copyChild(0);

    copy_root->setNewChild(0,new TestNode(1,2,nullptr,nullptr));

    ins.connect_copy();

    const auto left_child = tree.getRoot()->getL();
    const auto left_right_child = left_child->getR();
    const auto left_right_left = left_right_child->getL();

   


    bool success = left_right_left->getL()->getKey() == 1 &&
        left_right_left->getKey() == left_right_left_orig->getKey();



    return success;
}

bool rootChangeTest() {
     auto tree = AVLTree<int>();
    
    for (int i = 0; i < 10; i++) {
        tree.insert(i,i);
    }

    auto myPath = TreePathStackWithIndex<AVLNode<int>>();

    
    ConnPoint<AVLNode<int>> ins(nullptr,tree.getRootPointer(),myPath,lock);

    ins.createNewTree(new TestNode(222222,2,nullptr,nullptr));

    ins.connect_copy();

    return tree.getRoot()->getKey() == 222222;
}

bool CopyConnectionTest() {
    return inTheMiddleTest() && rootChangeTest();

}

bool connectionPointNotInTree() {

    // building an example avl tree

     auto tree = AVLTree<int>();
    
    for (int i = 0; i < 100; i++) {
        tree.insert(i,i);
    }

    auto myPath = TreePathStackWithIndex<AVLNode<int>>();

    const auto left_child_orig = tree.getRoot()->getL();
    const auto left_right_child_orig = left_child_orig->getR();

    //const auto left_right_left_orig = left_right_child_orig->getL();

    myPath.push(left_child_orig,1);
    myPath.push(left_right_child_orig,0);
    
    ConnPoint<AVLNode<int>> ins(left_right_child_orig,tree.getRootPointer(),myPath,lock);

    // done

    // clip tree at left child, setting its right to null
    // connPoint disconnected
    left_child_orig->setChild(1, nullptr);

    return !ins.validate_copy();
}

bool childPointerInOriginalTreeChanged() {

    // building an example avl tree

    auto tree = AVLTree<int>();
    
    for (int i = 0; i < 100; i++) {
        tree.insert(i,i);
    }

    auto myPath = TreePathStackWithIndex<AVLNode<int>>();

    const auto left_child_orig = tree.getRoot()->getL();
    const auto left_right_child_orig = left_child_orig->getR();

    //const auto left_right_left_orig = left_right_child_orig->getL();

    myPath.push(left_child_orig,0);
    myPath.push(left_right_child_orig,1);
    
    ConnPoint<AVLNode<int>> ins(left_right_child_orig,tree.getRootPointer(),myPath,lock);

    auto rootCopy = ins.copyChild(0);
    auto itsLeftChild = rootCopy->getChild(0);
    rootCopy->getChild(1);

    // oops, clipped the original tree
    itsLeftChild->getOriginal()->setChild(0,nullptr);

    return !ins.validate_copy();
}


bool unalteredTree() {

    // building an example avl tree

     auto tree = AVLTree<int>();
    
    for (int i = 0; i < 100; i++) {
        tree.insert(i,i);
    }

    auto myPath = TreePathStackWithIndex<AVLNode<int>>();

    const auto left_child_orig = tree.getRoot()->getL();
    const auto left_right_child_orig = left_child_orig->getR();

    //const auto left_right_left_orig = left_right_child_orig->getL();

    myPath.push(left_child_orig, 0);
    myPath.push(left_right_child_orig, 1);
    
    ConnPoint<AVLNode<int>> ins(left_right_child_orig,tree.getRootPointer(),myPath,lock);

    auto rootCopy = ins.copyChild(0);
    rootCopy->getChild(0);
    rootCopy->getChild(1);


    return ins.validate_copy();
}


bool ValidationTest() {
    return connectionPointNotInTree() && childPointerInOriginalTreeChanged() && unalteredTree();
}

bool NullHeadPopPath() {
    const auto LEFT = 0;

    // create a tree

    TestNode* l_child = new TestNode(1,2,nullptr,nullptr);
    TestNode* root = new TestNode(1,2,l_child,nullptr);

    TreePathStackWithIndex<TestNode> myPath;

    myPath.push(root,LEFT);

    // connection point below root

    ConnPoint<TestNode> conn(root,&root,myPath,lock);

    // make a new
    conn.newSafeTreeAt(0, nullptr);

    // head is null
    // pop 
    auto newRoot = conn.pop_path();
    

    return newRoot->getOriginal() == root && conn.validate_copy() && conn.getHead() == newRoot;

}


bool nonNullHeadPop() {
    const auto LEFT = 0;

    // create a tree

    TestNode* l_child = new TestNode(1,2,nullptr,nullptr);
    TestNode* root = new TestNode(1,2,l_child,nullptr);

    TreePathStackWithIndex<TestNode> myPath;

    myPath.push(root,LEFT);

    // connection point below root

    ConnPoint<TestNode> conn(root,&root,myPath,lock);

    // make a new
    conn.copyChild(0);

    // pop 
    auto newRoot = conn.pop_path();
    

    return newRoot->getOriginal() == root && conn.validate_copy() && conn.getHead() == newRoot;

}

bool PopUntilNull() {
    const auto LEFT = 0;

    // create a tree

    TestNode* l_child = new TestNode(1,2,nullptr,nullptr);
    TestNode* root = new TestNode(1,2,l_child,nullptr);

    TreePathStackWithIndex<TestNode> myPath;

    myPath.push(root,LEFT);

    // connection point below root

    ConnPoint<TestNode> conn(root,&root,myPath,lock);

    // make a new
    conn.copyChild(0);

    // pop 
    conn.pop_path();
    auto shouldBeNull = conn.pop_path();
    

    return shouldBeNull == nullptr && conn.validate_copy();
}

bool oldHeadAndNewHeadConnect() {
    const auto LEFT = 0;

    // create a tree

    TestNode* l_child = new TestNode(1,2,nullptr,nullptr);
    TestNode* root = new TestNode(1,2,l_child,nullptr);

    TreePathStackWithIndex<TestNode> myPath;

    myPath.push(root,LEFT);

    // connection point below root

    ConnPoint<TestNode> conn(root,&root,myPath,lock);

    // make a new
    conn.copyChild(0);

    // pop 
    conn.pop_path();
    conn.pop_path();


    auto copyRoot = conn.getHead();

    if (!copyRoot) {
        return false;
    }


    

    return copyRoot->getOriginal() == root 
    && copyRoot->getChild(LEFT)->getOriginal() == l_child
    && conn.validate_copy();
}


bool PopPathTest() {
    return NullHeadPopPath() && nonNullHeadPop() & PopUntilNull() 
    && oldHeadAndNewHeadConnect();
}





TEST_CASE("ConnPoint Tests", "[ConnPoint]") {
    std::cout << "ConnPoint Creation Test" <<std::endl;
    REQUIRE(ConnPointCreationTest());
    std::cout << "ConnPoint Traversal Test" <<std::endl;
    REQUIRE(TreeTraversalTest());
    std::cout << "ConnPoint New Tree Test" <<std::endl;
    REQUIRE(TreeBuildTest());
    std::cout << "ConnPoint Copy Connection Test" <<std::endl;
    REQUIRE(CopyConnectionTest());
    std::cout << "ConnPoint Validation Test" <<std::endl;
    REQUIRE(ValidationTest());
    std::cout << "ConnPoint Pop Path Test" <<std::endl;
    REQUIRE(PopPathTest());
    
}





