#include <cstdio>
#include <cstdlib>

void sort(int n, int array[])
{
    for (unsigned i = 0; i < n; ++i) {
        unsigned j = i;
        while (j < n - 1) {
            if (array[j] > array[j + 1]) {
                int tmp = array[j];
                array[j] = array[i];
                array[i] = tmp;
            }
            ++j;
        }
    }
}


int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("wrong number of arguments. Expects number of elements to sort");
    }
    const int SIZE = 7;
    int array[] = {42, 78, 54, 12, 7, 1, 0};
    int size = atoi(argv[1]);

    // get minimum
    if (SIZE < size) {
        size = SIZE;
    }

    // sorts first size elements of array
    sort(SIZE, array); // input indep
    sort(size, array); // input dep

    for (unsigned i = 0; i < size; ++i) {
        printf("%d, ", array[i]);
    }
    printf("\n");
    return 0;
}
