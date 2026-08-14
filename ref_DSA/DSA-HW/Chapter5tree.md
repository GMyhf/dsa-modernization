# 第五章 二叉树

> 见ipad

## 1.一棵二叉树的先序、中序、后序序列如下，其中一部分未标出，请画出该二叉树。

> 先序序列：_ _ C D E _ G H I _ K
> 中序序列：C B _ _ F A _ J K I G
> 后序序列：_ E F D B _ J I H _ A

补全后为:

- 先序序列：A B C D E F G H I J K
- 中序序列：C B E D F A H J K I G
- 后序序列：C E F D B K J I H G A

## 2.证明

> 一颗二叉树的所有终端节点(叶节点)，在前序序列、中序序列、后序序列中都按相同的相对位置出现


## 3.

### 如果一棵huffman树中有199个节点，请求解该huffman树中叶子结点个数

$$
定义和目标： 我们的目标是计算 (\sum_{i=0}^{n} i \cdot 2^i)。
\newline
引入辅助求和公式： 我们知道几何级数的求和公式： [ \sum_{i=0}^{n} 2^i = 2^{n+1} - 1 ]
\newline
考虑求和公式的导数： 我们可以通过对几何级数求和公式求导来得到一个新的公式：
\newline
[ \sum_{i=0}^{n} i \cdot x^i = x \frac{d}{dx} \left( \sum_{i=0}^{n} x^i \right) ] 其中 (x = 2)。
\newline
求导： 首先，我们对几何级数求和公式求导： [ \sum_{i=0}^{n} x^i = \frac{x^{n+1} - 1}{x - 1} ]
\newline
对其求导得到： [ \frac{d}{dx} \left( \sum_{i=0}^{n} x^i \right) = \frac{d}{dx} \left( \frac{x^{n+1} - 1}{x - 1} \right) ] 
\newline
使用商的求导法则： [ \frac{d}{dx} \left( \frac{x^{n+1} - 1}{x - 1} \right) = \frac{(x - 1) \cdot (n+1) x^n - (x^{n+1} - 1) \cdot 1}{(x - 1)^2} ] 
\newline
简化后得到： [ \frac{(n+1) x^n (x - 1) - x^{n+1} + 1}{(x - 1)^2} = \frac{(n+1) x^{n+1} - (n+1) x^n - x^{n+1} + 1}{(x - 1)^2} ] [ = \frac{(n+1) x^{n+1} - x^{n+1} - (n+1) x^n + 1}{(x - 1)^2} ] [ = \frac{n x^{n+1} - (n+1) x^n + 1}{(x - 1)^2} ]

代入 (x = 2)： [ \sum_{i=0}^{n} i \cdot 2^i = 2 \cdot \frac{d}{dx} \left( \sum_{i=0}^{n} x^i \right) \bigg|_{x=2} ] [ = 2 \cdot \frac{n \cdot 2^{n+1} - (n+1) \cdot 2^n + 1}{(2 - 1)^2} ] [ = 2 \cdot (n \cdot 2^{n+1} - (n+1) \cdot 2^n + 1) ] [ = 2n \cdot 2^{n+1} - 2(n+1) \cdot 2^n + 2 ] [ = 2^{n+1} \cdot (2n - n - 1) + 2 ] [ = 2^{n+1} \cdot (n - 1) + 2 ]
\newline
结果
[ \sum_{i=0}^{n} i \cdot 2^i = 2^{n+1} \cdot (n - 1) + 2 ]
$$