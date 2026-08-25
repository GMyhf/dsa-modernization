#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

namespace dsa {

// >>> orthogonal-graph
/// 十字链表：每条弧由 arcs_ 独占；出链和入链只保存非拥有链接。
class OrthogonalGraph {
public:
    explicit OrthogonalGraph(std::size_t count) : vertices_(count) {}

    [[nodiscard]] std::size_t vertices() const noexcept { return vertices_.size(); }
    [[nodiscard]] std::size_t edges() const noexcept { return arcs_.size(); }

    // >>> orthogonal-modern-add
    void add_edge(std::size_t tail, std::size_t head, int info = 1) {
        check_vertex(tail);
        check_vertex(head);
        if (find_arc(tail, head)) {
            throw std::invalid_argument("duplicate edge");
        }

        auto owned = std::make_unique<Arc>(tail, head, info);
        Arc* arc = owned.get();
        arc->tailnextarc = vertices_[tail].firstout;
        vertices_[tail].firstout = arc;
        arc->headnextarc = vertices_[head].firstin;
        vertices_[head].firstin = arc;
        arcs_.push_back(std::move(owned));
    }
    // <<< orthogonal-modern-add

    [[nodiscard]] std::vector<std::size_t> out_neighbors(std::size_t vertex) const {
        check_vertex(vertex);
        std::vector<std::size_t> result;
        for (const Arc* arc = vertices_[vertex].firstout; arc; arc = arc->tailnextarc) {
            result.push_back(arc->head);
        }
        return result;
    }

    [[nodiscard]] std::vector<std::size_t> in_neighbors(std::size_t vertex) const {
        check_vertex(vertex);
        std::vector<std::size_t> result;
        for (const Arc* arc = vertices_[vertex].firstin; arc; arc = arc->headnextarc) {
            result.push_back(arc->tail);
        }
        return result;
    }

    // >>> orthogonal-modern-remove
    bool remove_edge(std::size_t tail, std::size_t head) {
        check_vertex(tail);
        check_vertex(head);
        Arc** out_link = &vertices_[tail].firstout;
        while (*out_link && (*out_link)->head != head) {
            out_link = &(*out_link)->tailnextarc;
        }
        if (!*out_link) {
            return false;
        }

        Arc* victim = *out_link;
        *out_link = victim->tailnextarc;
        Arc** in_link = &vertices_[head].firstin;
        while (*in_link != victim) {
            in_link = &(*in_link)->headnextarc;
        }
        *in_link = victim->headnextarc;
        arcs_.erase(std::remove_if(arcs_.begin(), arcs_.end(),
                                   [victim](const std::unique_ptr<Arc>& owned) {
                                       return owned.get() == victim;
                                   }),
                    arcs_.end());
        return true;
    }
    // <<< orthogonal-modern-remove

private:
    struct Arc {
        Arc(std::size_t tail_value, std::size_t head_value, int info_value)
            : tail(tail_value), head(head_value), info(info_value) {}

        std::size_t tail;
        std::size_t head;
        Arc* tailnextarc = nullptr;
        Arc* headnextarc = nullptr;
        int info;
    };

    struct Vertex {
        Arc* firstin = nullptr;
        Arc* firstout = nullptr;
    };

    [[nodiscard]] Arc* find_arc(std::size_t tail, std::size_t head) const {
        for (Arc* arc = vertices_[tail].firstout; arc; arc = arc->tailnextarc) {
            if (arc->head == head) {
                return arc;
            }
        }
        return nullptr;
    }

    void check_vertex(std::size_t vertex) const {
        if (vertex >= vertices()) {
            throw std::out_of_range("vertex");
        }
    }

    std::vector<Vertex> vertices_;
    std::vector<std::unique_ptr<Arc>> arcs_;
};
// <<< orthogonal-graph

}  // namespace dsa
