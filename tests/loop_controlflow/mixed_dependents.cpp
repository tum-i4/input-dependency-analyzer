#include <iostream>

int sum(int n)
{
    int sum = 0;
    for (unsigned i = 0; i < n; ++i) {
        sum += i;
    }
    return sum;
}

int evens_sum(int n)
{
    int sum = 0;
    unsigned i = 0;
    unsigned m = 0;
    // dummy outer loop
    while (m < 10) {
        for (; i < n; ) {
            if (i % 2 == 0) {
                sum += i;
            }
            ++i;
        }
        i = 5;
        ++m;
    }
    return sum;
}

int mul(int n)
{
    int mul = 1;
    int i = n;
    while (true) {
        if (i == 0) {
            break;
        }
        if (i == 1) {
            continue;
        }
        mul *= i;
        --i;
    }
    return mul;
}

int complex_sum(int n)
{
    unsigned i = 0;
    int s = sum(n);
    while (i < n) {
        for (unsigned j = i; ; ++j) {
            if (j == n) {
                break;
            }
            s += j;
        }
        ++i;
    }
    return s;
}


extern int getInt();

int main(int argc, char* argv[])
{
    const unsigned n = argc * 3;
    unsigned s = 3;

    s = sum(s);
    unsigned m = mul(n);
    s = complex_sum(m);

    int k = evens_sum(s);

    return 0;
}

