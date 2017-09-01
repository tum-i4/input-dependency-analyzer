#include <stdio.h>
void logop(int i) {
	//printf("computed: %i\n", i);
}
long hash =0;
void hashMeInt(int i) {
	//printf("adding hash %i\n", i);
	hash +=i;
}
void hashMeLong(long i) {
	//printf("adding hash %ld\n", i);
	hash +=i;
}


//void dbghashMe(int i, std::string valueName){
//	printf("adding hash %s %i\n",valueName, i);
//        hash +=i;
//} 
void logHash() {
	printf("final hash: %ld\n", hash);
}
