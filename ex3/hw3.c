#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>


//		*** intlist_node define and functions ***

typedef struct intlist_node{
	int value;
	struct intlist_node *next;
	struct intlist_node *previous;

}intlist_node;

intlist_node* createIntNode(int val){

	intlist_node * new_node = (intlist_node *) malloc(sizeof(intlist_node));

	if(!new_node){
		printf("Error in intlist node allocation: %s\n", strerror(errno));
		exit(errno);
	}

	new_node->value = val;
	new_node->next = NULL;
	new_node->previous = NULL;

	return new_node;
}

void destroyIntNode(intlist_node *node){

	if(!node) return;

	node->next = NULL;
	node->previous = NULL;

	free(node);
	node = NULL;

}

//		*** intlist define and functions ***

typedef struct intlist{

	intlist_node 	*head;
	intlist_node 	*tail;
	int 			size;
	pthread_mutex_t lock;
	pthread_cond_t	notEmpty;

} intlist;


void intlist_init(intlist* list){

	int rc;
	pthread_mutexattr_t attr;

	list->head = NULL;
	list->tail = NULL;
	list->size = 0;


	//mutex attributes initialize
	rc = pthread_mutexattr_init(&attr);
	if(rc){
		printf("ERROR in pthread_mutexattr_init(): %s\n", strerror(rc));
		exit(rc);
	}

	rc = pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
	if(rc){
		printf("ERROR in pthread_mutexattr_settype(): %s\n", strerror(rc));
		exit(rc);
	}

	//lock initialize
	rc = pthread_mutex_init(&list->lock, &attr);
	if(rc){
		printf("ERROR in pthread_mutex_init(): %s\n", strerror(rc));
		exit(rc);
	}

	//condition variable initialize
	rc = pthread_cond_init(&list->notEmpty, NULL);
	if(rc){
		printf("ERROR in pthread_cond_init(): %s\n", strerror(rc));
		exit(rc);
	}

	//once the mutex lock created, we can destroy the mutexattr
	rc = pthread_mutexattr_destroy(&attr);
	if(rc){
		printf("ERROR in pthread_mutexattr_destroy(): %s\n", strerror(rc));
		exit(rc);
	}

}


void intlist_remove_last_k(intlist* list, int k){

	int rc, i = 0, k_to_delete = k;
	intlist_node *curr;

	if(k <= 0) return;

	rc = pthread_mutex_lock(&list->lock);
	if(rc){
		printf("ERROR in pthread_mutex_lock(): %s\n", strerror(rc));
		exit(rc);
	}

	if(list->size == 0) return;

	if(k >= list->size){		//delete all list nodes
		k_to_delete = list->size;
		curr = list->head;
		list->head = NULL;
	}

	else{
		curr = list -> tail;
		while(i<k-1 && curr != NULL){
			curr = curr->previous;
			i++;
		}
		curr->previous->next = NULL;


	}
	list->size -= k_to_delete;
	list->tail = curr->previous; //NULL if k>= list->size

	rc = pthread_mutex_unlock(&list->lock);
	if (rc) {
		printf("ERROR in pthread_mutex_unlock(): %s\n", strerror(rc));
		exit(rc);
	}


	//we first lock to disconnect the sub-list we want to delete from the original list, then unlock and delete/free the sublist

	//now delete the disconnected sub-list we want to delete

	while(curr->next){
		curr = curr->next;
		destroyIntNode(curr->previous);
	}
	destroyIntNode(curr);

}



void intlist_destroy(intlist* list){
	int rc;

	if(!list) return;
	intlist_remove_last_k(list, list->size);

	rc = pthread_mutex_destroy(&list->lock);
	if(rc){
		printf("ERROR in pthread_mutex_destroy(): %s\n", strerror(rc));
		exit(rc);
	}

	rc = pthread_cond_destroy(&list->notEmpty);
	if(rc){
		printf("ERROR in pthread_cond_destroy(): %s\n", strerror(rc));
		exit(rc);
	}

}



pthread_mutex_t* intlist_get_mutex(intlist* list){

	return &list->lock;

}

void intlist_push_head(intlist* list, int value){

	intlist_node *newNode = createIntNode(value);
	int rc;

	rc = pthread_mutex_lock(&list->lock);
	if (rc) {
		printf("ERROR in pthread_mutex_lock(): %s\n", strerror(rc));
		exit(rc);
	}

	newNode->next = list->head;
	if(list->head) newNode->next->previous = newNode;
	else list->tail = newNode; //the list was empty
	list->head = newNode;
	list->size++;

	rc = pthread_cond_signal(&list->notEmpty);
	if(rc){
		printf("ERROR in pthread_cond_signal(): %s\n", strerror(rc));
		exit(rc);
	}

	rc = pthread_mutex_unlock(&list->lock);
	if (rc) {
		printf("ERROR in pthread_mutex_unlock(): %s\n", strerror(rc));
		exit(rc);
	}

}

int intlist_pop_tail(intlist* list){
	int rc, val;
	intlist_node *tempNode;

	rc = pthread_mutex_lock(&list->lock);
	if (rc) {
		printf("ERROR in pthread_mutex_lock(): %s\n", strerror(rc));
		exit(rc);
	}

	while(list->size == 0){		//wait until the list is not empty

		rc = pthread_cond_wait(&list->notEmpty, &list->lock);
		if(rc){
			printf("ERROR in pthread_cond_wait(): %s\n", strerror(rc));
			exit(rc);
		}
	}
	tempNode = list->tail;

	if(list->tail->previous){		//list->size > 1
		list->tail = list->tail->previous;
		list->tail->next = NULL;

	}
	else{
		list->tail = NULL;
		list->head = NULL;
	}

	list->size--;

	rc = pthread_mutex_unlock(&list->lock);
	if(rc){
		printf("ERROR in pthread_mutex_unlock(): %s\n", strerror(rc));
		exit(rc);
	}

	val = tempNode->value;
	destroyIntNode(tempNode);

	return val;

}


int intlist_size(intlist* list){

	return list->size;

}

// ********************************************************************************************



//		***	global variables for simulator ***

intlist *list;
bool readers_running = true, writers_running = true, gc_running = true;
int MAX, size;
pthread_cond_t gc_cond;		//condition variable of garbage collector


//		*** functions for the simulator	***

void *writers_func(void *a){
	int rc;

	while(writers_running){

		// First, check the garbage collector condition
		rc = pthread_mutex_lock(&list->lock);
		if (rc) {
			printf("ERROR in pthread_mutex_lock(): %s\n", strerror(rc));
			exit(rc);
		}
		if(intlist_size(list) > MAX){
			rc = pthread_cond_signal(&gc_cond);
			if(rc){
				printf("ERROR in pthread_cond_signal() for the garbage collector: %s\n", strerror(rc));
				exit(rc);
			}
		}
		rc = pthread_mutex_unlock(&list->lock);
		if(rc){
			printf("ERROR in pthread_mutex_unlock(): %s\n", strerror(rc));
			exit(rc);
		}

		// Pushing random integer to the list
		intlist_push_head(list, rand());
	}

	//writers_running flag is off, so exit from this specific writer thread
	pthread_exit(NULL);
}

void *readers_func(void *a){

	while(readers_running){
		intlist_pop_tail(list);
	}

	//readers_running flag is off, so exit from this specific reader thread
	pthread_exit(NULL);
}


void *gc_func(void *a){
	int rc, median, size, deleted_cnt=0;

	while(gc_running){

		rc = pthread_mutex_lock(intlist_get_mutex(list));
		if(rc){
			printf("ERROR in pthrsead_mutex_lock() in the garbage collector thread: %s\n", strerror(rc));
			exit(rc);
		}

		while(intlist_size(list) <= MAX){
			rc = pthread_cond_wait(&gc_cond, intlist_get_mutex(list));

		if(rc){
			printf("ERROR in pthread_cond_wait() in the garbage collector thread: %s\n", strerror(rc));
			exit(rc);
		}

		// Now gc_running can be false, then we want to unlock and exit
		if(!gc_running){
			rc = pthread_mutex_unlock(&list->lock);
			if (rc) {
				printf("ERROR in pthread_mutex_unlock(): %s\n", strerror(rc));
				exit(rc);
			}

			pthread_exit(NULL);
		}
		}

		// Else - garbage collector is still working
		// removes half of the elements in the list (from the tail, rounded up) - until list size <= MAX

		while((size = intlist_size(list)) > MAX){	//I could check here the gc_running flag, but I want that the gc will finish the job although time is out

			median = size/2 + size%2;
			intlist_remove_last_k(list, median);
			deleted_cnt += median;

		}

		rc = pthread_mutex_unlock(&list->lock);
		if(rc){
			printf("ERROR in pthread_mutex_unlock(): %s\n", strerror(rc));
			exit(rc);
		}

		// Prints the number of items removed from the list
		fflush(NULL);
		printf("GC - %d items removed from the list\n", deleted_cnt);

		deleted_cnt = 0;

	}
	pthread_exit(NULL);

}

int main (int argc, char *argv[]){

	int WNUM, RNUM, TIME;
	int rc, i;
	char *string_end;
	pthread_t gc_thread;
	void *status;

	
	if(argc != 5){
		printf("Error - wrong number of arguments\n");
		exit(-1);
	}

	if((WNUM = (int)strtol(argv[1], &string_end, 10)) <= 0 || *string_end){
		printf("Error - WNUM has to be positive integer\n");
		exit(-1);
	}

	if((RNUM = (int)strtol(argv[2], &string_end, 10)) <= 0 || *string_end){
		printf("Error - RNUM has to be positive integer\n");
		exit(-1);
	}

	if((MAX = (int)strtol(argv[3], &string_end, 10)) <= 0 || *string_end){
		printf("Error - MAX has to be positive integer\n");
		exit(-1);
	}

	if((TIME = (int)strtol(argv[4], &string_end, 10)) <= 0 || *string_end){
		printf("Error - TIME has to be positive integer\n");
		exit(-1);
	}

	pthread_t writers_threads[WNUM], readers_threads[RNUM];

	// Initialization a global doubly-linked list of integers
	list = (intlist*) malloc(sizeof(intlist));
	if(!list){
		printf("ERROR in list malloc(): %s\n", strerror(errno));
		exit(errno);
	}

	intlist_init(list);


	// Creating a condition variable for the garbage collector
	rc = pthread_cond_init(&gc_cond, NULL);
	if(rc){
		printf("ERROR in pthread_cond_init(): %s\n", strerror(rc));
		exit(rc);
	}

	// Creating a thread for the garbage collector
	rc = pthread_create(&gc_thread, NULL, gc_func, NULL);
	if(rc){
		printf("ERROR in pthread_create() of the garbage collector: %s\n", strerror(rc));
		exit(rc);
	}

	// Creating WNUM threads for the writers
	for (i = 0; i < WNUM; i++) {

		rc = pthread_create(&writers_threads[i], NULL, writers_func, NULL);
		if(rc){
			printf("ERROR in pthread_create() for some writer thread: %s\n", strerror(rc));
			exit(rc);
		}
	}


	// Creating RNUM threads for the readers
	for (i = 0; i < RNUM; i++) {
		rc = pthread_create(&readers_threads[i], NULL, readers_func, NULL);
		if(rc){
			printf("ERROR in pthread_create() for some reader thread: %s\n", strerror(rc));
			exit(rc);
		}
	}

	// Sleeping for TIME seconds
	sleep(TIME);

	//	*** Stopping all running threads, avoiding deadlocks - readers and writers threads, and then also the gc thread	***

	// stop all readers running threads
	readers_running = false;

	for (i = 0; i < RNUM; i++) {
		rc = pthread_join(readers_threads[i], &status);
		if(rc){
			printf("ERROR in pthread_join() for some reader thread: %s\n", strerror(rc));
			exit(rc);
		}
	}

	// Stop all writers threads
	writers_running = false;
	for (i = 0; i < WNUM; i++) {
		rc = pthread_join(writers_threads[i], &status);
		if(rc){
			printf("ERROR in pthread_join() for some writer thread: %s\n", strerror(rc));
			exit(rc);
		}
	}

	// Stop the garbage collector thread
	gc_running = false;

	// sending a signal to wake up the gc from waiting
	rc = pthread_cond_signal(&gc_cond);
	if(rc){
		printf("ERROR in pthread_cond_signal(): %s\n", strerror(rc));
		exit(rc);
	}

	// Joining the garbage collector thread
	rc = pthread_join(gc_thread, &status);
	if(rc){
		printf("ERROR in pthread_join() for the garbage collector thread: %s\n", strerror(rc));
		exit(rc);
	}

	// Printing the size of the list as well as all items within it
	size = intlist_size(list);
	fflush(NULL);
	printf("list size is: %d\n", size);

	if(size > 0){
		for (i = 0; i < size-1; i++) {
			fflush(NULL);
			printf("%d, ", intlist_pop_tail(list));
		}
		fflush(NULL);
		printf("%d\n", intlist_pop_tail(list));
	}

	// Cleanup and Exit gracefully
	intlist_destroy(list);
	free(list);
	list = NULL;

	rc = pthread_cond_destroy(&gc_cond);
	if(rc){
		printf("ERROR in pthread_cond_destroy() for the garbage collector condition variable: %s\n", strerror(rc));
		exit(rc);
	}

	pthread_exit(NULL);

}
