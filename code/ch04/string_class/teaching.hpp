// 字符串类 String —— 教学版。原书【代码4.1】【算法4.3】【算法4.4】【算法4.5】。
//
// 一个文件、一个类、能直接编译运行，给「第一次读这一节」的人看。
//
// 本节的教学内容是**字符串的变长存储管理**：动态分配、按长度重新开辟、拷贝与释放。
// 所以这里是手写的 char* 缓冲区，不是 std::string——换成 std::string，这一节就没了。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  三法则（析构 + 拷贝构造 + 拷贝赋值）；
//   工程版  补齐移动语义（移动之后 data_ 为空，读取路径要多一层 raw() 兜底）、
//           比较运算符全家、copy-and-swap。
// 两份都在闸门里真编译真运行。先读这一份，4.2.5「进阶（选读）」再读那一份。
#pragma once

#include <cstddef>
#include <cstring>
#include <optional>
#include <stdexcept>

class String {
public:
    using size_type = std::size_t;

    // 空串。**注意它仍然申请了 1 个字节**，里面放一个 '\0'。
    // 这样 c_str() 永远返回一个合法的 C 字符串，调用方不必先判空指针。
    String() : data_(new char[1]), size_(0) {
        data_[0] = '\0';
    }

    // 从 C 字符串构造。参数是 `const char*` 而不是原书的 `char*`——
    // 原书那个签名让它自己书里的例子 `String s1 = "Hello";` 从 C++11 起就编译不过：
    // 字符串字面量的类型是 const char[6]，绑不到 char*。
    //
    // 这里**故意不加 explicit**，为的就是保住 `String s1 = "Hello";` 这种写法。
    String(const char* s) {   // NOLINT(google-explicit-constructor)
        if (s == nullptr) {
            throw std::invalid_argument("String: 不能用空指针构造字符串");
        }
        size_ = std::strlen(s);
        data_ = new char[size_ + 1];
        std::memcpy(data_, s, size_ + 1);   // +1 把结尾那个 '\0' 也带上
    }

    ~String() { delete[] data_; }

    // 三法则：自己管着 new 出来的缓冲区，拷贝必须自己写。
    // 原书正文里描述过赋值时「必须释放 s1 的原有空间」，却没有把拷贝构造和
    // 拷贝赋值作为清单给出。只有析构没有这两个，一次 `String b = a;` 就是二次释放。
    String(const String& other) : data_(new char[other.size_ + 1]), size_(other.size_) {
        std::memcpy(data_, other.data_, size_ + 1);
    }

    String& operator=(const String& other) {
        if (this == &other) {
            return *this;
        }
        char* fresh = new char[other.size_ + 1];   // 先备好新的
        std::memcpy(fresh, other.data_, other.size_ + 1);
        delete[] data_;                            // 再释放旧的
        data_ = fresh;
        size_ = other.size_;
        return *this;
    }

    size_type size() const { return size_; }
    size_type length() const { return size_; }
    bool empty() const { return size_ == 0; }
    const char* c_str() const { return data_; }

    void clear() {
        char* fresh = new char[1];
        fresh[0] = '\0';
        delete[] data_;
        data_ = fresh;
        size_ = 0;
    }

    // 按下标取字符。越界抛异常，不是返回一个随便什么值。
    char at(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("String::at: 下标越界");
        }
        return data_[index];
    }

    // 在串尾添加一个字符。
    //
    // **变长存储的代价在这里看得最清楚**：字符串长度变了，就得重新申请一块、
    // 把老内容拷过去、再把老的还回去。所以 append 一个字符是 O(n)，不是 O(1)。
    //
    // 返回自身引用，于是 `s.append('a').append('b')` 可以连着写；
    // 原书【代码4.1】声明的是**按值返回**，调用方从签名看不出它改不改本串。
    String& append(char c) {
        char* fresh = new char[size_ + 2];        // 老内容 + 新字符 + '\0'
        std::memcpy(fresh, data_, size_);
        fresh[size_] = c;
        fresh[size_ + 1] = '\0';
        delete[] data_;
        data_ = fresh;
        ++size_;
        return *this;
    }

    // 把 s 接在本串后面。同样是「重新申请、拷两段、释放旧的」。
    String& concatenate(const char* s) {
        if (s == nullptr) {
            throw std::invalid_argument("String::concatenate: 空指针");
        }
        size_type extra = std::strlen(s);
        char* fresh = new char[size_ + extra + 1];
        std::memcpy(fresh, data_, size_);
        std::memcpy(fresh + size_, s, extra + 1);
        delete[] data_;
        data_ = fresh;
        size_ += extra;
        return *this;
    }

    // 【算法4.5】从 pos 开始取长度至多 len 的子串。
    //
    // 原书在 `pos >= size` 时 `return NULL;`。那不是「返回空串」——
    // NULL 会去走 String(char*) 构造函数，然后 strlen(nullptr) 当场段错误。
    // 这里越界就抛异常，让错误停在发生的地方。
    // pos == size() 是合法的，得到空串（「从末尾取 0 个字符」）。
    String substr(size_type pos, size_type len) const {
        if (pos > size_) {
            throw std::out_of_range("String::substr: 起始位置越界");
        }
        size_type available = size_ - pos;
        size_type take = (len < available) ? len : available;   // 原书的 if (n > left) n = left
        String result;
        char* fresh = new char[take + 1];
        std::memcpy(fresh, data_ + pos, take);
        fresh[take] = '\0';
        delete[] result.data_;
        result.data_ = fresh;
        result.size_ = take;
        return result;
    }

    // 【算法4.4】从 start 开始查找字符 c。找到返回下标，没找到返回空 optional。
    // 原书用 -1 表示没找到——与「位置 0」只差一个符号，忘了判就会读错位置。
    std::optional<size_type> find(char c, size_type start = 0) const {
        for (size_type i = start; i < size_; ++i) {
            if (data_[i] == c) {
                return i;
            }
        }
        return std::nullopt;
    }

    // 【算法4.3】三路比较：负 / 零 / 正 表示 小于 / 等于 / 大于。
    //
    // 原书自己实现了一个 strcmp，返回值固定为 -1/0/1，并在正文里说
    // 「这与 C/C++ 语言中通常的大小比较习惯不一致」——其实不一致的是原书自己：
    // 标准 strcmp 返回的就是差值的符号，调用方只该看符号，不该看具体数值。
    int compare(const String& other) const {
        return std::strcmp(data_, other.data_);
    }

private:
    char* data_;       // 以 '\0' 结尾的字符数组，永远非空
    size_type size_;   // 字符个数，不含结尾的 '\0'
};

inline bool operator==(const String& a, const String& b) { return a.compare(b) == 0; }
inline bool operator!=(const String& a, const String& b) { return a.compare(b) != 0; }
inline bool operator<(const String& a, const String& b) { return a.compare(b) < 0; }
