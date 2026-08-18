"""倒排索引：有序归并、布尔查询与位置短语查询。"""

# >>> inverted-intersect
def intersect(left, right):
    out=[]; i=j=0
    while i<len(left) and j<len(right):
        if left[i]<right[j]: i+=1
        elif right[j]<left[i]: j+=1
        else: out.append(left[i]); i+=1; j+=1
    return out

def unite(left, right):
    out=[]; i=j=0
    while i<len(left) or j<len(right):
        if j==len(right) or (i<len(left) and left[i]<right[j]): value=left[i]; i+=1
        elif i==len(left) or right[j]<left[i]: value=right[j]; j+=1
        else: value=left[i]; i+=1; j+=1
        if not out or out[-1]!=value: out.append(value)
    return out

def difference(left, right):
    out=[]; j=0
    for value in left:
        while j<len(right) and right[j]<value: j+=1
        if j==len(right) or right[j]!=value: out.append(value)
    return out
# <<< inverted-intersect

class InvertedIndex:
    def __init__(self): self._terms={}; self._documents=[]
    def add_document(self, doc_id, terms):
        if self._documents and doc_id<=self._documents[-1]: raise ValueError("document ids must strictly increase")
        self._documents.append(doc_id)
        for position,term in enumerate(terms):
            posting=self._terms.setdefault(term, [[],[]])
            if not posting[0] or posting[0][-1]!=doc_id: posting[0].append(doc_id); posting[1].append([])
            posting[1][-1].append(position)
    def postings(self, term): return list(self._terms.get(term,[[],[]])[0])
    def and_query(self, terms):
        result=self.postings(terms[0]) if terms else []
        for term in terms[1:]: result=intersect(result,self.postings(term))
        return result
    def or_query(self, terms):
        result=[]
        for term in terms: result=unite(result,self.postings(term))
        return result
    def not_query(self, term): return difference(self._documents,self.postings(term))
    def phrase_query(self, terms):
        result=[]
        for doc in self.and_query(terms):
            starts=self._positions(terms[0],doc)
            if any(all(start+step in self._positions(term,doc) for step,term in enumerate(terms[1:],1)) for start in starts): result.append(doc)
        return result
    def _positions(self,term,doc):
        posting=self._terms.get(term,[[],[]])
        for i,value in enumerate(posting[0]):
            if value==doc: return posting[1][i]
        return []
    def document_count(self): return len(self._documents)
    def term_count(self): return len(self._terms)
    def postings_size(self): return sum(len(value[0]) for value in self._terms.values())
