# OJ复习，算法实现

>伪代码是很重要的，可以节约记忆时的细节，是直观理解到代码实现的桥梁。
>// 记住这些黄金组合：
// 需要快速查找：unordered_map + unordered_set
// 需要有序数据：map + set
// 需要双端操作：deque

1. 线性表：模拟、map的使用、scanf格式，insert
2. 栈与队列：使用栈、队列操作，在滑动窗口那里还是不太懂用数组模仿双端队列。
3. 字符串：KMP匹配
4. 二叉树
5. 树：并查集
6. 图
7. 内排序

## 邪恶STL小知识

- string 构造函数是```string s(int l, char c);```，可以直接用cin读入string
  - ```string.substr(pos, len),substr```; // 截取子串,substr(pos)表示从pos开始到结尾,substr(len)表示从0开始len长度,substr(begin,end)表示从begin到end-1
  - ```string.find(substr, pos)```; // 查找子串，返回位置，找不到返回-1
  - ```string.insert(pos, substr)```; // 插入子串
- 不同格式转换函数
  - ```stoi(string s);```字符串转整数
  - ```to_string(int n);```整数转字符串
  - ```char c = s[i];```字符串取单个字符
  - stoll, stof, stod等类似
- cin读入字符串时遇到空格会停止，可以用cin.getline(str, size)读入一整行,或者用getline(cin, stringVar),利用cin.ignore()跳过换行符
- ```pair<string,int>```可以作为键值对放在vector里，甚至可以在sort里根据string的字典序排序
- map声明：```map<string,int,greater<string>> mp;```，greater实现降序，正常是升序,unordered_map声明是```unordered_map<string,int> mp;```

## 往年考试题

### 25D序列的中位数

窗口的思维不行，总会有更远的数进入，要用优先队列左右各维护一个最大和最小的极值,优先队列就是按一定的方式得到那个排序，确实是堆实现的

- 重点在rebalance函数的使用时机

```cpp


void rebalance( priority_queue<long long>&left,
    priority_queue<long long,vector<long long>,greater<long long>>& right){
        if((int)left.size()<(int)right.size()){
            left.push(right.top());right.pop();//为空的话放入left，避免太少
        }
        else if((int)left.size()>(int)right.size()+1){
            right.push(left.top());
            left.pop();
        }
    }

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int N;
    if(!(cin>>N))return 0;
    vector<long long> A(N);
    for(int i=0;i<N;++i)cin>>A[i];
    priority_queue<long long>left;//最大堆，装较小的一半
    priority_queue<long long,vector<long long>,greater<long long>> right;
        for(int i=0;i<N;++i){
            long long x=A[i];
            if(left.empty()||x<=right.top()){
              left.push(x);
            }
            else right.push(x);
            rebalance(left,right);
            if(i&1==0){cout<<left.top()<<endl;}
        }
    
    return 0;
}
```
