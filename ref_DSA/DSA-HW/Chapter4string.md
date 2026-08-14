# 第四章 字符串

## 1.求下列字符串的next数组

> t='abcdaabcddaba'
> s='XYYXYZXZXYYXZ'

t_next=[-1,0,0,0,0,1,1,2,3,4,0,1,2]
s_next=[-1,0,0,0,1,2,0,1,0,1,2,3,4]

## 2.请设计算法消除字符串中的b和ac，需要满足条件
> (1)字符串只能遍历一次  
> (2)不能够使用额外空间，即O(1)的空间  
> 如“acbac”-> " ","ababac"->"aa"

1. 使用一个指针/数组索引```w``` 指向当前写入的位置，另一个指针/索引```r``` 用于遍历字符串。
- - 遍历字符串，如果遇到 "b"，直接跳过；如果遇到 "a"，检查下一个字符：
- - 如果下一个字符是 "c"，则跳过这两个字符。
- - 否则，将 "a" 写入当前位置。
2. 将 write 指针向前移动。
3. 最后，处理指针的位置，将剩余字符移到字符串的开始部分。


## 2.

求已知长度为m的字符串P和长度为n的字符串T，n>m,S=PT(由P和T连接形成的长度为m+n的字符串),类似于next数组，在S上定义next数组set数组，定义nextset[i]如下：
> 
- 分两种情况，说明如何通过nextset数组来判断P是否在文本T中出现  
- - (1)若P不会出现在T尾部，即P...，写出充要条件并证明
- - (2)若P可能出现在T尾部，写出充要条件单不需证明

### (1) 充要条件$nextset数组最后一位<m$

证明
- (充分性)由nextset定义可知，nextset的最后一位$<m$，就说明S最后可以和前面的P匹配的子串长度小于m，而P的长度为m，说明P不会出现在T尾部
- (必要性)同样由nextset数组定义可知，如果P不会出现在T尾部，next数组最后一位必然<m

### (2)若P可能出现在T尾部，写出充要条件但不需要证明

$nextset数组最后一位=m$

## 3.

> 求给定一个长度为n的字符串S，和一个整数i，要求设计一个线性时间，常数空间的算法，将$S[0...i]移动到S[(n-i-1)...(n-1)],将S[(i+1)...(n-1)]移动到S[0...(n-i-2)]$。如输入“HELLOWORLD”,i=4,输出“WORLDHELLO”.

```cpp

void reverse(char* str, int start, int end) {
    while (start < end) {
        std::swap(str[start], str[end]);
        start++;
        end--;
    }
}

void rotateString(char* str, int n, int i) {
    // 反转 S[0...i]
    reverse(str, 0, i);

    // 反转 S[i+1...n-1]
    reverse(str, i + 1, n - 1);

    // 反转整个字符串
    reverse(str, 0, n - 1);
}
```
调用rotateString(S,n,i)即可实现


$$
第 ( i ) 个子节点的编号为： [ \text{child}(N, i) = k \cdot N + i + 1 ] 其中，( i ) 的取值范围是 ( 0 ) 到 ( k-1 )。
$$