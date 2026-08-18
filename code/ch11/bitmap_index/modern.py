"""位图索引、字级游程编码与签名粗筛。"""
MASK=(1<<64)-1
class BitmapIndex:
    def __init__(self): self._maps={}; self._count=0; self._ops=0
    def add_record(self,value):
        word=self._count//64; bit=self._count%64
        for bits in self._maps.values():
            while len(bits)<=word: bits.append(0)
        bits=self._maps.setdefault(value,[0]*(word+1)); bits[word]|=1<<bit; self._count+=1
    def bitmap(self,value): return list(self._maps.get(value,[0]*self._words()))
    def select(self,value): return self._records(self.bitmap(value))
    # >>> bitmap-ops
    def select_and(self,a,b): return self._combine(a,b,lambda x,y:x&y)
    def select_or(self,a,b): return self._combine(a,b,lambda x,y:x|y)
    def select_not(self,value):
        bits=[(~word)&MASK for word in self.bitmap(value)]; self._ops+=len(bits)
        if bits and self._count%64: bits[-1]&=(1<<(self._count%64))-1
        return self._records(bits)
    # <<< bitmap-ops
    def _combine(self,a,b,op):
        left=self.bitmap(a); right=self.bitmap(b); self._ops+=len(left)
        return self._records([op(x,y) for x,y in zip(left,right)])
    @staticmethod
    def _records(bits): return [w*64+b for w,value in enumerate(bits) for b in range(64) if value&(1<<b)]
    def _words(self): return (self._count+63)//64
    def record_count(self): return self._count
    def distinct_values(self): return len(self._maps)
    def words(self): return len(self._maps)*self._words()
    def reset_ops(self): self._ops=0
    def word_ops(self): return self._ops

def run_length_encode(bits):
    out=[]; i=0
    while i<len(bits):
        run=1
        while i+run<len(bits) and bits[i+run]==bits[i]: run+=1
        out.extend([run,bits[i]]); i+=run
    return out
def run_length_decode(encoded):
    if len(encoded)%2: raise ValueError("encoded stream must be pairs")
    out=[]
    for i in range(0,len(encoded),2): out.extend([encoded[i+1]]*encoded[i])
    return out

class SignatureFile:
    def __init__(self,bits_per_term=2):
        if bits_per_term<1 or bits_per_term>8: raise ValueError("bits_per_term out of range")
        self._bits=bits_per_term; self._docs=[]; self._signatures=[]
    def _term_bits(self,term):
        value=1469598103934665603
        for c in term: value=((value^ord(c))*1099511628211)&MASK
        bits=0
        for i in range(self._bits): bits|=1<<((value+i*17)%64)
        return bits
    def _signature(self,terms):
        result=0
        for term in terms: result|=self._term_bits(term)
        return result
    def add(self,doc,terms): self._docs.append(doc); self._signatures.append(self._signature(terms))
    def candidates(self,terms):
        wanted=self._signature(terms)
        return [doc for doc,sig in zip(self._docs,self._signatures) if sig&wanted==wanted]
    def size(self): return len(self._docs)
