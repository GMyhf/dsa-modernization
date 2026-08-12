// 字符串类 String —— 原书【代码4.1】【算法4.3】【算法4.4】【算法4.5】的现代化实现。
//
// 本节的教学内容是**字符串的变长存储管理**：动态分配、按长度重新开辟、拷贝与释放。
// 因此按 D-001 用裸 char* 加显式五法则，不换成 std::string——换了这一节就没了。
//
// 匹配算法是 4.3 节的事，见 code/ch04/pattern_matching。
#pragma once

#include <cstddef>
#include <cstring>
#include <optional>
#include <stdexcept>

namespace dsa {

// >>> class-head
/// 变长字符串。内部保存一块以 '\0' 结尾的字符数组和当前长度。
///
/// 与原书 String 的差别：构造函数取 const char*（原书取 char*，
/// 使书中自己的例子 `String s1 = "Hello";` 在 C++11 起就是非法转换）；
/// 越界抛 std::out_of_range，而不是 `return NULL` 让调用方拿到一个必然崩溃的对象；
/// 补齐五法则；不做任何 I/O。
class String {
public:
    using size_type = std::size_t;

    /// 空串。注意它仍然持有一块 1 字节的缓冲区，于是 c_str() 永远可用、永不为空指针。
    String() : data_(new char[1]{'\0'}), size_(0) {}

    /// 从 C 字符串构造。故意**不加 explicit**：原书 `String s1 = "Hello";` 这种写法
    /// 是本节的教学用例，保留它；代价是隐式转换，值得知道但这里可以接受。
    String(const char* s) {  // NOLINT(google-explicit-constructor)
        if (s == nullptr) {
            // 原书的 Substr 在越界时 `return NULL`，随后 strlen(nullptr) 当场 SEGV。
            // 这里把它挡在门口，并且说清楚是什么问题。
            throw std::invalid_argument("String: 不能用空指针构造字符串");
        }
        size_ = std::strlen(s);
        data_ = new char[size_ + 1];
        std::memcpy(data_, s, size_ + 1);
    }
    // <<< class-head

    // >>> rule-of-five
    // 原书只在正文里描述了赋值时"必须释放 s1 的原有空间(delete [] s1.str)"，
    // 却没有把拷贝构造和拷贝赋值作为清单给出。只要有析构函数而没有这两个，
    // 一次 `String b = a;` 就是二次释放——与第 2、3 章是同一个错误。
    String(const String& other) : data_(new char[other.size_ + 1]), size_(other.size_) {
        // 注意读的是 other.raw() 不是 other.data_：源可能是被移动过的对象（data_ 为空），
        // 从空指针 memcpy 即便长度为 0 也是未定义行为。
        std::memcpy(data_, other.raw(), size_ + 1);
    }

    String& operator=(const String& other) {
        if (this != &other) {
            String copy(other);  // 拷贝并交换：自赋值安全，且拷贝失败时原对象不受影响
            swap(copy);
        }
        return *this;
    }

    /// 移动**不分配**：直接接管缓冲区，把对方置为 nullptr。
    /// 被移动方仍是一个可用的空串——读取路径统一走 raw()，它在 data_ 为空时返回 ""。
    /// （若在这里 new 一块空缓冲区来"修复"对方，移动就不再是 noexcept 的了。）
    String(String&& other) noexcept : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    String& operator=(String&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    ~String() { delete[] data_; }
    // <<< rule-of-five

    void swap(String& other) noexcept {
        char* tmp_data = data_;
        size_type tmp_size = size_;
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = tmp_data;
        other.size_ = tmp_size;
    }

    [[nodiscard]] size_type size() const noexcept { return size_; }
    [[nodiscard]] size_type length() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] const char* c_str() const noexcept { return raw(); }

    /// 清空。原书 clear() 的语义是"把串清空"，这里保持一致。
    void clear() {
        String empty_string;
        swap(empty_string);
    }

    [[nodiscard]] char at(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("String::at: 下标越界");
        }
        return raw()[index];
    }

    // >>> append
    /// 在串尾添加一个字符，返回自身引用。
    ///
    /// 原书【代码4.1】声明的是 `string append(const char c);`——**按值返回**。
    /// 代码4.1 只有声明没有函数体，所以「它到底改不改本串」在书里无从查证；
    /// 而这正是问题所在：一个修改器按值返回，调用方无法从签名判断
    /// `s.append('x');` 是改了 s 还是返回了一个新串而 s 原封不动。
    /// 返回自身引用把这件事说死，同时支持链式调用。
    String& append(char c) {
        char* fresh = new char[size_ + 2];
        std::memcpy(fresh, raw(), size_);
        fresh[size_] = c;
        fresh[size_ + 1] = '\0';
        delete[] data_;
        data_ = fresh;
        ++size_;
        return *this;
    }

    /// 把 s 连接在本串后面。s 为空指针时抛 std::invalid_argument。
    String& concatenate(const char* s) {
        if (s == nullptr) {
            throw std::invalid_argument("String::concatenate: 空指针");
        }
        const size_type extra = std::strlen(s);
        char* fresh = new char[size_ + extra + 1];
        std::memcpy(fresh, raw(), size_);
        std::memcpy(fresh + size_, s, extra + 1);
        delete[] data_;
        data_ = fresh;
        size_ += extra;
        return *this;
    }

    String& operator+=(char c) { return append(c); }
    String& operator+=(const String& other) { return concatenate(other.c_str()); }
    // <<< append

    // >>> substr
    /// 从 pos 开始抽取长度至多为 len 的子串。
    ///
    /// 原书【算法4.5】在 `pos >= size` 时 `return NULL;`——那不是"返回空串"，
    /// 而是拿 NULL 走 String(char*) 构造函数，接着 strlen(nullptr) 当场崩溃
    /// （证据见 legacy.md 缺陷 3）。这里越界就抛 std::out_of_range，
    /// 让错误停在发生的地方，而不是变成调用方某处的段错误。
    ///
    /// pos == size() 是合法的，得到空串——与"从末尾取 0 个字符"的直觉一致。
    [[nodiscard]] String substr(size_type pos, size_type len) const {
        if (pos > size_) {
            throw std::out_of_range("String::substr: 起始位置越界");
        }
        const size_type available = size_ - pos;
        const size_type take = len < available ? len : available;  // 原书的 if (n > left) n = left
        String result;
        char* fresh = new char[take + 1];
        std::memcpy(fresh, raw() + pos, take);
        fresh[take] = '\0';
        delete[] result.data_;
        result.data_ = fresh;
        result.size_ = take;
        return result;
    }
    // <<< substr

    // >>> find-compare
    /// 从 start 开始查找字符 c，返回下标；没有则 std::nullopt。
    /// 原书 `int find(const char c, const int start)` 用 -1 表示没找到，
    /// 与"位置 0"只差一个符号。
    [[nodiscard]] std::optional<size_type> find(char c, size_type start = 0) const {
        for (size_type i = start; i < size_; ++i) {
            if (raw()[i] == c) {
                return i;
            }
        }
        return std::nullopt;
    }

    /// 三路比较，负/零/正 表示 小于/等于/大于。
    ///
    /// 原书【算法4.3】自己实现了一个 strcmp，返回值固定为 -1/0/1，
    /// 并在正文里指出"这与 C/C++ 语言中通常的大小比较习惯(0和非0)不一致"——
    /// 其实不一致的是原书自己：标准 strcmp 返回的就是差值的符号，
    /// 调用方只该看符号，不该看具体数值。这里保持标准语义。
    [[nodiscard]] int compare(const String& other) const noexcept {
        return std::strcmp(raw(), other.raw());
    }
    // <<< find-compare

    friend bool operator==(const String& a, const String& b) noexcept { return a.compare(b) == 0; }
    friend bool operator!=(const String& a, const String& b) noexcept { return !(a == b); }
    friend bool operator<(const String& a, const String& b) noexcept { return a.compare(b) < 0; }
    friend bool operator>(const String& a, const String& b) noexcept { return b < a; }
    friend bool operator<=(const String& a, const String& b) noexcept { return !(b < a); }
    friend bool operator>=(const String& a, const String& b) noexcept { return !(a < b); }

private:
    /// 恒有效的只读视图：被移动之后 data_ 为空，这里返回静态空串。
    /// 有了它，"被移动方仍是可用的空串"这个保证不需要任何分配就能成立。
    [[nodiscard]] const char* raw() const noexcept { return data_ != nullptr ? data_ : ""; }

    char* data_;
    size_type size_;
};

inline void swap(String& a, String& b) noexcept { a.swap(b); }

}  // namespace dsa
