#include <iostream>

bool if_(int i)
{
    if (i == 0) {
        return false;
    }
    return true;
}

int if_else(int i, int j)
{
    if (i == 0) {
        return i;
    } else {
        i = j;
    }
    return i;
}

void if_else_if(int i, int& j)
{
    if (i == 0) {
        return;
    } else if (i < 0) {
        j = i * (-1);
    }
    j = i;
}

bool if_else_if_else(int i, int& j)
{
    if (i == j) {
        return false;
    } else if (i > j) {
        j = i;
        return true;
    } else {
        j -= i;
    }
    return true;
}

char switch_(int i, char def)
{
    char c;
    switch (i) {
    case 1:
        c = '1';
        break;
    case 2:
        c = '2';
        break;
    case 3:
        return '3';
    case 5:
        return '5';
    case 7:
        c = '7';
    default:
        c = def;
    };
    return c;
}

int main(int argc, char* argv[])
{
    int i = argc;
    int j = 3;
    char def = *argv[1];
    int k = 0;

    bool b = if_(i);
    printf("if call result %d\n", b);

    j = if_else(i, j);
    printf("if_else call result %d\n", j);

    if_else_if(j, k);
    printf("if_else_if call result %d\n", k);

    b = if_else_if_else(k, i);
    printf("if_else_if_else call result %d\n", b);

    char c = switch_(i, def);
    printf("switch call result %d\n", c);

    return 0;
}

