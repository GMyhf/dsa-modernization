#pragma once
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

namespace dsa::advanced {

class AvlTree {
    struct Node { int key; int height{1}; std::unique_ptr<Node> left,right; explicit Node(int k):key(k){} };
    std::unique_ptr<Node> root_;
    static int h(const std::unique_ptr<Node>& n){return n?n->height:0;}
    static void fix(Node* n){n->height=1+std::max(h(n->left),h(n->right));}
    static int bf(const std::unique_ptr<Node>& n){return n? h(n->right)-h(n->left):0;}
    static std::unique_ptr<Node> left(std::unique_ptr<Node> x){auto y=std::move(x->right);x->right=std::move(y->left);fix(x.get());y->left=std::move(x);fix(y.get());return y;}
    static std::unique_ptr<Node> right(std::unique_ptr<Node> y){auto x=std::move(y->left);y->left=std::move(x->right);fix(y.get());x->right=std::move(y);fix(x.get());return x;}
    static std::unique_ptr<Node> insert(std::unique_ptr<Node> n,int k){if(!n)return std::make_unique<Node>(k);if(k<n->key)n->left=insert(std::move(n->left),k);else if(k>n->key)n->right=insert(std::move(n->right),k);else return n;fix(n.get());int b=bf(n);if(b>1){if(k>n->right->key)return left(std::move(n));n->right=right(std::move(n->right));return left(std::move(n));}if(b<-1){if(k<n->left->key)return right(std::move(n));n->left=left(std::move(n->left));return right(std::move(n));}return n;}
    static std::unique_ptr<Node> erase(std::unique_ptr<Node> n,int k){if(!n)return n;if(k<n->key)n->left=erase(std::move(n->left),k);else if(k>n->key)n->right=erase(std::move(n->right),k);else{if(!n->left)return std::move(n->right);if(!n->right)return std::move(n->left);Node* s=n->right.get();while(s->left)s=s->left.get();n->key=s->key;n->right=erase(std::move(n->right),s->key);}fix(n.get());int b=bf(n);if(b>1){if(bf(n->right)<0)n->right=right(std::move(n->right));return left(std::move(n));}if(b<-1){if(bf(n->left)>0)n->left=left(std::move(n->left));return right(std::move(n));}return n;}
public:
    void insert(int k){root_=insert(std::move(root_),k);} void erase(int k){root_=erase(std::move(root_),k);} [[nodiscard]] bool contains(int k)const{auto*n=root_.get();while(n){if(k==n->key)return true;n=k<n->key?n->left.get():n->right.get();}return false;} [[nodiscard]] int height()const{return h(root_);}
};

class SplayTree {
    struct Node { int key; std::unique_ptr<Node> left,right; explicit Node(int k):key(k){} };
    std::unique_ptr<Node> root_;
    static void rotate_left(std::unique_ptr<Node>& t){auto r=std::move(t->right);t->right=std::move(r->left);r->left=std::move(t);t=std::move(r);}
    static void rotate_right(std::unique_ptr<Node>& t){auto l=std::move(t->left);t->left=std::move(l->right);l->right=std::move(t);t=std::move(l);}
    static void splay(std::unique_ptr<Node>& t,int k){if(!t||t->key==k)return;if(k<t->key){if(!t->left)return;if(k<t->left->key){splay(t->left->left,k);rotate_right(t);}else if(k>t->left->key){splay(t->left->right,k);if(t->left->right)rotate_left(t->left);}if(t->left)rotate_right(t);}else{if(!t->right)return;if(k>t->right->key){splay(t->right->right,k);rotate_left(t);}else if(k<t->right->key){splay(t->right->left,k);if(t->right->left)rotate_right(t->right);}if(t->right)rotate_left(t);}}
    static void insert_node(std::unique_ptr<Node>& n,int k){if(!n){n=std::make_unique<Node>(k);return;}splay(n,k);if(n->key==k)return;auto x=std::make_unique<Node>(k);if(k<n->key){x->right=std::move(n);x->left=std::move(x->right->left);n=std::move(x);}else{x->left=std::move(n);x->right=std::move(x->left->right);n=std::move(x);}}
public:
    void insert(int k){insert_node(root_,k);}
    [[nodiscard]] bool contains(int k){splay(root_,k);return root_&&root_->key==k;}
    [[nodiscard]] int root_key()const{return root_?root_->key:0;}
    [[nodiscard]] bool empty()const noexcept{return !root_;}
};
}
