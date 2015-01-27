#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "list.h"
#include "uthash.h"

//Sources:
//          memalign - man page
//          bufctl - man page
//          uthash - github.com/troydhanson/uthash
//
//Author: Jobin Bae
//TODO: Learn more about computer science.

//Defining the size of a page for easy references.
#define PAGE_SIZE 4096

//Struct for caches.
struct kmem_cache{
    char* name;   //Name of cache.
    size_t objSize;   //Size of each object in cache.
    struct double_ll* slabList;   //List of slabs.
    struct Node* freeSlab;  //List of free slabs.
    struct kmem_bufctl* buffhash;   //Buff control struct.
    int large;    //Binary for if the objects for this slab is >1/8 of a page.
    int offSet;   //Storage for offset of slabs.
};

//Struct for slabs.
struct kmem_slab{
    int allocated;  //# of currently allocated objects.
    int maxObjs;    //Max # of objects allowed in slab.
    struct double_ll* freelist;   //Linked list used as freelist.
};

//Struct for buffer control.
struct kmem_bufctl{
    struct kmem_slab* slab;   //Slab of the buffer control.
    void* buffer;     //Buffer variable.
    UT_hash_handle hh;    //Makes this structure hashable.
};

//Prototyped functions.
void kmem_init();
struct kmem_cache *kmem_cache_create(char *name, size_t size);
void *kmem_cache_alloc(struct kmem_cache *cp);
void kmem_cache_free(struct kmem_cache *cp, void *buf);
void kmem_cache_destroy(struct kmem_cache *cp);
void kmem_cache_grow(struct kmem_cache *cp);
void kmem_cache_reap(struct kmem_cache *cp);
void print_cache_info(struct kmem_cache *cp);
void print_slab_info(struct kmem_slab *sp);

//Pre-creation of slab and control structs.
struct kmem_cache* slab_structs = NULL;
struct kmem_cache* ctl_structs = NULL;

/*kmem_init() is the function that creates caches. Currently hardcoded to create
 * slab_struct and ctl_structs.
 */
void kmem_init(){
    slab_structs = kmem_cache_create("slab_structs", sizeof(struct kmem_slab));
    ctl_structs = kmem_cache_create("ctl_structs", sizeof(struct kmem_bufctl));
}

/*kem_cache_create takes the parameters name and size, and then creates a cache
 *with the name and size of the cache taken from the parameters. Each cache is
 *created in multiples of a page.
 *
 *Returns the pointer to the newly created cache.
 */
struct kmem_cache *kmem_cache_create(char *name, size_t size){

    struct kmem_cache* newCache = memalign(4096, sizeof(struct kmem_cache));
    newCache->name = name;    //Name of cache.
    newCache->objSize=size;     //Size of each object.
    newCache->slabList = ll_create();     //List of slabs within the cache.
    newCache->freeSlab = NULL;    //Free slabs of the cache.
    newCache->buffhash = NULL;    //Hash buffer for the cache.
    newCache->offSet = -1;    //Offest for calculating offset of buffers.
    //Binary for knowing if the size of the objects being stored is >1/8 of a page.
    newCache->large = (size>(PAGE_SIZE/8));
    return newCache;
}

/*kmem_cache_alloc takes in a parameter cp and allocates into the given cache.
 *If the cache has a freeslab, the created "temp_slab" is given to
 *the freeslab list. Otherwise, the created "temp_slab" is slotted with the
 *number of maximum objects held by cp and then added to cp. Eventually,
 *the slab's freelist is returned.
 */
void *kmem_cache_alloc(struct kmem_cache *cp){
    struct kmem_slab* temp_slab;    //Temporary slab for usage later.
    if(cp->freeSlab!=NULL) temp_slab = cp->freeSlab->value;   //Add to freelist.
    //If free slabs do not exist or if the slab is now fully grown:
    if(cp->freeSlab==NULL || temp_slab->allocated==temp_slab->maxObjs){
        kmem_cache_grow(cp);  //Expand the cache.
        return kmem_cache_alloc(cp);
    }
    else{   //Otherwise:
        temp_slab->allocated++;   //Incremet the allocation counter,
        return ll_remove_first(temp_slab->freelist);//return freelist of the new slab.
    }
}

/*kmem_cache_free takes in two parameters: cp and buf. This function check if
 *the the size is creater than 1/8 of a page and utilizes the buf parameter
 *and uthash.h in order to find the correct slab. Otherwise, if the object
 *size is small, it iterates through the slab list and finds the correct slab.
 *It then decrements from its allocation counter, and adds that object address
 *to the freelist. If the # of allocated objects is zero, remove that cache.
 */
void kmem_cache_free(struct kmem_cache *cp, void *buf){
    struct kmem_slab* returned_slab; //Temporary variable to be returned.
    //If the object size greater then 1/8 of a page:
    if(cp->large){
        //Use bufctl and uthash to find the correct slab.
        struct kmem_bufctl* ret_ctl;
        int bufadr = buf;
        HASH_FIND_INT(cp->buffhash, &bufadr, ret_ctl);
        returned_slab = ret_ctl->slab; //the slab being returned is found by bufctl.
    }else{ //Otherwise, iterate through the list of slabs and find the correct slab.
        struct Node* move = cp->slabList->head;
        while(((int) move->value- (int) buf)>4196 || ((int) move->value-(int)buf)<0) move = move->next;
        returned_slab = move->value; //the slab being returned is found by the iterating through the list.
    }
    returned_slab->allocated--; //the returned slab's allocation counter is decremented.
    ll_add(returned_slab->freelist, buf); //then added to the freelist.
    if(returned_slab->allocated==0){ kmem_cache_reap(cp); } //and if the slab no longer has anything allocated, remove it.
}

/*kmem_cache_destroy takes in a parameter cp and checks if it has any slabs.
 *If not, free the cache.
 */
void kmem_cache_destroy(struct kmem_cache *cp){
    if(cp->slabList == 0){
        free(cp);
    }
}
/*kmem_cache_grow takes in a parameter cp and grows the cache. If the size of
 *each object is greater than 1/8 of a page, bufctl and uthas is used to
 *correctly determine the addresses of the next area. The new slab is added
 *to the freelist and slablist. Otherwise, it calculates the maximum number of
 *objects in the slab, creates a page for those objects and populates it with
 *slots for those objects. This slab is added to slablist. The offset of the
 *address of the new slab is found by using modulo of page sizes.
 */
void kmem_cache_grow(struct kmem_cache *cp){
    struct kmem_slab newSlab;
    struct kmem_slab* newFreeSlab;
    newSlab.allocated = 0;
    newSlab.freelist = ll_create();
    if(cp->large){
        newSlab.maxObjs = 1;
        if(slab_structs==NULL || ctl_structs==NULL) kmem_init();
        struct kmem_slab* slab_mem = kmem_cache_alloc(slab_structs);
        memcpy(slab_mem, &newSlab, sizeof(struct kmem_slab));

        int mem_alloc = PAGE_SIZE;
        while(mem_alloc<cp->objSize) mem_alloc+=PAGE_SIZE;

        void* buf = memalign(PAGE_SIZE, mem_alloc);
        struct kmem_bufctl* ctl = kmem_cache_alloc(ctl_structs);
        ctl->slab = slab_mem;
        ctl->buffer = buf;
        HASH_ADD_INT(cp->buffhash, buffer, ctl);
        ll_add(slab_mem->freelist, buf);
        ll_add(cp->slabList, slab_mem);
        newFreeSlab = slab_mem;

    }else{
        newSlab.maxObjs = (PAGE_SIZE-sizeof(struct kmem_slab))/cp->objSize;
        void* page = memalign(PAGE_SIZE, PAGE_SIZE);
        int i;
        for(i = 0; i<(PAGE_SIZE-sizeof(struct kmem_slab)); i+=cp->objSize){
            ll_add(newSlab.freelist, page+i);
        }
        memcpy(page+PAGE_SIZE-sizeof(struct kmem_slab), &newSlab, sizeof(struct kmem_slab));
        ll_add(cp->slabList, page+PAGE_SIZE-sizeof(struct kmem_slab));
        newFreeSlab = page+PAGE_SIZE-sizeof(struct kmem_slab);
    }
    struct Node* move = cp->slabList->head;
    while(move->value!=newFreeSlab) move = move->next;
    cp->freeSlab=move;
    cp->offSet = (int) (newFreeSlab) % 4096;
    printf("Expanding: Cache %s; Offset of: %d\n", cp->name, cp->offSet);
}

/*kmem_cache_reap takes in a parameter cp. This cache is reduceed. It achieves
 *this by creating a temporary slab pointed at the head of the slablist and
 *replacing it with another slab. If the length of the slablist is 1, then the
 *freeslab of cp is now null. Otherwise, it points the freeslab to the previous
 *slab. If the size of each object is greater than 1/8 of a page, then uthash
 *and bufctl is used to find the correct addresses. The freelist of the temp.
 *slab is deleted and empty_slab is removed from the list of slabs.
 */
void kmem_cache_reap(struct kmem_cache *cp){
    //Temporary node pointed at the head of the slablist.
    struct Node* empty_slab = cp->slabList->head;
    //Temporary slab.
    struct kmem_slab* current_slab = NULL;
    //Binary to tell if the slab has any objects allocated.
    int allocated_slab_binary = -1;

    //Iterate through slabs until an allocated slab is found.
    while(allocated_slab_binary!=0){
        current_slab = empty_slab->value;
        allocated_slab_binary = current_slab->allocated;
        empty_slab = empty_slab->next;
    }
    //Go back to the last unallocated slab.
    empty_slab = empty_slab->prev;
    //If there is one slab in the cache, there are no freeslabs.
    if(ll_length(cp->slabList)==1){ cp->freeSlab=NULL; }
    //Otherwise, set the last unallocated slab into the freeslab list.
    else if(cp->freeSlab==empty_slab){ cp->freeSlab = empty_slab->prev; }
    //If the objects of the slab is larger than 1/8 of a page,
    if(cp->large){ //Use bufctl and uthash to free the empty slab and destroy the freelist, as no empty slabs remain.
        struct kmem_bufctl* ctl;
        int adr = current_slab->freelist->head->value;
        HASH_FIND_INT(cp->buffhash, &adr, ctl);
        HASH_DEL(cp->buffhash, ctl);
        kmem_cache_free(ctl_structs, ctl);
        kmem_cache_free(slab_structs, empty_slab->value);
        ll_destroy(current_slab->freelist);
    }else{
        //Otherwise, just destroy the freelist.
        ll_destroy(current_slab->freelist);
    }
    //Remove the empty_slab from the slablist.
    ll_remove(cp->slabList, empty_slab);
    printf("Reduction of cache: %s\n", cp->name);
}

//Basic printing function for caches.
void print_cache_info(struct kmem_cache *cp){
    printf("Name of cache: %s\n", cp->name);
    printf("Size of each object: %d\n", (int) cp->objSize);
    if(cp->freeSlab!=NULL){ printf("Address of free slab: %d\n", cp->freeSlab->value); }
    printf("Is the object size > 1/8 of a page: %d\n", cp->large);
    printf("Slab information: \n");
    struct Node* move = cp->slabList->head;
    while(move!=NULL){
        print_slab_info(move->value);
        if(move==cp->slabList->tail){ return; }
        move = move->next;
    }
}

//Basic printing function fo slabs.
void print_slab_info(struct kmem_slab *sp){
    printf("\tAddress: %d\n", sp);
    //printf("Free ");
    //ll_print(sp->freelist);
    printf("\tAllocated # of objects: %d\n", sp->allocated);
    printf("\tPotentail # of objects in slab: %d\n", sp->maxObjs);
}
