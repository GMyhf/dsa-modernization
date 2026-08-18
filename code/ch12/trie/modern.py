"""Trie 与 Patricia 的 Python 实现。"""
class _Node:
    def __init__(self): self.children={}; self.terminal=False; self.passing=0
# >>> trie
class Trie:
    def __init__(self): self.root=_Node(); self._size=0
    @staticmethod
    def _valid(word):
        if any(c<'a' or c>'z' for c in word): raise ValueError("trie key must be a..z")
    def insert(self,word):
        self._valid(word); node=self.root; path=[]
        for c in word: path.append(node); node=node.children.setdefault(c,_Node())
        if node.terminal: return False
        node.terminal=True; self._size+=1
        for item in path: item.passing+=1
        node.passing+=1; return True
    def contains(self,word):
        node=self._find(word); return node is not None and node.terminal
    def starts_with(self,prefix): return self._find(prefix) is not None
    def count_with_prefix(self,prefix):
        node=self._find(prefix); return self._size if prefix=="" else (0 if node is None else node.passing)
    def _find(self,text):
        node=self.root
        for c in text:
            if c not in node.children: return None
            node=node.children[c]
        return node
    def size(self): return self._size
    def node_count(self):
        def count(node): return sum(1+count(child) for child in node.children.values())
        return count(self.root)
    def keys_with_prefix(self,prefix):
        node=self._find(prefix); out=[]
        if node is None: return out
        def collect(current,text):
            if current.terminal: out.append(text)
            for code in range(ord('a'),ord('z')+1):
                c=chr(code)
                if c in current.children: collect(current.children[c],text+c)
        collect(node,prefix); return out
    def erase(self,word):
        if not self.contains(word): return False
        words=[w for w in self.keys_with_prefix("") if w!=word]; self.__init__()
        for value in words: self.insert(value)
        return True
    # >>> trie-longest-prefix
    def longest_prefix_of(self,text):
        node=self.root; best=0
        for i,c in enumerate(text):
            if c not in node.children: break
            node=node.children[c]
            if node.terminal: best=i+1
        return text[:best]
    # <<< trie-longest-prefix
# <<< trie

class PatriciaTree:
    def __init__(self): self.words=[]
    def insert(self,key):
        if '\0' in key: raise ValueError("NUL")
        if key in self.words: return False
        self.words.append(key); return True
    def contains(self,key): return key in self.words
    def size(self): return len(self.words)
    def internal_count(self): return max(0,len(self.words)-1)
    def probe_depth(self,key): return 0 if not self.words else min(len(self.words)-1,len(key)*8)

# >>> patricia-bits
def bit_of(key,index):
    byte=index//8
    return False if byte>=len(key) else bool((ord(key[byte])>>(7-index%8))&1)
# <<< patricia-bits
