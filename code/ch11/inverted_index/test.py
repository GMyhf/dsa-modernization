import sys
from pathlib import Path
import modern
sys.path.insert(0, str(Path(__file__).parents[2] / "support"))
import shared_cases
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
shared = shared_cases.load()
for case in shared:
    left, right = case.input.split("|", 1)
    operation = modern.intersect if case.operation == "intersect" else modern.difference
    check(operation(shared_cases.integers(left), shared_cases.integers(right)) == shared_cases.integers(case.expected), "T-047 inverted")
print(f"共享用例: {len(shared)}")
print(f"{checks} 项断言")
