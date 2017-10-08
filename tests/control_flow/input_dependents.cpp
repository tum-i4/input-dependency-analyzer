#include <iostream>
#include <cstdlib>

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
    int i = atoi(argv[1]);
    char def = *argv[2];
    int k = 0;
    int j = k + 3;

    if_(k); // input indep mask {0} if_1

    bool b = if_(i); // input_dep mask {1} if_2

    j = if_else(i, j); // mask {1, 0} if_else3

    if_else_if(j, k); // {1 0} if_else_if4
    b = if_else_if_else(k, i); //{1, 1} if_else_if_else5
    def = switch_(k, def); //{1, 1} switch6
    
    return 0;
}

