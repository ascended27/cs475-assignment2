#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_BR_CAP 5
#define CROSS_TIME 4
#define DIREC_PROB 0.7
#define EAST_DIR 0
#define WEST_DIR 1

#define handle_err(s) do{perror(s); exit(EXIT_FAILURE);}while(0)

// TODO: ArriveBridge() for not rush hour policy 
// TODO: ArriveBridge() for rush hour policy
// TODO: Cross_Bridge()
// TODO: ExitBridge() for not rush hour policy
// TODO: ExitBridge() for rush hour policy
// TODO: OneVehicle() for rush hour

//This is a node for the queue
typedef struct _node
{
    pthread_cond_t* cond;
    int vid;
    struct _node* next;
    struct _node* prev;
} node;

typedef struct _thread_argv
{
	int vid;
	int direc;
	int time_to_cross;
} thread_argv;

typedef struct _fifoQueue
{
	int direc;
	int size;

	node* head;
	node* tail;
} fifoQueue;

/**
 * Student may add necessary variables to the struct
 **/
typedef struct _bridge {
	int dept_idx;
	int num_car;
	int curr_dir;
	int max_car;
} bridge_t;


//Global Queues
fifoQueue* eastQueue;
fifoQueue* westQueue;
pthread_mutex_t eastQLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t westQLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t carMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t arriveMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t crossMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t exitMutex = PTHREAD_MUTEX_INITIALIZER;


void bridge_init();
void bridge_destroy();
void dispatch(int n);
void *OneVehicle(void *argv);
void ArriveBridge(int vid, int direc);
void CrossBridge(int vid, int direc, int time_to_cross);
void ExitBridge(int vid, int direc);

// I don't think we will need these since we are using a linked list.
//This function will insert a new thread to the fifoQueue
// 1 is true, 0 is false.
int isFull();

//This function will tell us if the queue is empty.
//1 is empty, 0 is not empty.
int isEmpty(int);

//Inserts node at tail queue. 1 successful, 0 not successful
int enqueue(pthread_cond_t*,int,int);

//Remove a node from the queue. It will return a node from the front of th queue
pthread_cond_t* dequeue(int);

//Will initialize the queue
void initQueues();

// Will clean up the queues
void destroyQueues();

// Return the size of the queue specified by the passed int. The passed int should be 
// Either WEST_DIR or EAST_DIR. Returns the size of the queue or -1 if the passed int
// is not WEST_DIR or EAST_DIR.
int getSize(int);

//Encapsulates logic for checking whether car should wait to cross
int testEastStateWait(int vid);
int testWestStateWait(int vid);

//Encapsulates test of state to see if a car can be unlocked on its cond var.
int testEastStateGo(int vid, int direc);
int testWestStateGo(int vid, int direc);

pthread_cond_t** condVars = NULL;/* Array to hold cond variables */
pthread_t *threads = NULL;	/* Array to hold thread structs */
thread_argv *args = NULL;	/* Array to hold thread arguments */
int num_v = 30;			/* Total number of vehicles to be created */

bridge_t br;			/* Bridge struct shared by the vehicle threads*/

int main(int argc, char *argv[])
{
	int sched_opt;
	int i;

	if(argc < 2)
	{
		printf("Usage: %s SCHED_OPT [SEED]\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	/* Process Arguments */
	sched_opt = atoi(argv[1]);
	if(argc == 3)
		srand((unsigned int)atoi(argv[2]));
	else
		srand((unsigned int)time(NULL));

	/* Allocate memory for thread structs and arguments */
	if((threads = (pthread_t *)malloc(sizeof(pthread_t) * num_v)) == NULL)
		handle_err("malloc() Failed for threads");
	if((args = (thread_argv *)malloc(sizeof(thread_argv) * num_v)) == NULL)
		handle_err("malloc() Failed for args");

	/* Init bridge struct */
	bridge_init();

	initQueues();
	condVars = malloc(sizeof(pthread_cond_t)*num_v);

	/* Create vehicle threads */
	switch(sched_opt)
	{
		case 1 : dispatch(5); break;
		case 2 : dispatch(10); break;
		case 3 : dispatch(30); break;
		default:
			fprintf(stderr, "Bad Schedule Option %d\n", sched_opt);
			exit(EXIT_FAILURE);
	}
	
	/* Join all the threads */
	for(i = 0; i < num_v; i++)
		pthread_join(threads[i], NULL);

	/* Clean up and exit */
	bridge_destroy();

	destroyQueues();

	exit(EXIT_SUCCESS);
}

/**
 *	Create n vehicle threads for every 10 seconds until the total
 * 	number of vehicles reaches num_v
 *	Each thread handle is stored in the shared array - threads
 */
void dispatch(int n)
{
  int k, i;
  
	for(k = 0; k < num_v; k += n)
	{
		printf("Dispatching %d vehicles\n", n);

		for( i = k; i < k + n && i < num_v; i++)
		{
			/* The probability of direction 0 is DIREC_PROB */
			int direc = rand() % 1000 > DIREC_PROB * 1000 ? 0 : 1;

			args[i] = (thread_argv){i, direc, CROSS_TIME};
			if(pthread_create(threads + i, NULL, &OneVehicle, args + i) != 0)
				handle_err("pthread_create Failed");
		}
		
		printf("Sleep 10 seconds\n"); sleep(10);
	}
}

void *OneVehicle(void *argv)
{
	thread_argv *args = (thread_argv *)argv;	
	ArriveBridge(args->vid, args->direc);
	CrossBridge(args->vid, args->direc, args->time_to_cross);
	ExitBridge(args->vid, args->direc);
	
	pthread_exit(0);
}

/**
 *	Students to complete the following functions
 */

void bridge_init()
{
	br.dept_idx = 0;
	br.curr_dir = 0;
	br.num_car = 0;
	br.max_car = 5;
	return;
}

void bridge_destroy()
{
	return;
}

void ArriveBridge(int vid, int direc)
{
    /*
    Used malloc to place new condition variables on the heap so
    that they aren't lost once this function's stack frame is gone.
    */
	//pthread_mutex_lock(&arriveMutex);
    pthread_cond_t newCond = NULL;
    pthread_cond_init(&newCond,NULL);
    enqueue(&newCond,direc,vid);
	//pthread_mutex_lock(&mutex);
	if(direc == EAST_DIR || (direc == WEST_DIR && westQueue->size == 0)){
		if(eastQueue->head->vid == vid && br.curr_dir == EAST_DIR){
			// We are first in line so dequeue ourself and don't wait
			dequeue(direc);
		}else{
			pthread_mutex_lock(&carMutex);
			while(br.curr_dir != EAST_DIR)
				pthread_cond_wait(&newCond,&carMutex);
			pthread_mutex_unlock(&carMutex);

		}
	} else if(direc == WEST_DIR || (direc == EAST_DIR && eastQueue->size == 0)){
		if(westQueue->head->vid == vid && br.curr_dir == WEST_DIR){
			// We are first in line so dequeue ourself and don't wait
			dequeue(direc);
		}else{
			pthread_mutex_lock(&carMutex);
			while(br.curr_dir != WEST_DIR)
				pthread_cond_wait(&newCond,&carMutex);
			pthread_mutex_unlock(&carMutex);

		}
	}
	//pthread_mutex_unlock(&mutex);
    //pthread_mutex_unlock(&arriveMutex);
}

void CrossBridge(int vid, int direc, int time_to_cross)
{
	// Acquire a lock b/c we are interacting with
	// shared bridge data.
	pthread_mutex_lock(&mutex);
	
	// A new car just got on the bridge so increment
	// the car number
	br.num_car++;
	
	// If there is room for more cars on the bridge then
	// signal another car to go
	if(br.num_car < br.max_car){
		// If the direction is east check the east side first
		if(direc == EAST_DIR){
			// If the east queue isn't empty then signal a car from there
			if(eastQueue->size != 0){
				//pthread_mutex_lock(&carMutex);
				pthread_cond_t * cond = dequeue(direc);
				pthread_cond_broadcast(cond);
				//pthread_mutex_unlock(&carMutex);		
			}
		} else if(direc == WEST_DIR){
		// Otherwise the direction is west check the west side first
			// If the west queue isn't empty then signal a car from there
			if(westQueue->size != 0){
				//pthread_mutex_lock(&carMutex);
				pthread_cond_t * cond = dequeue(direc);
				pthread_cond_broadcast(cond);
				//pthread_mutex_unlock(&carMutex);
			}
		}
		
	}
	
	// Output the crossing message
    fprintf(stderr, "vid=%d dir=%d starts crossing. Bridge num_car=%d curr_dir=%d\n",
        vid, direc, br.num_car, br.curr_dir);
    
	// Release the lock
	pthread_mutex_unlock(&mutex);
	
	// Sleep till the car has crossed the bridge
	sleep(time_to_cross);
	
	
	return;
}

void ExitBridge(int vid, int direc)
{
	// Acquire a lock b/c we are interacting with
	// shared bridge data.
	pthread_mutex_lock(&mutex);
	// This car just left the bridge so decrement the car count
	br.num_car--;

	// If that count is 0 then see if there are any new cars waiting
	if(br.num_car == 0){
		//pthread_mutex_lock(&carMutex);
		// If direc is east then check there first
		if(direc == EAST_DIR){
			// If the east queue isn't empty then signal a car there
			if(eastQueue -> size != 0){
				pthread_cond_t * cond = dequeue(direc);
				pthread_cond_broadcast(cond);
			} else if(westQueue -> size != 0){ 
			// Otherwise check the west queue and signal a car if that isn't empty
			// and flip the bridge direction
				br.curr_dir = WEST_DIR;
				pthread_cond_t * cond = dequeue(WEST_DIR);
				pthread_cond_broadcast(cond);
			}
		} else if(direc == WEST_DIR){ // Otherwise check the west side for cars
			// If the west queue isn't empty then signal a car there
			if(westQueue -> size != 0){
				pthread_cond_t * cond = dequeue(direc);
				pthread_cond_broadcast(cond);
			} else if(eastQueue -> size != 0){
				// Otherwise check the east queue and signal a car if that isn't empty
				// and flip the bridge direction
				br.curr_dir = EAST_DIR;
				pthread_cond_t * cond = dequeue(EAST_DIR);
				pthread_cond_broadcast(cond);
			}
		}
		//pthread_mutex_unlock(&carMutex);
	}
	
	// Output the exit message
	fprintf(stderr, "vid=%d dir=%d exit with departure idx=%d\n", 
		vid, direc, br.dept_idx);
	// Increment the dept_idx of the bridge
	br.dept_idx++;
	
	// Release the lock
	pthread_mutex_unlock(&mutex);
	return;
}

int testEastStateWait(int vid)
{
    return eastQueue->head->vid != vid && br.num_car >= br.max_car && br.curr_dir == EAST_DIR;
}

int testWestStateWait(int vid)
{
    return westQueue->head->vid != vid && br.num_car >= br.max_car && br.curr_dir == WEST_DIR;
}

int testEastStateGo(int vid, int direc)
{
    if(direc == WEST_DIR && br.num_car == 0)
        return 1;

    if(direc == EAST_DIR && br.num_car < 5)
        return 1;

    return 0;
}

int testWestStateGo(int vid, int direc)
{
    if(direc == EAST_DIR && br.num_car == 0)
        return 1;

    if(direc == WEST_DIR && br.num_car < 5)
        return 1;

    return 0;
}

//==================== Queue Functions ================================
void initQueues(){
	eastQueue = malloc(sizeof(fifoQueue));
	eastQueue->size = 0;
	eastQueue->head = NULL;
	eastQueue->tail = NULL;
	eastQueue->direc = EAST_DIR;
	westQueue = malloc(sizeof(fifoQueue));
	westQueue->size = 0;
	westQueue->head = NULL;
	westQueue->tail = NULL;
	westQueue->direc = WEST_DIR;
}

void destroyQueues(){
	node* current;
	node* toDelete;
	current = eastQueue -> head;
	while(current!=NULL){
		toDelete = current;
		current = current -> next;
		free(toDelete);
	}

	current = westQueue -> head;
	while(current!=NULL){
		toDelete = current;
		current = current -> next;
		free(toDelete);
	}
}

int enqueue(pthread_cond_t* condition,int direc,int vid){
	node* newNode = malloc(sizeof(node));
	newNode -> cond = condition;
	newNode -> vid = vid;
	newNode -> next = NULL;
	newNode -> prev = NULL;

	if(direc == EAST_DIR){
		// Not 100% sure if this will end up getting called by the threads. 
		//I don't think it will but here are some locks just in case.
		pthread_mutex_lock(&eastQLock);
		newNode -> prev = eastQueue -> tail;
		if(eastQueue -> size == 0)
			eastQueue -> head = newNode;
		else if(eastQueue -> size > 0)
			eastQueue -> tail -> next = newNode;
		eastQueue -> tail = newNode;
		eastQueue -> size++;
		pthread_mutex_unlock(&eastQLock);

		return 1;
	} else if(direc == WEST_DIR){
		pthread_mutex_lock(&westQLock);
		newNode -> prev = westQueue -> tail;
		if(westQueue -> size == 0)
			westQueue -> head = newNode;
		else if(westQueue -> size > 0)
			westQueue -> tail -> next = newNode;
		westQueue -> tail = newNode;
		westQueue -> size++;
		pthread_mutex_unlock(&westQLock);
		return 1;
	} else{
		return 0;
	}

}

pthread_cond_t* dequeue(int direc){
	pthread_cond_t* toReturn;

	if(direc == EAST_DIR){
		if(isEmpty(EAST_DIR) == 0){
			// Not 100% sure if this will end up getting called by the threads. 
			//I don't think it will but here are some locks just in case.
			pthread_mutex_lock(&eastQLock);
			node* node = eastQueue -> head;
			eastQueue -> head = eastQueue -> head->next;
			toReturn = node -> cond;
			free(node);
			eastQueue -> size--;
			if(eastQueue->size == 0){
				eastQueue->head = NULL;
				eastQueue->tail = NULL;
			}
			pthread_mutex_unlock(&eastQLock);
		}
		return toReturn;		
	} else if(direc == WEST_DIR){
		if(isEmpty(WEST_DIR) == 0){
			pthread_mutex_lock(&westQLock);
			node* node = westQueue -> head;
			westQueue -> head = westQueue -> head->next;
			toReturn = node -> cond;
			free(node);
			westQueue -> size--;
			if(westQueue->size == 0){
				westQueue->head = NULL;
				westQueue->tail = NULL;
			}
			pthread_mutex_unlock(&westQLock);
		}
		return toReturn;
	} else {
		return NULL;
	}

}

int isEmpty(int direc){

	int toReturn = -1;

	if(direc == EAST_DIR){
		pthread_mutex_lock(&eastQLock);
		if(eastQueue != NULL){
			if(eastQueue -> head != NULL){
				toReturn = 0;
			} else{
				toReturn = 1;
			}
		}
		pthread_mutex_unlock(&eastQLock);
	} else if(direc == WEST_DIR){
		pthread_mutex_lock(&westQLock);
		if(westQueue != NULL){
			if(westQueue -> head != NULL){
				toReturn = 0;
			} else{
				toReturn = 1;
			}
		}
		pthread_mutex_unlock(&westQLock);
	}
	return toReturn;

}

int getSize(int direc){
	int toReturn = -1;
	if(direc == EAST_DIR){
		pthread_mutex_lock(&eastQLock);
		toReturn = eastQueue -> size;
		pthread_mutex_unlock(&eastQLock);
	} else if(direc == WEST_DIR){
		pthread_mutex_lock(&westQLock);
		toReturn = westQueue -> size;
		pthread_mutex_lock(&westQLock);
	}
	return toReturn;	


}