import modern
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
print(f"{checks} 项断言")
