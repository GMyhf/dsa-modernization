import modern
checks=0
def check(v,n):
    global checks; checks+=1
    if not v: raise AssertionError(n)
d=modern.MultiLevelIndex(modern.DENSE,2,2); d.load([(3,"c"),(1,"a"),(2,"b")])
check(d.find(2)=="b" and d.find(9) is None,"稠密索引无序主文件")
check(d.levels()==2 and d.entries()==3,"多级索引")
s=modern.MultiLevelIndex(modern.SPARSE,2,2); s.load([(1,"a"),(2,"b"),(3,"c")]); check(s.find(3)=="c","稀疏索引")
raised=False
try: s.load([(2,"b"),(1,"a")])
except ValueError: raised=True
check(raised,"稀疏主文件必须有序")
print(f"{checks} 项断言")
