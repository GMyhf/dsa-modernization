import modern
checks = 0
def check(v,n):
    global checks
    checks += 1
    if not v:
        raise AssertionError(n)
check(modern.intersect([1,3,5],[3,4,5])==[3,5],"交")
check(modern.unite([1,3,5],[3,4,5])==[1,3,4,5],"并")
check(modern.difference([1,3,5],[3,4])==[1,5],"差")
i = modern.InvertedIndex()
i.add_document(1,["the","quick","brown"])
i.add_document(2,["brown","quick"])
check(i.and_query(["quick","brown"])==[1,2],"布尔与")
check(i.phrase_query(["quick","brown"])==[1],"位置短语")
check(i.not_query("quick")==[],"非查询")
raised = False
try:
    i.add_document(2,["x"])
except ValueError:
    raised = True
check(raised,"文档号严格递增")
print(f"{checks} 项断言")
