#pragma once
#include <cstddef>

namespace dsa::advanced {
class MarkSweepHeap {
    struct Node { int value; bool marked{false}; Node* edge{nullptr}; Node* next{nullptr}; explicit Node(int v):value(v){} };
    Node* all_{nullptr};
public:
    ~MarkSweepHeap(){sweep();while(all_){Node*n=all_->next;delete all_;all_=n;}}
    Node* make(int v){Node*n=new Node(v);n->next=all_;all_=n;return n;}
    static void link(Node* from,Node* to)noexcept{from->edge=to;}
    void collect(Node* root)noexcept{mark(root);sweep();}
    [[nodiscard]] std::size_t live()const noexcept{std::size_t n=0;for(Node*p=all_;p;p=p->next)++n;return n;}
    [[nodiscard]] int value(const Node*n)const noexcept{return n->value;}
private:
    static void mark(Node*n)noexcept{if(!n||n->marked)return;n->marked=true;mark(n->edge);}
    void sweep()noexcept{Node** p=&all_;while(*p){if(!(*p)->marked){Node*dead=*p;*p=dead->next;delete dead;}else{(*p)->marked=false;p=&(*p)->next;}}}
};
}
