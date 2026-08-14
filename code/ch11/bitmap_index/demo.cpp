#include "modern.hpp"

#include <cstdio>

int main() {
    dsa::index::BitmapIndex index;
    for (int i = 0; i < 200; ++i) {
        index.add_record((i % 3) == 0 ? "及格" : "不及格");
    }
    index.reset_ops();
    const auto passed = index.select("及格");
    const auto failed = index.select_not("及格");
    std::printf("200 条记录，%zu 个取值，位图共 %zu 个机器字\n",
                index.distinct_values(), index.words());
    std::printf("及格 %zu 条，不及格 %zu 条；取反只做了 %zu 次字运算\n",
                passed.size(), failed.size(), index.word_ops());

    // 稀疏位图：大片全 0 的字，游程压缩很有效。
    dsa::index::BitmapIndex sparse;
    for (int i = 0; i < 1000; ++i) {
        sparse.add_record(i < 3 ? "命中" : "其他");
    }
    const auto bits = sparse.bitmap("命中");
    const auto encoded = dsa::index::run_length_encode(bits);
    std::printf("稀疏位图 %zu 个字 → 游程压缩后 %zu 个字\n", bits.size(), encoded.size());

    dsa::index::SignatureFile signatures(2);
    signatures.add(1, {"数据", "结构"});
    signatures.add(2, {"算法", "分析"});
    std::printf("签名粗筛「数据」的候选文档数：%zu（仍需回原文确认）\n",
                signatures.candidates({"数据"}).size());
    return 0;
}
