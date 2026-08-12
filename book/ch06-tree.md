# 第6章 树

左孩子/右兄弟表示保留一般树的任意度语义；先根、后根和广度周游均可复核。递归周游与销毁在极深树上有 Stack Overflow Risk。

```cpp file=code/ch06/general_tree/modern.hpp
// 原书【代码6.1】【代码6.2】【算法6.3】至【算法6.5】【代码6.6】至【代码6.8】【算法6.9】【算法6.10】。
#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>
namespace dsa {
// >>> general-tree
template <typename T>
class GeneralTree {
public:
    struct Node { T value; Node* child{nullptr}; Node* sibling{nullptr}; Node* parent{nullptr}; explicit Node(const T& v):value(v){} };
    GeneralTree()=default; GeneralTree(const GeneralTree& o):root_(clone(o.root_,nullptr)){} GeneralTree& operator=(const GeneralTree&o){if(this!=&o){GeneralTree c(o);swap(c);}return *this;} GeneralTree(GeneralTree&&o)noexcept:root_(o.release()){} GeneralTree& operator=(GeneralTree&&o)noexcept{if(this!=&o){clear();root_=o.release();}return *this;} ~GeneralTree(){clear();}
    void swap(GeneralTree&o)noexcept{using std::swap;swap(root_,o.root_);} [[nodiscard]] Node* root()noexcept{return root_;} [[nodiscard]] const Node* root()const noexcept{return root_;} void create_root(const T&v){clear();root_=new Node(v);} Node* insert_first(Node* p,const T&v){if(!p)throw std::invalid_argument("parent");Node*n=new Node(v);n->sibling=p->child;n->parent=p;p->child=n;return n;} Node* insert_next(Node* p,const T&v){if(!p)throw std::invalid_argument("sibling");Node*n=new Node(v);n->sibling=p->sibling;n->parent=p->parent;p->sibling=n;return n;}
    [[nodiscard]] Node* parent_of(Node*n)const noexcept{return n?n->parent:nullptr;} void delete_subtree(Node*n){if(!n)return;if(n==root_){root_=root_->sibling;n->sibling=nullptr;}else{Node**link=&n->parent->child;while(*link&&*link!=n)link=&(*link)->sibling;if(*link==n)*link=n->sibling;n->sibling=nullptr;}destroy(n);} void clear()noexcept{destroy(root_);root_=nullptr;}
    template<class V> void preorder(V&&v)const{pre(root_,v);} template<class V> void postorder(V&&v)const{post(root_,v);} template<class V> void breadth_first(V&&v)const{std::vector<Node*> q;for(Node*n=root_;n;n=n->sibling)q.push_back(n);for(std::size_t i=0;i<q.size();++i){v(q[i]->value);for(Node*c=q[i]->child;c;c=c->sibling)q.push_back(c);}}
private: static void destroy(Node*n)noexcept{if(n){destroy(n->child);destroy(n->sibling);delete n;}} static Node* clone(const Node*n,Node*p){if(!n)return nullptr;Node*c=new Node(n->value);c->parent=p;try{c->child=clone(n->child,c);c->sibling=clone(n->sibling,p);}catch(...){destroy(c);throw;}return c;} template<class V>static void pre(Node*n,V&v){for(;n;n=n->sibling){v(n->value);pre(n->child,v);}} template<class V>static void post(Node*n,V&v){for(;n;n=n->sibling){post(n->child,v);v(n->value);}} Node*release()noexcept{Node*r=root_;root_=nullptr;return r;} Node*root_{nullptr};
};
// <<< general-tree
// >>> disjoint-set
class DisjointSet { public: explicit DisjointSet(std::size_t n):parent_(n),rank_(n,0){for(std::size_t i=0;i<n;++i)parent_[i]=i;} std::size_t find(std::size_t x){if(x>=parent_.size())throw std::out_of_range("index");return parent_[x]==x?x:parent_[x]=find(parent_[x]);} bool unite(std::size_t a,std::size_t b){a=find(a);b=find(b);if(a==b)return false;if(rank_[a]<rank_[b])std::swap(a,b);parent_[b]=a;if(rank_[a]==rank_[b])++rank_[a];return true;} [[nodiscard]]bool same(std::size_t a,std::size_t b){return find(a)==find(b);} private:std::vector<std::size_t>parent_,rank_;};
// <<< disjoint-set
}
```

```cpp file=code/ch06/general_tree/modern.hpp
// 原书【代码6.1】【代码6.2】【算法6.3】至【算法6.5】【代码6.6】至【代码6.8】【算法6.9】【算法6.10】。
#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>
namespace dsa {
// >>> general-tree
template <typename T>
class GeneralTree {
public:
    struct Node { T value; Node* child{nullptr}; Node* sibling{nullptr}; Node* parent{nullptr}; explicit Node(const T& v):value(v){} };
    GeneralTree()=default; GeneralTree(const GeneralTree& o):root_(clone(o.root_,nullptr)){} GeneralTree& operator=(const GeneralTree&o){if(this!=&o){GeneralTree c(o);swap(c);}return *this;} GeneralTree(GeneralTree&&o)noexcept:root_(o.release()){} GeneralTree& operator=(GeneralTree&&o)noexcept{if(this!=&o){clear();root_=o.release();}return *this;} ~GeneralTree(){clear();}
    void swap(GeneralTree&o)noexcept{using std::swap;swap(root_,o.root_);} [[nodiscard]] Node* root()noexcept{return root_;} [[nodiscard]] const Node* root()const noexcept{return root_;} void create_root(const T&v){clear();root_=new Node(v);} Node* insert_first(Node* p,const T&v){if(!p)throw std::invalid_argument("parent");Node*n=new Node(v);n->sibling=p->child;n->parent=p;p->child=n;return n;} Node* insert_next(Node* p,const T&v){if(!p)throw std::invalid_argument("sibling");Node*n=new Node(v);n->sibling=p->sibling;n->parent=p->parent;p->sibling=n;return n;}
    [[nodiscard]] Node* parent_of(Node*n)const noexcept{return n?n->parent:nullptr;} void delete_subtree(Node*n){if(!n)return;if(n==root_){root_=root_->sibling;n->sibling=nullptr;}else{Node**link=&n->parent->child;while(*link&&*link!=n)link=&(*link)->sibling;if(*link==n)*link=n->sibling;n->sibling=nullptr;}destroy(n);} void clear()noexcept{destroy(root_);root_=nullptr;}
    template<class V> void preorder(V&&v)const{pre(root_,v);} template<class V> void postorder(V&&v)const{post(root_,v);} template<class V> void breadth_first(V&&v)const{std::vector<Node*> q;for(Node*n=root_;n;n=n->sibling)q.push_back(n);for(std::size_t i=0;i<q.size();++i){v(q[i]->value);for(Node*c=q[i]->child;c;c=c->sibling)q.push_back(c);}}
private: static void destroy(Node*n)noexcept{if(n){destroy(n->child);destroy(n->sibling);delete n;}} static Node* clone(const Node*n,Node*p){if(!n)return nullptr;Node*c=new Node(n->value);c->parent=p;try{c->child=clone(n->child,c);c->sibling=clone(n->sibling,p);}catch(...){destroy(c);throw;}return c;} template<class V>static void pre(Node*n,V&v){for(;n;n=n->sibling){v(n->value);pre(n->child,v);}} template<class V>static void post(Node*n,V&v){for(;n;n=n->sibling){post(n->child,v);v(n->value);}} Node*release()noexcept{Node*r=root_;root_=nullptr;return r;} Node*root_{nullptr};
};
// <<< general-tree
// >>> disjoint-set
class DisjointSet { public: explicit DisjointSet(std::size_t n):parent_(n),rank_(n,0){for(std::size_t i=0;i<n;++i)parent_[i]=i;} std::size_t find(std::size_t x){if(x>=parent_.size())throw std::out_of_range("index");return parent_[x]==x?x:parent_[x]=find(parent_[x]);} bool unite(std::size_t a,std::size_t b){a=find(a);b=find(b);if(a==b)return false;if(rank_[a]<rank_[b])std::swap(a,b);parent_[b]=a;if(rank_[a]==rank_[b])++rank_[a];return true;} [[nodiscard]]bool same(std::size_t a,std::size_t b){return find(a)==find(b);} private:std::vector<std::size_t>parent_,rank_;};
// <<< disjoint-set
}
```
