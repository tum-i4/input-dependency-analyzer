#include <stdio.h>
#include <vector>

class hash_logger
{
public:
    static hash_logger& get()
    {
        static hash_logger logger(2);
        return logger;
    }

public:
    void log(unsigned number, unsigned long long hash)
    {
        if (log_counts[number] >= max_log_count) {
            return;
        }
        printf("%llu\n", hash);
        ++log_counts[number];
    }

private:
    hash_logger(int log_count)
        : max_log_count(log_count)
        , log_counts(1000)
    {
    }

private:
    unsigned max_log_count;
    std::vector<unsigned> log_counts;
};

extern "C" {
long hash =0;
void logHash(unsigned number) {
    hash_logger::get().log(number, hash);
}

void logop(int i) {
	//printf("computed: %i\n", i);
}
void hashMeInt(int i) {
	//printf("adding hash %i\n", i);
	hash +=i;
}
void hashMeLong(long i) {
	//printf("adding hash %ld\n", i);
	hash +=i;
}


}

