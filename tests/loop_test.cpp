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
            --i;
            continue;
        }
        mul *= i;
        --i;
    }
    return mul;
}

int main(int argc, char* argv[])
{
    const unsigned n = argc * 3;
    unsigned s = 3;

    s = sum(s);
    printf("result of sum %d\n", s);

    unsigned m = mul(n);
    printf("result of mul %d\n", m);

    int k = evens_sum(s);
    printf("result of evens sum %d\n", k);

    return 0;
}

