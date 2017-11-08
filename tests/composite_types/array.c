
extern int get_int();

int main()
{
    int array[] = {0, 1, 2, 3, 4, 5};

    array[0] = get_int(); // input dep
    array[1] = 3; // indep

    int sum = array[2] + array[0]; // dep
    array[0] = 42; //indep
    sum = array[0]; // indep

    int indx = 5; //indep
    array[indx] = get_int(); // make whole array dep

    sum = array[indx]; // dep
    return 0;
}

