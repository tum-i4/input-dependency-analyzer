

extern int get_int();

int main()
{
    const int n = 3;
    const int m = 7;
    int array[n][m] = {{0, 1, 2, 3, 4, 5, 6},
                       {7, 8, 9, 0, 1, 2, 3},
                       {4, 5, 6, 7, 8, 9, 0}};

    int first_row_sum = 0;
    for (unsigned i = 0; i < m; ++i) {
        first_row_sum += array[0][i];
    }

    array[0][1] = get_int();

    first_row_sum += array[0][3];

    return 0;
}

