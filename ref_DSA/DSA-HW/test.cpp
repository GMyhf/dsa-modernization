#include <iostream>
using namespace std;
int main()
{
    int res = 0;
    int k = 6;
    for (int i = 1; i <= 1 << k; i++)
        for (int j = 1; j <= i; j *= 2)
            res++;

    cout << res;
    return 0;
}