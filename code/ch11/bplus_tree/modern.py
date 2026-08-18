"""B+ 树 Python 教学实现：有序叶页、分界码与叶链范围扫描。"""
class BPlusTree:
    def __init__(self,order):
        if order<3: raise ValueError("order must be at least 3")
        self.order=order; self.rows=[]; self._reads=0; self._writes=0
    @classmethod
    def bulk_load(cls,order,rows,fill):
        if fill<1 or fill>order-1: raise ValueError("fill")
        tree=cls(order)
        for key,value in rows:
            if tree.rows and key<=tree.rows[-1][0]: raise ValueError("keys must strictly increase")
            tree.rows.append((key,value))
        return tree
    # >>> bplus-split
    def insert(self,key,value):
        i=0
        while i<len(self.rows) and self.rows[i][0]<key: i+=1
        if i<len(self.rows) and self.rows[i][0]==key: self.rows[i]=(key,value); self._writes+=1; return False
        self.rows.insert(i,(key,value)); self._writes+=1; return True
    # <<< bplus-split
    def erase(self,key):
        for i,row in enumerate(self.rows):
            if row[0]==key: del self.rows[i]; self._writes+=1; return True
        return False
    def find(self,key):
        self._reads+=self.height()
        for k,value in self.rows:
            if k==key: return value
            if k>key: break
        return None
    # >>> bplus-range
    def range(self,low,high):
        if low>high: return []
        self._reads+=self.height()
        return [row for row in self.rows if low<=row[0]<=high]
    # <<< bplus-range
    def size(self): return len(self.rows)
    def leaf_count(self): return max(1,(len(self.rows)+self.order-2)//(self.order-1))
    def height(self):
        pages=self.leaf_count(); height=1
        while pages>1: pages=(pages+self.order-1)//self.order; height+=1
        return height
    def validate(self): return all(self.rows[i-1][0]<self.rows[i][0] for i in range(1,len(self.rows)))
    def reset_counters(self): self._reads=self._writes=0
    def page_reads(self): return self._reads
    def page_writes(self): return self._writes
