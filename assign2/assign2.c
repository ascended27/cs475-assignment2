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

// TODO: addHead helper for fifoQueue
// TODO: addTail helper for fifoQueue
// TODO: ArriveBridge() for not rush hour policy 
// TODO: ArriveBridge() for rush hour policy
// TODO: Cross_Bridge()
// TODO: ExitBridge() for not rush hour policy
// TODO: ExitBridge() for rush hour policy
// TODO: OneVehicle() for rush hour

// IN PROGRESS: queue()

//This is a node for the queue
typedef struct _node
{
    pthread_cond_t* cond;
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
} bridge_t;


//Global Queues
fifoQueue* eastQueue;
fifoQueue* westQueue;
pthread_mutex_t eastQLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t westQLock = PTHREAD_MUTEX_INITIALIZER;

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

// I don't think we will need these since we are using a linked list.
//This function will tell us if the queue is empty.
//1 is empty, 0 is not empty.
int isEmpty();

//Inserts node at tail queue. 1 successful, 0 not successful
int queue(node* newNode);

//Remove a node from the queue. It will return a node from the front of th queue
pthread_cond_t* dequeue();

//Will initialize the queue
void initQueues();

// Return the size of the queue specified by the passed int. The passed int should be 
// Either WEST_DIR or EAST_DIR. Returns the size of the queue or -1 if the passed int
// is not WEST_DIR or EAST_DIR.
int getSize(int);

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

	return;
}

void bridge_destroy()
{
	return;
}

void ArriveBridge(int vid, int direc)
{
	return;
}

void CrossBridge(int vid, int direc, int time_to_cross)
{
	fprintf(stderr, "vid=%d dir=%d starts crossing. Bridge num_car=%d curr_dir=%d\n", 
		vid, direc, br.num_car, br.curr_dir);
	sleep(time_to_cross);
	return;
}

void ExitBridge(int vid, int direc)
{
	fprintf(stderr, "vid=%d dir=%d exit with departure idx=%d\n", 
		vid, direc, br.dept_idx);
	return;
}

//==================== Queue Functions ================================
void initQueues(){
	eastQueue = malloc(sizeof(fifoQueue));
	westQueue = malloc(sizeof(fifoQueue));
}

int enqueue(pthread_cond_t* condition,int direc){
	node* newNode = malloc(sizeof(node));
	newNode -> cond = condition;
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
		if(isEmpty(EAST_DIR) == 1){
			// Not 100% sure if this will end up getting called by the threads. 
			//I don't think it will but here are some locks just in case.
			pthread_mutex_lock(&eastQLock);
			node* node = eastQueue -> head;
			eastQueue -> head = eastQueue -> head->next;
			toReturn = node -> cond;
			free(node);
			eastQueue -> size--;
			pthread_mutex_unlock(&eastQLock);
		}
		return toReturn;		
	} else if(direc == WEST_DIR){
		if(isEmpty(WEST_DIR) == 1){
			pthread_mutex_lock(&westQLock);
			node* node = eastQueue -> head;
			westQueue -> head = westQueue -> head->next;
			toReturn = node -> cond;
			free(node);
			westQueue -> size--;
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
			if(eastQueue -> head != NULL){
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
