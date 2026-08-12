#pragma once
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>
namespace dsa { namespace advanced {
// >>> optimal-bst
struct OptimalBstResult{std::vector<std::vector<long long>>cost;std::vector<std::vector<std::size_t>>root;}; inline OptimalBstResult optimal_bst(const std::vector<int>&p,const std::vector<int>&q){if(q.size()!=p.size()+1)throw std::invalid_argument("weights");const std::size_t n=p.size();OptimalBstResult r{std::vector<std::vector<long long>>(n+1,std::vector<long long>(n+1)),std::vector<std::vector<std::size_t>>(n+1,std::vector<std::size_t>(n+1))};for(std::size_t i=0;i<=n;++i)r.cost[i][i]=q[i];for(std::size_t len=1;len<=n;++len)for(std::size_t i=0;i+len<=n;++i){auto j=i+len;long long w=0;for(std::size_t k=i;k<=j;++k){w+=q[k];if(k>i)w+=p[k-1];}r.cost[i][j]=std::numeric_limits<long long>::max()/4;for(std::size_t k=i+1;k<=j;++k)if(auto c=r.cost[i][k-1]+r.cost[k][j]+w;c<r.cost[i][j])r.cost[i][j]=c,r.root[i][j]=k;}return r;}
// <<< optimal-bst
}}
