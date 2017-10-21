
typedef struct {
    int age;
    char* name;
    int is_flying;
} person;

extern char* get_name();
extern void print_name(char* );
extern void print_int(int );

int main()
{
    person me;
    me.age = 131;
    me.name = "^";
    me.is_flying = 0;

    me.name = get_name();
    print_name(me.name);
    print_int(me.age);
    print_int(me.is_flying);

    int age = me.age;

    return 0;
}

