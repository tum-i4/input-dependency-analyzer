
/*
void loop(int l, int u)
{
    int a = 0;
    int b = 0;
    l = 8;
    for (unsigned i = l; i < u; ++i) {
        l = 7;
        if (u == 10) {
            break;
        }
        if (u == 30) {
            continue;
        }
        if (i % 2 == 0) {
            a += l;
            b+= u;
        } else {
            b += l;
            a += u;
        }
        --u;
    }
}
*/

void change_int(int& i)
{
    i = 42;
}

int main(int argc, char* argv[])
{
    int a = 0;
    int c = argc;
    int d = a + c;
    change_int(a);
    int k = a;

    return 0;
}

