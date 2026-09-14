#include "modern.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("  FAIL: %s\n", name);
    }
}

dsa::GeneralTree<char> make_tree() {
    dsa::GeneralTree<char> tree;
    tree.create_root('A');
    auto* b = tree.insert_first(tree.root(), 'B');
    auto* c = tree.insert_next(b, 'C');
    tree.insert_first(b, 'D');
    tree.insert_first(c, 'E');
    return tree;
}

void test_tree() {
    auto tree = make_tree();
    std::vector<char> preorder;
    std::vector<char> postorder;
    std::vector<char> breadth;
    tree.preorder([&](char value) { preorder.push_back(value); });
    tree.postorder([&](char value) { postorder.push_back(value); });
    tree.breadth_first([&](char value) { breadth.push_back(value); });

    check(preorder == std::vector<char>({'A', 'B', 'D', 'C', 'E'}),
          "算法6.3 preorder");
    check(postorder == std::vector<char>({'D', 'B', 'E', 'C', 'A'}),
          "算法6.4 postorder");
    check(breadth == std::vector<char>({'A', 'B', 'C', 'D', 'E'}),
          "算法6.5 breadth first");

    auto* b = tree.root()->child;
    auto* c = b->sibling;
    check(tree.parent_of(b) == tree.root(), "代码6.7 first child parent");
    check(tree.parent_of(c) == tree.root(), "代码6.7 sibling parent");
    check(tree.parent_of(tree.root()) == nullptr, "代码6.2 root parent");
    check(tree.parent_of(nullptr) == nullptr, "代码6.2 null parent");

    auto* new_first = tree.insert_first(tree.root(), 'F');
    check(tree.root()->child == new_first, "代码6.6 insert first prepends");
    check(new_first->sibling == b, "代码6.6 old first follows new");
    check(new_first->parent == tree.root(), "代码6.6 inserted parent");
    tree.delete_subtree(new_first);
    check(tree.root()->child == b, "代码6.7 delete prepended child");

    tree.delete_subtree(b);
    check(tree.root()->child == c, "代码6.7 delete first child reconnects");
    check(c->parent == tree.root(), "代码6.7 surviving child parent");
    tree.delete_subtree(c);
    check(tree.root()->child == nullptr, "代码6.7 delete last child");

    tree.create_root('X');
    auto* second_root = tree.insert_next(tree.root(), 'Y');
    tree.delete_subtree(second_root);
    check(tree.root()->value == 'X', "代码6.7 delete forest sibling root");
    check(tree.root()->sibling == nullptr, "代码6.7 forest tail unlinked");
    auto* restored_second = tree.insert_next(tree.root(), 'Y');
    tree.delete_subtree(tree.root());
    check(tree.root() == restored_second, "代码6.7 delete first forest root");
    check(tree.root()->parent == nullptr, "代码6.7 successor remains forest root");

    auto copy = tree;
    copy.root()->value = 'Z';
    check(tree.root()->value == 'Y', "代码6.1 deep copy source unchanged");
    check(copy.root()->value == 'Z', "代码6.1 copied node independent");
    dsa::GeneralTree<char> assigned;
    assigned = tree;
    auto& alias = assigned;
    assigned = alias;
    check(assigned.root()->value == 'Y', "代码6.2 self assignment");
    auto moved = std::move(copy);
    check(copy.root() == nullptr, "代码6.2 move clears source");
    check(moved.root()->value == 'Z', "代码6.2 move owns tree");

    bool bad_parent = false;
    try {
        tree.insert_first(nullptr, '!');
    } catch (const std::invalid_argument&) {
        bad_parent = true;
    }
    check(bad_parent, "代码6.6 rejects null parent");
    bool bad_sibling = false;
    try {
        tree.insert_next(nullptr, '!');
    } catch (const std::invalid_argument&) {
        bad_sibling = true;
    }
    check(bad_sibling, "代码6.6 rejects null sibling");
    tree.delete_subtree(nullptr);
    check(tree.root() != nullptr, "代码6.7 null deletion is no-op");
    tree.clear();
    check(tree.root() == nullptr, "代码6.7 clear tree");
}

void test_disjoint_set() {
    dsa::DisjointSet sets(8);
    for (std::size_t index = 0; index < 8; ++index) {
        check(sets.find(index) == index, "代码6.8 singleton find");
    }
    check(sets.unite(0, 1), "代码6.8 unite pair one");
    check(sets.unite(2, 3), "代码6.8 unite pair two");
    check(sets.unite(4, 5), "代码6.8 unite pair three");
    check(sets.unite(0, 2), "代码6.8 weighted union");
    check(sets.unite(0, 4), "代码6.8 merge classes");
    check(sets.same(1, 3), "代码6.8 equivalence class");
    check(sets.same(5, 0), "算法6.9 path compression find");
    check(!sets.same(1, 6), "代码6.8 distinct sets");
    const auto root = sets.find(5);
    check(root == sets.find(0), "算法6.9 compressed root");
    check(root == sets.find(1), "算法6.9 sibling compressed root");
    check(!sets.unite(1, 5), "代码6.8 duplicate union false");

    bool bad_find = false;
    try {
        (void)sets.find(8);
    } catch (const std::out_of_range&) {
        bad_find = true;
    }
    check(bad_find, "代码6.8 find bounds check");
    bool bad_unite = false;
    try {
        (void)sets.unite(0, 9);
    } catch (const std::out_of_range&) {
        bad_unite = true;
    }
    check(bad_unite, "代码6.8 unite bounds check");
}


// 【算法6.10】带双标记位的先根次序表示 → 「左子/右兄」树。
//
// 用原书图6.5(a) 那片森林的双标记序列（图6.15，原书第 154 页）：
//   先根次序 A B C E F D G H J I
//   ltag(有孩子) 0 1 0 1 1 1 0 0 1 1  →  has_child = (ltag == 0)
//   rtag(有兄弟) 0 0 0 0 1 1 1 0 1 1  →  has_sibling = (rtag == 0)
//
// **这两行以前是反的**（2026-09-11 修）。图6.15 的三行从上到下印的是 rtag / info / ltag，
// ltag 在最下面；照着「第一行就是 ltag」抄，两行就互换了。互换之后还原出来的是
// **另一片森林**——C 成了 B 的孩子而不是 B 的兄弟——可它的**先根序列恰好一字不差**，
// 所以当时那条「先根周游还原出原序列」的判据一点都没红。
//
// 于是判据加到两条：先根 + 后根。两种次序合起来才能唯一确定一棵树
// （与「前序 + 中序定二叉树」同一条道理，见书稿第 6.1.4 节的对应关系），
// 光对先根是查不出结构错的。
//   先根 A B C E F D G H J I
//   后根 B E F C D A J H I G
// 再加上逐点的父子 / 兄弟断言，把 B 是叶子、C 的父是 A 这两处钉死。
//
// 变异自检（2026-09-11 实测）：
//   · 把「扫到没有孩子的结点才弹栈」改成「每个结点都弹」→ 压栈出栈配不上，
//     from_dual_tag 抛「标志位不自洽」，两档构建都红；
//   · 把 ltag/rtag 两行换回互换的那一组 → 后根那条加结构断言共 10 条红，
//     而**先根那条照样绿**——这正是当初没能抓住它的原因。
void test_dual_tag_construction() {
    using Node = dsa::GeneralTree<char>::DualTagNode;
    const Node nodes[] = {
        {'A', true,  true },   // ltag=0 rtag=0：有孩子 B，有下一棵树 G
        {'B', false, true },   // ltag=1 rtag=0：叶结点，有兄弟 C
        {'C', true,  true },   // ltag=0 rtag=0：有孩子 E，有兄弟 D
        {'E', false, true },   // ltag=1 rtag=0：叶结点，有兄弟 F
        {'F', false, false},   // ltag=1 rtag=1
        {'D', false, false},   // ltag=1 rtag=1
        {'G', true,  false},   // ltag=0 rtag=1：有孩子 H，是最后一棵树
        {'H', true,  true },   // ltag=0 rtag=0：有孩子 J，有兄弟 I
        {'J', false, false},   // ltag=1 rtag=1
        {'I', false, false},   // ltag=1 rtag=1
    };
    auto tree = dsa::GeneralTree<char>::from_dual_tag(nodes, 10);

    std::string pre;
    tree.preorder([&pre](char c) { pre.push_back(c); });
    check(pre == "ABCEFDGHJI", "算法6.10 先根周游还原出原序列");

    // 光有先根分不出两片森林，后根才分得出：互换 ltag/rtag 得到的那片森林后根是
    // "FEDCBA JIHG" 之类，与下面这串对不上。
    std::string post;
    tree.postorder([&post](char c) { post.push_back(c); });
    check(post == "BEFCDAJHIG", "算法6.10 后根周游与原书图6.5(a) 一致");

    // 结构本身也要对：A 的长子是 B，B 是叶结点，C 是 B 的右兄弟、父是 A。
    //
    // 这几句一律走 null 安全的取值器。理由是实打实踩过的：变异自检时把 ltag/rtag 换回
    // 互换的那一组，`a->child->sibling` 就是空指针，原先直写 `c->child` 当场是 UB——
    // release-O2 下直接 SIGSEGV，连哪条断言红的都看不见。测试要在坏数据上**报错**，
    // 不是**崩溃**。
    auto kid = [](const auto* n) { return n != nullptr ? n->child : nullptr; };
    auto sib = [](const auto* n) { return n != nullptr ? n->sibling : nullptr; };
    auto par = [](const auto* n) { return n != nullptr ? n->parent : nullptr; };
    auto is = [](const auto* n, char expect) { return n != nullptr && n->value == expect; };

    const auto* a = tree.root();
    check(is(a, 'A'), "算法6.10 根是 A");
    check(is(kid(a), 'B'), "算法6.10 A 的长子是 B");
    check(kid(kid(a)) == nullptr, "算法6.10 B 是叶结点（ltag 为 1）");

    const auto* c = sib(kid(a));
    check(is(c, 'C'), "算法6.10 C 是 B 的右兄弟，不是 B 的孩子");
    check(is(kid(c), 'E'), "算法6.10 C 的长子是 E");
    check(is(sib(kid(c)), 'F'), "算法6.10 F 是 E 的右兄弟");
    // 父指针也要接对：兄弟共享父结点
    check(par(c) == a, "算法6.10 C 的父是 A");
    check(is(sib(c), 'D') && par(sib(c)) == a, "算法6.10 C 的右兄弟 D 与 C 同父");

    // A 的 rtag 为 0，所以它在森林里还有下一棵树
    const auto* g = sib(a);
    check(is(g, 'G'), "算法6.10 A 有右兄弟 G（这是一片森林，不是一棵树）");
    check(is(kid(g), 'H'), "算法6.10 G 的长子是 H");
    check(is(kid(kid(g)), 'J'), "算法6.10 H 的长子是 J");
    check(is(sib(kid(g)), 'I') && par(sib(kid(g))) == g, "算法6.10 I 是 H 的右兄弟、父是 G");
}

void test_dual_tag_edge_cases() {
    using Node = dsa::GeneralTree<char>::DualTagNode;

    auto empty_tree = dsa::GeneralTree<char>::from_dual_tag(nullptr, 0);
    check(empty_tree.root() == nullptr, "算法6.10 空序列得到空树");

    const Node single[] = {{'X', false, false}};
    auto one = dsa::GeneralTree<char>::from_dual_tag(single, 1);
    check(one.root() != nullptr && one.root()->value == 'X', "算法6.10 单结点序列");
    check(one.root()->child == nullptr && one.root()->sibling == nullptr,
          "算法6.10 单结点没有孩子也没有兄弟");

    bool null_rejected = false;
    try {
        (void)dsa::GeneralTree<char>::from_dual_tag(nullptr, 3);
    } catch (const std::invalid_argument&) {
        null_rejected = true;
    }
    check(null_rejected, "算法6.10 count 非零而数组为空指针时抛异常");

    // 标志位不自洽：两个结点都说「没有孩子、没有兄弟」，第二个结点无处安放。
    // 原书这里对空栈取顶，是未定义行为；这里必须抛异常。
    const Node inconsistent[] = {{'A', false, false}, {'B', false, false}};
    bool bad_rejected = false;
    try {
        (void)dsa::GeneralTree<char>::from_dual_tag(inconsistent, 2);
    } catch (const std::invalid_argument&) {
        bad_rejected = true;
    }
    check(bad_rejected, "算法6.10 标志位不自洽时抛异常，而不是对空栈取顶");

    // 最后一个结点若声称还有孩子，序列没有正常收尾
    const Node unfinished[] = {{'A', true, false}, {'B', true, false}};
    bool unfinished_rejected = false;
    try {
        (void)dsa::GeneralTree<char>::from_dual_tag(unfinished, 2);
    } catch (const std::invalid_argument&) {
        unfinished_rejected = true;
    }
    check(unfinished_rejected, "算法6.10 末结点仍声称有孩子时抛异常");

    // 末结点若声称还有**兄弟**，同样不自洽：它的右兄弟只能排在它后面，
    // 而它已经是最后一个了。2026-08-17 Codex 复查前，这两种序列都被静默接受。
    const Node dangling_single[] = {{'X', false, true}};
    bool single_rejected = false;
    try {
        (void)dsa::GeneralTree<char>::from_dual_tag(dangling_single, 1);
    } catch (const std::invalid_argument&) {
        single_rejected = true;
    }
    check(single_rejected, "算法6.10 单结点却声称有兄弟时抛异常");

    const Node dangling_last[] = {{'A', true, false}, {'B', false, true}};
    bool last_rejected = false;
    try {
        (void)dsa::GeneralTree<char>::from_dual_tag(dangling_last, 2);
    } catch (const std::invalid_argument&) {
        last_rejected = true;
    }
    check(last_rejected, "算法6.10 末结点声称有兄弟时抛异常");
}

// 代码6.8 的**重量权衡合并规则**：小树挂到大树下，比的是元素个数不是树高。
//
// 这一组用例专门分辨「按重量」和「按秩」——两者在这组等价对上长出不同的父指针数组。
// 变异：改回按秩合并，下面「4 挂到 0 下面」那条会红。
void test_weighted_union_rule() {
    dsa::DisjointSet sets(8);

    // 先把 {0,1,2} 并成一棵 3 个元素、高度 1 的树
    check(sets.unite(0, 1), "并 0-1");
    check(sets.unite(0, 2), "并 0-2");
    check(sets.set_size(0) == 3, "{0,1,2} 规模为 3");

    // 再把 {4,5} 并成一棵 2 个元素、高度 1 的树
    check(sets.unite(4, 5), "并 4-5");
    check(sets.set_size(4) == 2, "{4,5} 规模为 2");

    // 关键一步：两棵树**高度相同**（都是 1），但规模不同（3 vs 2）。
    //   按重量 → 小的挂到大的下面，根是 0；
    //   按秩   → 秩相同，取决于实现的先后手，原实现会让 4 当根。
    check(sets.unite(4, 0), "并 {4,5} 与 {0,1,2}");
    check(sets.find(4) == 0, "重量权衡：规模小的 {4,5} 挂到规模大的 {0,1,2} 下，根是 0");
    check(sets.set_size(0) == 5, "合并后规模累加为 5");
}

// 并列时的口径：规模相同，值大的根挂到值小的根下（课程第 6 章习题 8 的原话）。
void test_weighted_union_tie_break() {
    dsa::DisjointSet sets(4);
    check(sets.unite(2, 3), "并 2-3，规模同为 1");
    check(sets.find(3) == 2, "并列时值大的 3 挂到值小的 2 下");
    check(sets.parents()[3] == 2, "父指针数组里 3 的父是 2");

    dsa::DisjointSet reversed(4);
    check(reversed.unite(3, 2), "换个参数顺序再并一次");
    check(reversed.find(3) == 2, "结果与参数顺序无关，仍是 3 挂到 2 下");
}

// 连通分量与连通点对：每连一条边后，拿「边表 + BFS」暴力数一遍对拍。
// 变异判据：读非根元素的规模（学生常见错）、在冗余边上也减分量数——两者都要红。
struct BruteCount {
    std::size_t components;
    std::uint64_t pairs;
};
BruteCount brute_force(std::size_t count, const std::vector<std::pair<std::size_t, std::size_t>>& edges) {
    std::vector<std::vector<std::size_t>> adjacent(count);
    for (const auto& edge : edges) {
        adjacent[edge.first].push_back(edge.second);
        adjacent[edge.second].push_back(edge.first);
    }
    std::vector<bool> seen(count);
    BruteCount result{0, 0};
    for (std::size_t start = 0; start < count; ++start) {
        if (seen[start]) continue;
        ++result.components;
        std::vector<std::size_t> queue{start};
        seen[start] = true;
        for (std::size_t head = 0; head < queue.size(); ++head) {
            for (std::size_t next : adjacent[queue[head]]) {
                if (!seen[next]) { seen[next] = true; queue.push_back(next); }
            }
        }
        const std::uint64_t size = queue.size();
        result.pairs += size * (size - 1) / 2;
    }
    return result;
}

void test_component_counter() {
    dsa::ComponentCounter counter(4);
    check(counter.components() == 4 && counter.connected_pairs() == 0, "6.2.5 计数 初始 n 个分量、0 对");
    check(counter.connect(0, 1) == 1, "6.2.5 计数 1×1 新增 1 对");
    check(counter.connect(2, 3) == 1, "6.2.5 计数 另一组 1×1");
    check(counter.connect(1, 3) == 4, "6.2.5 计数 经非根元素合并 2×2 新增 4 对");
    check(counter.components() == 1 && counter.connected_pairs() == 6, "6.2.5 计数 全连通 C(4,2)=6");
    check(counter.connect(0, 3) == 0, "6.2.5 计数 冗余边新增 0 对");
    check(counter.connect(2, 2) == 0, "6.2.5 计数 自环新增 0 对");
    check(counter.components() == 1 && counter.connected_pairs() == 6, "6.2.5 计数 冗余边不改两个数");
    bool rejected = false;
    try { (void)counter.connect(0, 4); } catch (const std::out_of_range&) { rejected = true; }
    check(rejected && counter.components() == 1, "6.2.5 计数 越界抛异常且不动计数");

    std::mt19937 random(20260914);
    bool pair_delta = true, totals = true, redundant = true;
    std::size_t redundant_seen = 0;
    for (int round = 0; round < 200; ++round) {
        const std::size_t count = 1 + random() % 30;
        dsa::ComponentCounter sample(count);
        std::vector<std::pair<std::size_t, std::size_t>> edges;
        std::uint64_t before = 0;
        for (int step = 0; step < 45; ++step) {
            const std::size_t left = random() % count;
            const std::size_t right = random() % count;
            const std::size_t components_before = sample.components();
            const std::uint64_t added = sample.connect(left, right);
            edges.emplace_back(left, right);
            const auto expected = brute_force(count, edges);
            pair_delta = pair_delta && added == expected.pairs - before;
            totals = totals && sample.components() == expected.components &&
                     sample.connected_pairs() == expected.pairs;
            if (expected.pairs == before) {
                ++redundant_seen;
                redundant = redundant && added == 0 && sample.components() == components_before;
            }
            before = expected.pairs;
        }
    }
    check(pair_delta, "6.2.5 计数 随机 connect 返回值等于新增连通点对（BFS 对拍）");
    check(totals, "6.2.5 计数 随机 分量数与点对总数等于 BFS 暴力结果");
    check(redundant && redundant_seen > 100, "6.2.5 计数 随机 冗余边返回 0 且分量数不变");
}

}  // namespace

int main() {
    test_tree();
    test_disjoint_set();
    test_dual_tag_construction();
    test_dual_tag_edge_cases();
    test_weighted_union_rule();
    test_weighted_union_tie_break();
    test_component_counter();
    std::printf("GeneralTree: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
