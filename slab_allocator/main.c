#include <stdlib.h>
#include <stdio.h>
#include "slab.h"

struct testObj1{
    int w, x, y, z;
};

struct testObj2{
    int test2[50];
};

struct testObj3{
    int test3[100][100];
};

int main(){
    int i;    //Counter variable for later
    kmem_init();
    //creating each cache for each testObj struct.
    struct kmem_cache* test1 = kmem_cache_create("TEST CACHE A", sizeof(struct testObj1));
    struct kmem_cache* test2 = kmem_cache_create("TEST CACHE B", sizeof(struct testObj2));
    struct kmem_cache* test3 = kmem_cache_create("TEST CACHE C", sizeof(struct testObj3));
    //Creating test objects.
    struct testObj1* tester1[750];
    struct testObj2* tester2[35];
    struct testObj3* tester3[2];
    //Print the size of each test object.
    printf("\n\nCaches created:\n");
    printf("\tSize of testObj1: %d\n\tSize of testObj2: %d\n\tSize of testObj3: %d\n\n", sizeof(struct testObj1), sizeof(struct testObj2), sizeof(struct testObj3));
    //Allocation for each testing.
    tester1[0] = kmem_cache_alloc(test1);
    tester2[0] = kmem_cache_alloc(test2);
    tester3[0] = kmem_cache_alloc(test3);
    //Print each address.
    printf("\nObjects allocated:\n\ttester1 address: %d\n\ttester2 address: %d\n\ttester3 address: %d\n", tester1[0], tester2[0], tester3[0]);
    //Testing if each object is manipulatable.
    tester1[0]->w = 5;
    tester2[0]->test2[0] = 10;
    tester3[0]->test3[0][0] = 15;
    //Print the results of the manipulated data.
    printf("\nData manipulated\n");
    printf("\ttest1: %d\n\ttest2: %d\n\ttest3: %d\n\n", tester1[0]->w, tester2[0]->test2[0], tester3[0]->test3[0][0]);
    //Expand the cache and find its offset.
    for(i = 1; i<500; i++){ tester1[i] = kmem_cache_alloc(test1); }
    for(i = 1; i<35; i++){ tester2[i] = kmem_cache_alloc(test2); }
    for(i = 1; i<2; i++){ tester3[i] = kmem_cache_alloc(test3); }
    //Find the addresses of certain locations of the object.
    printf("\nAddress of manipulated data:\n\ttester1: %d\n\ttester2: %d\n\ttester3: %d\n", tester1[253], tester2[5], tester3[1]);
    //Manipulate the data of the locations above.
    tester1[253]->x = 7;
    tester2[5]->test2[0] = 7;
    tester3[1]->test3[0][0] = 7;
    //Verify that the data was manipulated corrected.
    printf("\nSecond Data Manipulation.\n");
    printf("test1: %d\ttest2: %d\ttest3: %d\n", tester1[253]->x, tester2[5]->test2[0], tester3[1]->test3[0][0]);

    //Now testing freeing of objects.
    printf("\nFreeing objects\n");
    //Free each object in testObj1 and 2.
    for(i = 0; i<255; i++){ kmem_cache_free(test1, tester1[i]); }
    for(i = 2; i<35; i++){ kmem_cache_free(test2, tester2[i]); }

    //Checking each cache.
    printf("\nObjects freed\n");
    printf("\n-------TESTING TEST1 CACHE----------\n");
    print_cache_info(test1);
    printf("\n-------TESTING TEST2 CACHE----------\n");
    print_cache_info(test2);
    printf("\n-------TESTING TEST3 CACHE---------\n"); 
    print_cache_info(test3);

    //Testing edge case of overflowing a cache.
    printf("\n-----------OVERFLOW OF TEST1------------\n");
    for(i = 500; i<750; i++){
        tester1[i] = kmem_cache_alloc(test1);
    }
    printf("\n------Current state of test1-----------\n");
    print_cache_info(test1);
    return 0;
}
