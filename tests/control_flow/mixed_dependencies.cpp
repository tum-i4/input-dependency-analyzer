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
    char def = *argv[2];
    int k = 0;

    bool b = if_(i); // {1}
    j = if_else(i, j); // {1, 0}
    if_else_if(j, k); // {1, 0}
    b = if_else_if_else(k, i); // {1, 1}
    char c = switch_(3, def); // {0, 1}
    def = switch_(j, c); // {1, 1}
    return 0;
}

