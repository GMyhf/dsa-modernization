#pragma once
#include <cstddef>
#include <memory>
#include <stdexcept>

namespace dsa::advanced {

class SparseMatrix {
    struct Node { std::size_t row, col; int value; std::unique_ptr<Node> next_row; Node* next_col{nullptr}; Node(std::size_t r,std::size_t c,int v):row(r),col(c),value(v){} };
    std::size_t rows_, cols_; std::unique_ptr<Node>* row_heads_; Node** col_heads_;
public:
    SparseMatrix(std::size_t rows,std::size_t cols):rows_(rows),cols_(cols),row_heads_(new std::unique_ptr<Node>[rows]),col_heads_(new Node*[cols]{}){}
    ~SparseMatrix(){delete[] row_heads_;delete[] col_heads_;}
    SparseMatrix(const SparseMatrix&)=delete; SparseMatrix& operator=(const SparseMatrix&)=delete;
    [[nodiscard]] std::size_t rows()const noexcept{return rows_;} [[nodiscard]] std::size_t cols()const noexcept{return cols_;}
    void set(std::size_t r,std::size_t c,int v){if(r>=rows_||c>=cols_)throw std::out_of_range("SparseMatrix index");Node* prev=nullptr;Node* p=row_heads_[r].get();while(p&&p->col<c){prev=p;p=p->next_row.get();}if(p&&p->col==c){if(v==0){if(prev)prev->next_row=std::move(p->next_row);else row_heads_[r]=std::move(p->next_row);rebuild_columns();}else p->value=v;return;}if(v==0)return;auto n=std::make_unique<Node>(r,c,v);n->next_row=prev?std::move(prev->next_row):std::move(row_heads_[r]);if(prev)prev->next_row=std::move(n);else row_heads_[r]=std::move(n);rebuild_columns();}
    [[nodiscard]] int get(std::size_t r,std::size_t c)const{if(r>=rows_||c>=cols_)throw std::out_of_range("SparseMatrix index");for(Node*p=row_heads_[r].get();p&&p->col<=c;p=p->next_row.get())if(p->col==c)return p->value;return 0;}
    [[nodiscard]] std::size_t nonzeros()const noexcept{std::size_t n=0;for(std::size_t r=0;r<rows_;++r)for(Node*p=row_heads_[r].get();p;p=p->next_row.get())++n;return n;}
    template<class F> void for_each_column(std::size_t c,F f)const{if(c>=cols_)throw std::out_of_range("SparseMatrix column");for(Node*p=col_heads_[c];p;p=p->next_col)f(p->row,p->value);}
private:
    void rebuild_columns()noexcept{for(std::size_t c=0;c<cols_;++c)col_heads_[c]=nullptr;for(std::size_t r=0;r<rows_;++r){for(Node*p=row_heads_[r].get();p;p=p->next_row.get()){Node** q=&col_heads_[p->col];while(*q&&(*q)->row<r)q=&(*q)->next_col;p->next_col=*q;*q=p;}}}
};
}
