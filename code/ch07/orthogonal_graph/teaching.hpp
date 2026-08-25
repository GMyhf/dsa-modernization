#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace dsa {

// >>> orthogonal-graph-teaching
// 原书式教学实现：弧结点只有一份，同时接进出边链和入边链。
class OrthogonalGraphTeaching {
public:
    // >>> orthogonal-arcbox
    struct ArcBox {
        std::size_t tailvex;       // 弧尾 u
        std::size_t headvex;       // 弧头 v
        ArcBox* tailnextarc;       // 下一条同尾弧
        ArcBox* headnextarc;       // 下一条同头弧
        int info;                  // 权值或其他边属性
    };

    struct VexNode {
        ArcBox* firstin = nullptr;   // 第一条指向本顶点的弧
        ArcBox* firstout = nullptr;  // 第一条从本顶点出发的弧
    };
    // <<< orthogonal-arcbox

    explicit OrthogonalGraphTeaching(std::size_t count) : vertices_(count) {}
    OrthogonalGraphTeaching(const OrthogonalGraphTeaching&) = delete;
    OrthogonalGraphTeaching& operator=(const OrthogonalGraphTeaching&) = delete;
    ~OrthogonalGraphTeaching() { clear(); }

    // >>> orthogonal-teaching-add
    void add_edge(std::size_t tail, std::size_t head, int info = 1) {
        check_vertex(tail);
        check_vertex(head);
        auto* arc = new ArcBox{tail, head, vertices_[tail].firstout,
                               vertices_[head].firstin, info};
        vertices_[tail].firstout = arc;  // 接进 tail 的出边链
        vertices_[head].firstin = arc;   // 同一结点接进 head 的入边链
        ++arc_count_;
    }
    // <<< orthogonal-teaching-add

    [[nodiscard]] std::vector<std::size_t> out_neighbors(std::size_t vertex) const {
        check_vertex(vertex);
        std::vector<std::size_t> result;
        for (ArcBox* arc = vertices_[vertex].firstout; arc; arc = arc->tailnextarc) {
            result.push_back(arc->headvex);
        }
        return result;
    }

    [[nodiscard]] std::vector<std::size_t> in_neighbors(std::size_t vertex) const {
        check_vertex(vertex);
        std::vector<std::size_t> result;
        for (ArcBox* arc = vertices_[vertex].firstin; arc; arc = arc->headnextarc) {
            result.push_back(arc->tailvex);
        }
        return result;
    }

private:
    void clear() noexcept {
        for (VexNode& vertex : vertices_) {
            ArcBox* arc = vertex.firstout;
            while (arc) {
                ArcBox* next = arc->tailnextarc;
                delete arc;
                arc = next;
            }
            vertex.firstout = nullptr;
            vertex.firstin = nullptr;
        }
    }

    void check_vertex(std::size_t vertex) const {
        if (vertex >= vertices_.size()) {
            throw std::out_of_range("vertex");
        }
    }

    std::vector<VexNode> vertices_;
    std::size_t arc_count_ = 0;
};
// <<< orthogonal-graph-teaching

}  // namespace dsa
