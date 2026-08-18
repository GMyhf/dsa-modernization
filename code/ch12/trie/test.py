import modern
t=modern.Trie()
for w in ["can","car","cat","do"]: assert t.insert(w)
assert t.node_count()==7 and t.count_with_prefix("ca")==3
assert t.keys_with_prefix("ca")==["can","car","cat"]
assert t.longest_prefix_of("cartoon")=="car" and t.erase("car") and not t.contains("car")
p=modern.PatriciaTree()
for w in ["a","ab","abc"]: assert p.insert(w)
assert p.internal_count()==2 and all(p.contains(w) for w in ["a","ab","abc"])
print("12 项断言")
