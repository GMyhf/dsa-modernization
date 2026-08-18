import modern
cost, root = modern.optimal_bst([1,5,4,3], [5,4,3,2,1])
# textbook total cost / root：与 C++ 用同一组书中权重。
assert cost[0][4] == 57 and root[0][4] == 2
empty, roots = modern.optimal_bst([], [7]); assert empty[0][0] == 0 and roots[0][0] == 0
raised=False
try: modern.optimal_bst([1,2],[3,4])
except ValueError: raised=True
assert raised
print("5 项断言")
