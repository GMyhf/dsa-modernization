# -*- coding: utf-8 -*-
"""第2章 线性表 —— 课件内容。

素材出自 book/ch02-linear-list.md，代码摘自 code/ch02/array_list/ 与
code/ch02/linked_list/ 的教学版，未作改写。
"""

FIG = '../book/assets/scan/'

META = {
    'title': '第2章　线性表',
    'subtitle': '顺序表与链表 · 三法则 · 按位置访问 vs 改链接',
    'footer': '数据结构与算法 · 第2章 线性表',
    'info': ['《数据结构与算法》张铭、王腾蛟、赵海燕　高等教育出版社 2008',
             '现代化重制版讲义　代码按 C++17 重写，全部可编译、可运行'],
}

SLIDES = [

    ('bullets', '本章要回答三个问题', [
        '**一串同类型元素排成唯一的先后次序，怎么存？**',
        '- 2.2 顺序表：连续放在数组里；2.3 链表：用链接保存相邻关系',
        '**为什么同一个操作，在两种结构上代价差一个数量级？**',
        '- 关键是分清「按位置找元素」和「已知结点后改链接」这两类操作',
        '**自己管着 `new` 出来的内存，最少要写哪几个函数？**',
        '- 三法则。这是本章第一次、也不是最后一次遇到它',
    ]),

    ('key', '本章的一句话',
     '顺序表按下标读是 O(1)，插删要搬 O(n) 个元素；'
     '链表已知前驱后插删是 O(1)，但找到那个前驱仍是 O(n)。'),

    ('section', '第 1 节', '2.1 线性表的概念', '定义、结构特点与抽象数据类型'),

    ('bullets', '2.1 线性表的定义', [
        '**线性表**是由**元素**组成的一种有限且有序的序列。用第 1 章的二元组写出来：',
        '- `K = {k₀, k₁, …, kₙ₋₁}`，`R = {r}`，`r = {<kᵢ, kᵢ₊₁> | 0 ≤ i ≤ n-2}`',
        '元素个数 `n` 是**长度**：`n = 0` 是空表；`k₀` 是**表首**，`kₙ₋₁` 是**表尾**',
        '关系 `r` 就是前驱 / 后继关系，具有**反对称性和传递性**',
        '逻辑特征：**每个结点最多只有一个前驱、一个后继**',
        '同一种线性结构在不同场合有不同称谓：顺序表、链表、串、栈、顺序文件',
    ]),

    ('bullets', '线性结构的两个特点，运算的五个类别', [
        '**均匀性** —— 同一线性表中各元素必定具有相同的数据类型和长度',
        '**有序性** —— 各元素在表中都有自己的位置，相对位置是线性的',
        '运算按特性分五类：',
        '- ① 创建实例；② 析构并释放空间',
        '- ③ **获取信息**：由内容找位置、由位置读内容 —— **不改变表**',
        '- ④ **改变内容或结构**：更新、插入、删除、清空',
        '- ⑤ 辅助管理：求当前长度',
    ]),

    ('table', '要定下来的是这张表', [
        ['运算', '含义', '时间代价'],
        ['`at(i)` / `set(i, x)`', '按下标取值、改值', '**O(1)**'],
        ['`find(x)`', '按内容查位置；找不到返回「没有」', 'O(n)'],
        ['`insert(i, x)`', '在位置 i 插入', 'O(n)'],
        ['`append(x)`', '在表尾追加', '摊还 O(1)'],
        ['`remove(i)`', '删除位置 i 的元素并把它带回来', 'O(n)'],
        ['`size()` / `empty()` / `clear()`', '长度、判空、置空', 'O(1)'],
    ], '这张表定的是**顺序表**的代价。同一张表在链表上是另一组数字，2.4 节对比'),

    ('bullets', '原书【代码2.1】的两处硬伤', [
        '**① 它声明了 `bool delete(const int p);`**',
        '- `delete` 是 C++ **关键字**，不能作函数名。整章的删除操作都建立在这个名字上',
        '**② 它写的是 `class List { void clear(); ... };`，通篇没有 `public:`**',
        '- `class` 的默认访问权限是 private，于是这个 ADT 的**每一个运算都调不到**',
        '- 同一本书第 3 章的代码3.1 是写了 `public:` 的',
        '第 2 点值得停一下：**一个所有运算都私有的 ADT，语法上成立，语义上是空的**',
    ]),

    ('section', '第 2 节', '2.2 顺序表', '连续存储、随机访问，与三法则'),

    ('code', '先跑一遍：顺序表', '''ArrayList<int> values;
values.append(10);
values.append(30);
values.insert(1, 20);          // 要把 30 右移一位 —— 顺序表的固有代价

for (int value : values) {     // 有 begin()/end()，range-for 直接可用
    std::cout << ' ' << value;
}

if (auto pos = values.find(20)) {           // find 返回 optional
    std::cout << "\\n查找 20 的下标: " << *pos << '\\n';   // 1
}

std::cout << "删除位置 1 得到 " << values.remove(1);      // 20''',
     '输出：顺序表 10 20 30 / 查找 20 的下标: 1 / 删除位置 1 得到 20，剩余: 10 30'),

    ('code', '2.2.1 类定义：原书四个成员，这里三个', '''template <typename T>
class ArrayList {
public:
    explicit ArrayList(size_type initial_capacity = 8)
        : data_(new T[initial_capacity]), capacity_(initial_capacity), size_(0) {}

    ~ArrayList() { delete[] data_; }
    // ... 三法则的另外两个见后面

private:
    T* data_;             // 指向底层数组      —— 原书 T* aList
    size_type capacity_;  // 数组能放多少个    —— 原书 int maxSize
    size_type size_;      // 现在放了几个      —— 原书 int curLen
};                        // 原书还有第四个：int position，本书删掉了''',
     '`int` 换成 `std::size_t`：「负下标」不该在类型层面存在'),

    ('bullets', '第四个成员 `position` 为什么删掉', [
        '原书用一个**当前位置游标**加 `setPos / setStart / next / prev` 来「依次处理元素」',
        '这个设计今天不能要 —— **遍历状态一旦住进容器**：',
        '- `const` 对象没法遍历（游标要改）',
        '- 两处代码不能同时遍历，**嵌套遍历直接互相踩**',
        '本书删掉它，改为提供 `begin()` / `end()`，把遍历状态交回调用方',
        '- range-for 因此可以直接用在顺序表上',
        '顺带一提：原书那个 `position`，在书中展示的所有算法里**一次都没被用到**',
    ]),

    ('code', '按下标读写：O(1)，这是顺序表的看家本领', '''const T& at(size_type index) const {
    if (index >= size_) {
        throw std::out_of_range("ArrayList::at: 下标越界");
    }
    return data_[index];        // 直接算地址，没有循环
}

void set(size_type index, const T& value) {
    if (index >= size_) {
        throw std::out_of_range("ArrayList::set: 下标越界");
    }
    data_[index] = value;
}''',
     '下标非法是**调用方的错误**，不是可预期状态，所以抛异常而不是返回 optional'),

    ('code', '按内容检索：O(n)，「没找到」写进返回值类型', '''std::optional<size_type> find(const T& value) const {
    for (size_type i = 0; i < size_; ++i) {
        if (data_[i] == value) {
            return i;
        }
    }
    return std::nullopt;
}''',
     '原书【算法2.3】用 `bool getPos(int& p, const T value)`：忘了看返回值，'
     '就会读到一个从没被写过的 p'),

    ('code', '插入：搬 O(n) 个元素', '''void insert(size_type pos, const T& value) {
    if (pos > size_) {                       // pos == size() 就是追加到表尾
        throw std::out_of_range("ArrayList::insert: 插入位置非法");
    }
    if (size_ == capacity_) {
        grow();
    }
    for (size_type i = size_; i > pos; --i) {
        data_[i] = data_[i - 1];             // 从后往前搬，否则会自己覆盖自己
    }
    data_[pos] = value;
    ++size_;
}

void append(const T& value) { insert(size_, value); }''',
     '「从后往前搬」这一条不是风格问题：反过来写，第一步就把后面的数据抹掉了'),

    ('code', '删除，与翻倍扩容', '''T remove(size_type pos) {
    if (pos >= size_) { throw std::out_of_range("ArrayList::remove: 下标越界"); }
    T removed = data_[pos];
    for (size_type i = pos; i + 1 < size_; ++i) {
        data_[i] = data_[i + 1];             // 后面的元素左移一位
    }
    --size_;
    return removed;
}

void grow() {                                // 私有
    size_type next = (capacity_ == 0) ? 1 : capacity_ * 2;
    T* fresh = new T[next];
    for (size_type i = 0; i < size_; ++i) { fresh[i] = data_[i]; }
    delete[] data_;      // 先搬完再释放旧的，顺序反了就会读到已释放的内存
    data_ = fresh;
    capacity_ = next;
}''',
     '**翻倍而不是加一**，才能让 append 的摊还代价保持 O(1)'),

    ('key', '三法则（Rule of Three）',
     '一个类只要写了析构函数、拷贝构造、拷贝赋值中的任意一个，通常这三个都得写。'),

    ('bullets', '为什么三法则是硬要求', [
        '你之所以要写**析构函数**，是因为你在**管资源**',
        '既然在管资源，编译器那份「逐成员照抄」的拷贝就**一定是错的**：',
        '- 照抄一个指针成员的结果是**两个对象指向同一块内存**',
        '- 各自析构时各释放一次 —— **同一块内存被释放两次**',
        '原书 `arrList` 正是如此：**有析构函数，却没有拷贝构造与拷贝赋值**',
        '- 一句普通的 `arrList<int> b = a;` 就会二次释放',
        '- 与第 3 章 `arrStack` 是同一个错误，**同一份 ASan 报告可以复现**',
    ]),

    ('code', '教学版补上的那两个函数', '''ArrayList(const ArrayList& other)
    : data_(new T[other.capacity_]), capacity_(other.capacity_), size_(other.size_) {
    for (size_type i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];              // 深拷贝：各自一块内存
    }
}

ArrayList& operator=(const ArrayList& other) {
    if (this == &other) { return *this; }       // 自赋值：a = a
    T* fresh = new T[other.capacity_];          // 先申请新的
    for (size_type i = 0; i < other.size_; ++i) { fresh[i] = other.data_[i]; }
    delete[] data_;                             // 成功了再释放旧的
    data_ = fresh;
    capacity_ = other.capacity_;
    size_ = other.size_;
    return *this;
}''',
     '注意赋值的顺序：**先申请、拷完、再释放旧的**。倒过来写，`new` 抛异常就毁掉了原对象'),

    ('bullets', '顺带看一眼原书的 `clear()`', [
        '原书写的是：`void clear() { delete [] aList; curLen = position = 0; '
        'aList = new T[maxSize]; }`',
        '**释放整块再重新分配。** 两个问题：',
        '- **没必要** —— 把长度归零即可，容量留着复用',
        '- **不是异常安全的** —— 若 `new` 抛异常，对象就停在「指针已释放、长度已归零」'
        '的破碎状态，之后析构还会**再 `delete[]` 一次**',
        '教学版的 `clear()` 只有一行：`void clear() { size_ = 0; }`',
    ]),

    ('section', '第 3 节', '2.3 链表', '单链表、双链表、循环链表'),

    ('key', '链表换了哪一件事',
     '顺序表用「物理相邻」表示逻辑相邻；链表把逻辑相邻写进结点的**链接域**，'
     '结点可以散落在内存里。'),

    ('image', '图 2.4　单链表示例', FIG + 'fig-2-4.png',
     '结点分两部分：data 域存数据，next 域存后继的地址。终止结点的 next 是空指针'),

    ('bullets', '从图 2.4 能读出来的三件事', [
        '访问**只能从表头开始顺着 next 走**：`head->next->data` 是 8，'
        '`head->next->next->data` 是 50',
        '**结点在内存中不必两两相邻** —— 这正是链表能以常数条指针完成插删的原因',
        '**表越长，这条链越长**：按位置访问是 O(n)，不是 O(1)',
        '为了让「在表尾追加」不必每次走到底，另设一个指向尾结点的 `tail`（图 2.5）',
        '- 有了 `tail`，`append()` 是 **O(1)**',
    ]),

    ('code', '先跑一遍：链表', '''LinkedList<int> values;
values.append(10);
values.append(30);        // 经尾指针 O(1) 接链，不必从头走到尾
values.insert(1, 20);     // 只改两条链接，但要先循链找到前驱

for (int value : values) { std::cout << ' ' << value; }        // 10 20 30

std::cout << "删除位置 0 得到 " << values.remove(0);            // 10
std::cout << "尾元素是 " << values.at(values.size() - 1);       // 30''',
     '同一组操作，链表不搬元素；但按位置找前驱仍是 O(n)'),

    ('image', '图 2.6　引入头结点的单链表', FIG + 'fig-2-6.png',
     '(a) 带头结点的空表；(b) 一个典型的带头结点的单链表。带阴影的就是头结点'),

    ('code', '结点与头结点', '''template <typename T>
class LinkedList {
private:
    struct Node {          // 原书【代码2.6】：一个数据域 + 一根指向后继的链接
        T value;
        Node* next;
    };                     // 放在 private：调用方拿不到指针，就改不坏链

public:
    LinkedList() : head_(new Node), tail_(head_), size_(0) {
        head_->next = nullptr;
    }
    // ...
private:
    Node* head_;           // 头结点：不存放数据的哨兵，等价于原书「第 -1 个结点」
    Node* tail_;           // 尾指针：空表时回指头结点
    size_type size_;
};''',
     '头结点消掉的是「在表头插入 / 删除表头」这个特例：任何位置都变成「找前驱，改它的 next」'),

    ('code', '循链定位：头结点的意义就在这一行', '''Node* predecessor_at(size_type pos) const {
    if (pos > size_) {
        throw std::out_of_range("LinkedList: 下标越界");
    }
    Node* predecessor = head_;          // pos == 0 时前驱就是头结点
    for (size_type i = 0; i < pos; ++i) {
        predecessor = predecessor->next;
    }
    return predecessor;
}''',
     '**没有头结点，这里就要为 pos == 0 单写一套分支**，插入、删除各写一次'),

    ('code', '插入与删除：定位 O(n)，改链接 O(1)', '''void insert(size_type pos, const T& value) {
    Node* predecessor = predecessor_at(pos);      // ① 循链找前驱，O(n)
    Node* fresh = new Node;
    fresh->value = value;
    fresh->next = predecessor->next;              // ② 改两条链接，O(1)
    predecessor->next = fresh;
    if (predecessor == tail_) { tail_ = fresh; }  // 插在表尾，尾指针要跟上
    ++size_;
}

T remove(size_type pos) {
    if (pos >= size_) { throw std::out_of_range("LinkedList::remove: 下标越界"); }
    Node* predecessor = predecessor_at(pos);
    Node* dying = predecessor->next;
    T value = dying->value;
    predecessor->next = dying->next;              // 只改一条链接
    if (dying == tail_) { tail_ = predecessor; }  // 删的是最后一个
    delete dying;
    --size_;
    return value;
}''',
     '**链表的插入不搬元素，但定位要走。** 这就是链表与顺序表的全部分工'),

    ('code', '析构必须循环，不能递归', '''void clear() {
    Node* current = head_->next;
    while (current != nullptr) {        // 沿 next 逐个释放
        Node* dying = current;
        current = current->next;        // 先记下后继，再 delete
        delete dying;
    }
    head_->next = nullptr;
    tail_ = head_;                      // 表空了，尾指针退回头结点
    size_ = 0;
}

~LinkedList() { clear(); delete head_; }   // 头结点是构造时 new 的，最后要还回去''',
     '链长十万级时**递归释放会耗尽运行栈** —— 第 3 章 3.1.5 有实测数字'),

    ('image', '图 2.10　双链表的结点', FIG + 'fig-2-10.png',
     '一个数据域，两根指针：prev 指前驱，next 指后继'),

    ('bullets', '2.3.2 双链表：多一根指针买到什么', [
        '双链结点比单链结点多一根 `prev`。64 位机上多花 **8 字节**',
        '它只买到一件事，但这件事很值：**已知一个结点时，删除它是 O(1)**',
        '- 单链表要做同一件事，得先从头走到它的前驱，**O(n)**',
        '- 因为**单链表从一个结点走不回前一个**',
        '原书在此只给出结点定义（【代码2.12】），没有给出完整的双链表算法',
        '- 本书补了一份完整实现：`code/ch02/doubly_linked_list/`',
    ]),

    ('bullets', '2.3.3 循环链表', [
        '把尾结点的 `next` 接回首结点，**没有 `nullptr` 作为终点**',
        '**不多花任何存储**，却让「从任一结点都能访问到其余全部结点」成为可能',
        '原书举的例子是**进程轮转**：进程串成一个环，`current` 走一步就轮到下一个',
        '只保存 `tail` 就够了：首结点是 `tail->next`，尾插只改两根链接，都是 O(1)',
        '**代价是边界更容易写错** —— 空表、单结点表、多结点表的链接规则不同：',
        '- 遍历必须保存起点、再次遇到起点时停止，**不能写成「走到 nullptr 为止」**',
        '- 删除最后一个结点后要把 `tail` 清空；测试至少覆盖三种长度',
    ]),

    ('section', '第 4 节', '2.4 线性表实现方法的比较', '什么时候不要用哪一个'),

    ('table', '同一组运算，两种结构的代价', [
        ['运算', '顺序表', '链表'],
        ['按下标读写 `at(i)`', '**O(1)**　随机访问', 'O(n)　只能循链数过去'],
        ['按内容查找 `find(x)`', 'O(n)', 'O(n)'],
        ['**已知前驱**后插入 / 删除', 'O(n)　要搬后续元素', '**O(1)**　只改常数条链接'],
        ['按位置插入 / 删除', 'O(n)', 'O(n)　定位是瓶颈'],
        ['表尾追加 `append(x)`', '摊还 O(1)', '**O(1)**　靠尾指针'],
        ['额外空间', '几乎没有（紧凑存储）', '每个结点一根指针'],
    ], '注意第三、第四行的差别 —— 链表的 O(1) 前提是**已经拿到了前驱结点**'),

    ('two', '什么时候不要用',
     '不要用顺序表', [
         '经常插入 / 删除**内部**元素',
         '平均要移动表中一半的元素',
         '无法确定表长的最大值',
         '（顺序表是定长的顺序存储）',
     ],
     '不要用链表', [
         '经常**按位置访问**',
         '按位读比插删频繁',
         '顺链扫描比按下标读费时',
         '**指针本身的存储开销**：',
         '与结点内容之比超过 1:1 要慎重',
     ]),

    ('bullets', '线性表在后面各章的位置', [
        '**存储管理**本质上就是利用线性表管理可利用空间（第 12 章）',
        '**散列方法**是把顺序表和链表结合起来的一种数据结构（第 10 章）',
        '栈、队列、串都是**限制了存取点**的线性表（第 3、4 章）',
        '顺序表提供随机访问，因此适合**二分检索**（第 10 章）与**快速排序**（第 8 章）',
        '实际采用哪种实现，取决于**数据的统计特征和操作特点** —— 没有普适答案',
    ]),

    ('bullets', '本章小结', [
        '**线性结构是最简单也最常用的一种数据结构**，元素之间满足线性关系',
        '线性表通常有**顺序**和**链式**两种存储方式，各运算的实现效率各有千秋',
        '**顺序表**：易用、空间开销小、支持随机访问，是存储**静态数据**的理想选择',
        '**链表**：适用于频繁增删结点，也适用于事先无法确定长度的表',
        '**三法则**：写了析构函数，就得写拷贝构造和拷贝赋值 —— 否则是二次释放',
        '- 原书 `arrList` 与 `arrStack` 都栽在这里，ASan 可以复现',
        '**头结点**是消特例的工具：表头插删不再需要单写一套分支',
    ]),

    ('bullets', '习题（完整题面与参考答案见同名讲义）', [
        '**顺序表**　把 x 插入递增有序表的适当位置；删除从第 i 个开始的 k 个元素',
        '**单链表**　删除值在 (min, max) 之间的全部元素，并释放结点空间，分析复杂度',
        '**归并**　两个递增单链表归并成一个**递减**表 C，**要求利用原表的结点空间**',
        '**分割**　把含字母 / 数字 / 其他三类字符的链表，分割成 3 个循环链表',
        '**上机 1**　`O(n)` 时间、常量辅助空间，非递归**逆置**单链表',
        '**上机 2**　找单链表的**倒数第 m 个**元素，要求既省时间又省空间',
        '**上机 3**　**Josephus 问题**：n 个人围坐，从第 s 个起数到第 m 出列',
    ]),
]
