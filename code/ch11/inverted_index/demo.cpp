#include "modern.hpp"

#include <cstdio>

int main() {
    dsa::index::InvertedIndex index;
    index.add_document(310, {"计算机系", "英语专长"});
    index.add_document(330, {"计算机系"});
    index.add_document(341, {"计算机系"});
    index.add_document(421, {"英语专长"});

    const auto show = [](const char* label, const std::vector<int>& docs) {
        std::printf("%s:", label);
        for (const int doc : docs) {
            std::printf(" %04d", doc);
        }
        std::printf("\n");
    };
    show("计算机系          ", index.postings("计算机系"));
    show("英语专长          ", index.postings("英语专长"));
    show("计算机系且擅长英语", index.and_query({"计算机系", "英语专长"}));
    show("计算机系或擅长英语", index.or_query({"计算机系", "英语专长"}));
    show("不擅长英语        ", index.not_query("英语专长"));

    dsa::index::InvertedIndex text;
    text.add_document(1, {"the", "quick", "brown", "fox"});
    text.add_document(2, {"the", "brown", "quick", "fox"});
    show("含 quick 与 brown ", text.and_query({"quick", "brown"}));
    show("短语 quick brown  ", text.phrase_query({"quick", "brown"}));
    return 0;
}
