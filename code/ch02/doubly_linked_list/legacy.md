# 代码 2.12 证据

原书代码 2.12 只定义双链结点，没有给出容器级的插入、删除和生命周期管理。
这不是把缺失内容误报成原书编译错误，而是明确本书新增的现代化范围。

现代实现补充了：

- `push_front`、`push_back` 和按位置 `insert`；
- `pop_front`、`pop_back` 和按位置 `erase`；
- 前驱/后继链接、空表和单结点边界；
- 拷贝构造、拷贝赋值、移动构造、移动赋值和 `clear`。

结点由 `DoublyLinkedList` 独占释放。`prev` 和 `next` 是非拥有观察链接，不能各自使用
`unique_ptr`，否则一个结点会有两个所有者并在析构时重复释放。

可复现验证：

```text
$ python3 tools/check_code.py code/ch02/doubly_linked_list --allow-degraded
DoublyLinkedList: 6 checks, 0 failures
```

Sanitizer 受当前 macOS `sanitizer_malloc_mac.inc:189` 空探针故障影响，Release-O2 已执行。

边界证据包括空表、首结点、尾结点、单结点和越界位置；拷贝测试确认两个对象不共享结点。
移动测试确认源对象归零，删除测试确认最后一个结点删除后表恢复为空。
这些测试不把 `std::list` 当作实现，容器内部不执行 I/O，也不把所有权交给调用方。
因此本单元的“完整”指接口和生命周期已覆盖，不声称复刻原书不存在的算法编号。
