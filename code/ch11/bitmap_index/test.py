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
check(i.record_count()==5 and i.distinct_values()==2,"记录数和不同取值")
check(i.select("未知")==[],"未知取值为空")
check(i.select_and("男","女")==[],"互斥取值交集为空")
check(i.select_or("男","女")==[0,1,2,3,4],"两个取值并集覆盖全部")
check(i.bitmap("男")[0] == (1<<0)|(1<<3)|(1<<4),"位图位位置")
j = modern.BitmapIndex()
for n in range(200):
    j.add_record("a" if n%3==0 else "b")
j.reset_ops()
check(j.select_and("a","b")==[] and j.word_ops()==4,"字操作计数")
check(j.select_or("a","b")==list(range(200)),"跨多个机器字求并")
check(j.select_not("a")==j.select("b"),"跨机器字取反")
check(j.record_count()==200 and j.words()==8,"机器字数量")
bits = [0]*20+[1]*3
check(modern.run_length_decode(modern.run_length_encode(bits))==bits,"压缩可逆")
check(modern.run_length_encode([])==[],"空位图编码")
check(modern.run_length_decode([])==[],"空编码解码")
raised = False
try:
    modern.run_length_decode([3])
except ValueError:
    raised = True
check(raised,"奇数长度编码被拒绝")
s = modern.SignatureFile()
s.add(1,["a","b"])
check(1 in s.candidates(["a"]),"签名无假阴性")
check(s.size()==1,"签名文档数")
check(2 not in s.candidates(["missing"]),"未加入文档不在候选")
raised = False
try:
    modern.SignatureFile(0)
except ValueError:
    raised = True
check(raised,"签名位数范围")
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
