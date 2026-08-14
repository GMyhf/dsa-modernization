#include <iostream>
#define VISITED 1;
#define UNVISITED 0;
using namespace std;

class Dist
{
public:
    int index;
    int lengh;
    int pre;
};
class Graph
{
public:
    int numVertex; // 顶点个数
    int numEdge;
    int *Mark;   // visit?
    int *Ingree; // 入度
    int VerticesNum()
    {
        return numVertex;
    }
    int EdgeNum();
} void Dijkstra(Graph &G, int s, Dist *&D)
{
    D = new Dist[G.VerticesNum()];
    for (int i = 0; i < G.VerticesNum(); ++i)
    {
        G.Mark[i] = UNVISITED;
        D[i].index = i;
        D[i].length = INFINITE;
        D[i].pre = s;
    }
    D[s].length=0;
    MinHeap<Dist>H(G.EdgeNum());
    H.Insert(D[s]);
}
int main()
{
    return 0;
}