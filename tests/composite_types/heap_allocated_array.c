#include <stdlib.h>

extern int get_int();

int* create_array(int size)
{
    int* array = malloc(sizeof(int) * size);
    for (unsigned i = 0; i < size; ++i) {
        array[i] = i;
    }
    // last element is input dep
    array[5] = get_int();
    return array;
}

int main()
{
    int size = 5;
    int* array = create_array(size);

    int last_elem = array[5];
    int first_elem = array[0];
    int diff = last_elem - first_elem;

    array[5] = 42;
    diff = array[5] - first_elem;

    return 0;
}

