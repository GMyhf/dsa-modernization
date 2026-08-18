import modern
checks=0
def check(v,n):
    global checks; checks+=1
    if not v: raise AssertionError(n)
t=modern.BPlusTree.bulk_load(3,[(10,"a"),(20,"b"),(30,"c"),(50,"d"),(70,"e"),(90,"f")],2)
check(t.height()==2 and t.leaf_count()==3,"页层次")
check(t.insert(60,"x") and t.validate(),"插入后不变量")
check([k for k,_ in t.range(35,80)]==[50,60,70],"叶链范围扫描")
check(t.erase(60) and not t.erase(60),"删除状态")
check(not t.insert(50,"new") and t.find(50)=="new","覆盖")
print(f"{checks} 项断言")
