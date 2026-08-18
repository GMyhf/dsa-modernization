"""稠密/稀疏多级线性索引。"""
DENSE = "dense"
SPARSE = "sparse"
class MultiLevelIndex:
    def __init__(self,kind,records_per_page,entries_per_page):
        if records_per_page<1 or entries_per_page<2:
            raise ValueError("page capacity too small")
        self.kind = kind
        self.rpp = records_per_page
        self.epp = entries_per_page
        self.records = []
        self._levels = []
        self._reads = 0
    def load(self,records):
        self.records = list(records)
        self._levels = []
        if self.kind==SPARSE:
            if any(self.records[i-1][0]>=self.records[i][0] for i in range(1,len(self.records))):
                raise ValueError("sparse index needs a sorted main file")
            bottom = [(self.records[i][0],i//self.rpp) for i in range(0,len(self.records),self.rpp)]
        else:
            bottom = [(key,i) for i,(key,_) in enumerate(self.records)]
            for end in range(len(bottom)-1,0,-1):
                for i in range(end):
                    if bottom[i][0]>bottom[i+1][0]:
                        bottom[i],bottom[i+1] = bottom[i+1],bottom[i]
            if any(bottom[i-1][0]==bottom[i][0] for i in range(1,len(bottom))):
                raise ValueError("duplicate key")
        if bottom:
            self._levels.append(bottom)
        while self._levels and len(self._levels[-1])>self.epp:
            lower = self._levels[-1]
            self._levels.append([(lower[i][0],i//self.epp) for i in range(0,len(lower),self.epp)])
    # >>> index-find
    def find(self,key):
        if not self._levels:
            return None
        page = self._locate(self._levels[-1],0,len(self._levels[-1]),key)
        for level in range(len(self._levels)-1,0,-1):
            page = self._levels[level][page][1]
            self._reads += 1
            first = page*self.epp
            last = min(first+self.epp,len(self._levels[level-1]))
            page = self._locate(self._levels[level-1],first,last,key)
        entry = self._levels[0][page]
        if self.kind==DENSE:
            if entry[0]!=key:
                return None
            self._reads += 1
            return self.records[entry[1]][1]
        self._reads += 1
        for record_key,value in self.records[entry[1]*self.rpp:(entry[1]+1)*self.rpp]:
            if record_key==key:
                return value
        return None
    # <<< index-find
    @staticmethod
    def _locate(level,first,last,key):
        slot = first
        for i in range(first,last):
            if level[i][0]<=key:
                slot = i
            else:
                break
        return slot
    def levels(self):
        return len(self._levels)
    def entries(self):
        return len(self._levels[0]) if self._levels else 0
    def data_pages(self):
        return (len(self.records)+self.rpp-1)//self.rpp
    def page_reads(self):
        return self._reads
    def reset_counters(self):
        self._reads = 0
