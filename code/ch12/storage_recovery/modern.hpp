#pragma once

#include <cstddef>
#include <vector>

namespace dsa::advanced {

// >>> mark-sweep
/// 标记–清除（mark and sweep）：原书 12.2.4 讲的那种无用单元回收。
///
/// 分两趟：
///
/// 1. **标记**——从**一组根**（栈上的指针、全局变量）出发，走遍所有还能碰到的对象并打标记。
/// 2. **清除**——扫一遍堆，没打上标记的统统回收，同时把标记清掉，为下一轮做准备。
///
/// 它和引用计数的关键差别就在第 1 趟：可达性是**从根走出来的**，与「有多少人指着我」无关。
/// 所以两个对象互相指着、却没人从根能走到它们时，引用计数收不回，标记–清除收得回。
/// 12.2.1 的广义表用的是引用计数，正因如此那里造不出环也不敢造。
///
/// 这是教学模型，不是真实的运行时垃圾回收器：对象图由调用方显式搭出来，
/// 没有栈扫描、没有写屏障、没有分代，也不区分「这是指针」和「这只是个看起来像地址的整数」
/// ——而那恰恰是真实 GC 最难的部分。
class MarkSweepHeap {
public:
    struct Node {
        int value;
        bool marked = false;
        std::vector<Node*> edges;  // 出边：一个对象可以指向多个对象
        Node* next = nullptr;      // 堆内所有对象串成一条链，供清除阶段扫描
    };

    MarkSweepHeap() = default;
    MarkSweepHeap(const MarkSweepHeap&) = delete;
    MarkSweepHeap& operator=(const MarkSweepHeap&) = delete;
    ~MarkSweepHeap() { destroy_all(); }

    /// 在堆上造一个对象。返回的是观察指针——所有权始终在堆这边。
    Node* allocate(int value) {
        Node* node = new Node{value, false, {}, all_};
        all_ = node;
        ++live_;
        return node;
    }

    /// 让 from 指向 to。一个对象可以有多条出边，这才叫对象图。
    static void link(Node* from, Node* to) { from->edges.push_back(to); }

    /// 跑一轮回收。返回这一轮回收掉的对象数。
    std::size_t collect(const std::vector<Node*>& roots) {
        mark(roots);
        return sweep();
    }

    [[nodiscard]] std::size_t live() const noexcept { return live_; }

private:
    /// 标记阶段用**显式栈**，不用递归——对象图可以很深，
    /// 而 GC 恰恰是在内存吃紧时跑的，那时最不该再去吃调用栈（D-001 §2b 同一个判据）。
    void mark(const std::vector<Node*>& roots) {
        std::vector<Node*> pending;
        for (Node* root : roots) {
            if (root != nullptr && !root->marked) {
                root->marked = true;
                pending.push_back(root);
            }
        }
        while (!pending.empty()) {
            Node* node = pending.back();
            pending.pop_back();
            for (Node* next : node->edges) {
                if (next != nullptr && !next->marked) {
                    next->marked = true;   // 先标记再入栈：环不会让这里转不出来
                    pending.push_back(next);
                }
            }
        }
    }

    /// 清除阶段顺着 all_ 链走一遍：没标记的删掉，有标记的把标记清零。
    std::size_t sweep() noexcept {
        std::size_t reclaimed = 0;
        Node** slot = &all_;
        while (*slot != nullptr) {
            Node* node = *slot;
            if (node->marked) {
                node->marked = false;  // 为下一轮复位
                slot = &node->next;
            } else {
                *slot = node->next;
                delete node;
                ++reclaimed;
                --live_;
            }
        }
        return reclaimed;
    }

    void destroy_all() noexcept {
        while (all_ != nullptr) {
            Node* next = all_->next;
            delete all_;
            all_ = next;
        }
        live_ = 0;
    }

    Node* all_ = nullptr;   // 堆内全部对象，清除阶段沿它扫描
    std::size_t live_ = 0;
};
// <<< mark-sweep

}  // namespace dsa::advanced
