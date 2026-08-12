#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>
namespace dsa {
// >>> graph
class Graph { public: static constexpr int infinity=std::numeric_limits<int>::max()/4; struct Edge{std::size_t from,to;int weight;bool operator<(const Edge&o)const noexcept{return weight<o.weight;}}; explicit Graph(std::size_t n):a_(n,std::vector<int>(n,infinity)){for(std::size_t i=0;i<n;++i)a_[i][i]=0;} [[nodiscard]]std::size_t vertices()const noexcept{return a_.size();} void add_edge(std::size_t u,std::size_t v,int w,bool directed=true){check(u);check(v);if(w<0)throw std::invalid_argument("negative edge");a_[u][v]=w;if(!directed)a_[v][u]=w;} [[nodiscard]]std::vector<std::size_t> dfs(std::size_t s)const{check(s);std::vector<bool>seen(vertices());std::vector<std::size_t>r;auto go=[&](auto&&self,std::size_t u)->void{seen[u]=true;r.push_back(u);for(std::size_t v=0;v<vertices();++v)if(a_[u][v]<infinity&&!seen[v])self(self,v);};go(go,s);return r;} [[nodiscard]]std::vector<std::size_t>bfs(std::size_t s)const{check(s);std::vector<bool>seen(vertices());std::queue<std::size_t>q;std::vector<std::size_t>r;q.push(s);seen[s]=true;while(!q.empty()){auto u=q.front();q.pop();r.push_back(u);for(std::size_t v=0;v<vertices();++v)if(a_[u][v]<infinity&&!seen[v])seen[v]=true,q.push(v);}return r;}
[[nodiscard]]std::optional<std::vector<std::size_t>> topological_sort()const{std::vector<std::size_t>in(vertices());for(std::size_t u=0;u<vertices();++u)for(std::size_t v=0;v<vertices();++v)if(u!=v&&a_[u][v]<infinity)++in[v];std::queue<std::size_t>q;for(std::size_t i=0;i<vertices();++i)if(!in[i])q.push(i);std::vector<std::size_t>r;while(!q.empty()){auto u=q.front();q.pop();r.push_back(u);for(std::size_t v=0;v<vertices();++v)if(u!=v&&a_[u][v]<infinity&&!--in[v])q.push(v);}return r.size()==vertices()?std::optional<std::vector<std::size_t>>(r):std::nullopt;}
[[nodiscard]]std::vector<int>dijkstra(std::size_t s)const{check(s);std::vector<int>d(vertices(),infinity);std::vector<bool>used(vertices());d[s]=0;for(std::size_t k=0;k<vertices();++k){std::size_t u=vertices();for(std::size_t i=0;i<vertices();++i)if(!used[i]&&(u==vertices()||d[i]<d[u]))u=i;if(u==vertices()||d[u]==infinity)break;used[u]=true;for(std::size_t v=0;v<vertices();++v)if(a_[u][v]<infinity&&d[v]>d[u]+a_[u][v])d[v]=d[u]+a_[u][v];}return d;}
[[nodiscard]]std::vector<std::vector<int>>floyd()const{auto d=a_;for(std::size_t k=0;k<vertices();++k)for(std::size_t i=0;i<vertices();++i)for(std::size_t j=0;j<vertices();++j)if(d[i][k]<infinity&&d[k][j]<infinity)d[i][j]=std::min(d[i][j],d[i][k]+d[k][j]);return d;}
[[nodiscard]]std::optional<std::vector<Edge>>prim(std::size_t s)const{check(s);std::vector<int>d(vertices(),infinity);std::vector<std::size_t>pre(vertices());std::vector<bool>used(vertices());std::vector<Edge>r;d[s]=0;for(std::size_t k=0;k<vertices();++k){std::size_t u=vertices();for(std::size_t i=0;i<vertices();++i)if(!used[i]&&(u==vertices()||d[i]<d[u]))u=i;if(u==vertices()||d[u]==infinity)return std::nullopt;used[u]=true;if(u!=s)r.push_back({pre[u],u,d[u]});for(std::size_t v=0;v<vertices();++v)if(!used[v]&&a_[u][v]<d[v])d[v]=a_[u][v],pre[v]=u;}return r;}
[[nodiscard]]std::optional<std::vector<Edge>>kruskal()const{std::vector<Edge>e;for(std::size_t i=0;i<vertices();++i)for(std::size_t j=i+1;j<vertices();++j)if(a_[i][j]<infinity)e.push_back({i,j,a_[i][j]});std::sort(e.begin(),e.end());std::vector<std::size_t>p(vertices());for(std::size_t i=0;i<vertices();++i)p[i]=i;auto f=[&](auto&&self,std::size_t x)->std::size_t{return p[x]==x?x:p[x]=self(self,p[x]);};std::vector<Edge>r;for(auto x:e)if(f(f,x.from)!=f(f,x.to)){p[f(f,x.from)]=f(f,x.to);r.push_back(x);}return r.size()+1==vertices()?std::optional<std::vector<Edge>>(r):std::nullopt;}
private:void check(std::size_t i)const{if(i>=vertices())throw std::out_of_range("vertex");}std::vector<std::vector<int>>a_;};
// <<< graph
}
