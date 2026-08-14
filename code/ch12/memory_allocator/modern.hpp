#pragma once
#include <cstddef>
#include <memory>
#include <optional>

namespace dsa::advanced {
enum class Fit { First, Best, Worst };
class BoundaryAllocator {
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    struct Block { std::size_t offset{}, size{}, prev{npos}, next{npos}; bool free{false}; };
    std::unique_ptr<Block[]> blocks_; std::size_t capacity_, count_{1}, head_{0};
public:
    explicit BoundaryAllocator(std::size_t bytes,std::size_t max_blocks=128):blocks_(std::make_unique<Block[]>(max_blocks)),capacity_(bytes){blocks_[0]={0,bytes,npos,npos,true};}
    [[nodiscard]] std::optional<std::size_t> allocate(std::size_t bytes,Fit fit){if(bytes==0)return std::nullopt;std::size_t chosen=npos,best=npos;for(std::size_t i=head_;i!=npos;i=blocks_[i].next){if(blocks_[i].free&&blocks_[i].size>=bytes){if(fit==Fit::First){chosen=i;break;}if(best==npos||(fit==Fit::Best&&blocks_[i].size<blocks_[best].size)||(fit==Fit::Worst&&blocks_[i].size>blocks_[best].size))best=i;}}if(fit!=Fit::First)chosen=best;if(chosen==npos)return std::nullopt;Block& b=blocks_[chosen];if(b.size>bytes&&count_<capacity_){const std::size_t tail=count_++;blocks_[tail]={b.offset+bytes,b.size-bytes,chosen,b.next,true};if(b.next!=npos)blocks_[b.next].prev=tail;b.next=tail;b.size=bytes;}b.free=false;return b.offset;}
    bool release(std::size_t offset){for(std::size_t i=head_;i!=npos;i=blocks_[i].next){if(blocks_[i].offset==offset&&!blocks_[i].free){blocks_[i].free=true;merge(i);return true;}}return false;}
    [[nodiscard]] std::size_t free_bytes()const noexcept{std::size_t n=0;for(std::size_t i=head_;i!=npos;i=blocks_[i].next)if(blocks_[i].free)n+=blocks_[i].size;return n;}
private:
    void merge(std::size_t i){Block& b=blocks_[i];if(b.next!=npos&&blocks_[b.next].free){const std::size_t n=b.next;b.size+=blocks_[n].size;b.next=blocks_[n].next;if(b.next!=npos)blocks_[b.next].prev=i;}if(b.prev!=npos&&blocks_[b.prev].free){const std::size_t p=b.prev;blocks_[p].size+=b.size;blocks_[p].next=b.next;if(b.next!=npos)blocks_[b.next].prev=p;}}
};
}
