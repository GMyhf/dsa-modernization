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
i = modern.BitmapIndex()
for v in ["男","女","女","男","男"]:
    i.add_record(v)
check(i.select("男")==[0,3,4],"位图选择")
check(i.select_not("男")==[1,2],"尾位掩码")
j = modern.BitmapIndex()
for n in range(200):
    j.add_record("a" if n%3==0 else "b")
j.reset_ops()
check(j.select_and("a","b")==[] and j.word_ops()==4,"字操作计数")
bits = [0]*20+[1]*3
check(modern.run_length_decode(modern.run_length_encode(bits))==bits,"压缩可逆")
s = modern.SignatureFile()
s.add(1,["a","b"])
check(1 in s.candidates(["a"]),"签名无假阴性")
shared = shared_cases.load()
for case in shared:
    values = shared_cases.integers(case.input)
    if case.expected_error:
        raised = False
        try:
            modern.run_length_decode(values)
        except ValueError:
            raised = True
        check(raised, "T-047 bitmap exception")
    else:
        check(modern.run_length_decode(modern.run_length_encode(values)) == shared_cases.integers(case.expected), "T-047 bitmap rle")
print(f"共享用例: {len(shared)}")
print(f"{checks} 项断言")
