#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>






typedef struct customer_info{ /// use this struct to record the customer information read from customers.txt
    int user_id;
	int class_type;
	int service_time;
	int arrival_time;
	struct customer_info* next;
}customer_info;



typedef struct {
	customer_info* head;
	customer_info* tail;

} Queue;

// initialize queue
void cust_queue_init(Queue* queue){
	queue->head = NULL;
	queue->tail = NULL;
}

// implement enqueue function
void enqueue(Queue *queue, customer_info* cust){

    cust->next = NULL;

	if (queue->head == NULL){
		queue->head = cust;
		queue -> tail = cust;
	}else{
		queue->tail->next = cust;
		queue->tail = cust;

	}
    


}









//implement dequeue
customer_info *dequeue(Queue *queue){
	customer_info* cur = queue->head;
	queue->head = queue->head->next;

	return cur;
}

// global value
Queue Equeue;
Queue Bqueue;
Queue Cqueue;


pthread_mutex_t timeMutex;
pthread_mutex_t tryMutex;




pthread_cond_t bService;


double arrived_time=0;

double total_waiting_time = 0;
double bus_wait_time=0;
int bus_cus = 0;
int econ_cus = 0;


double bus_total_time = 0;
double econ_total_time=0;

int eq_length =0;
int bq_length =0;
int cq_length =5;
struct timeval start_time;

double getCurrentSimulationTime(){
	struct timeval cur_time;
	double cur_secs, init_secs;

	//mutx
	pthread_mutex_lock(&timeMutex);
	init_secs = (start_time.tv_sec + (double) start_time.tv_usec / 1000000);
	// printf("%f\n", init_secs);
	//mutx
	pthread_mutex_unlock(&timeMutex);

	gettimeofday(&cur_time, NULL);
	cur_secs = (cur_time.tv_sec+(double) cur_time.tv_usec / 1000000);

	return cur_secs-init_secs;

}




int read_file(int cusNum, FILE* file, customer_info* s){
	// this helper function takes file name as an argument, and handle the content then return the needed content in required format
	char content;
    char buf[20];
    int id, classType, arriveTime, serviceTime;

	// for loop to give related infomation to each customer
    for(int i =0; i< cusNum; i++){
        fgets(buf, 20, file);
        sscanf(buf,"%d:%d,%d,%d", &id, &classType, &arriveTime, &serviceTime);
        if(arriveTime <= 0){
            fprintf(stderr,"invalid infomation, removed \n");
			fclose(file);
            exit(1);
        }else{
			s[i].user_id = id;
			s[i].class_type = classType;
			s[i].arrival_time = arriveTime;
			s[i].service_time = serviceTime;

			// compute  each queue length
			if(s[i].class_type ==1){
				bus_cus++;
			}else{
				econ_cus++;
			}
		}
        



        // printf("%d",s[0].user_id);
    }

}


// create customer threads
void * customer_entry(void * cus_info){
	struct customer_info * p_myInfo = (customer_info *) cus_info;

	


	// sleep to match arrive time 
	usleep(p_myInfo->arrival_time*100000);
	printf("A customer arrives: customer ID %2d. \n", p_myInfo->user_id);
	
	// lock to prevent concurrent operation
	pthread_mutex_lock(&tryMutex);
	if (p_myInfo->class_type == 0){
		//econ class
		// pthread_mutex_lock(&econMutex);
		arrived_time = getCurrentSimulationTime();
		
		enqueue(&Equeue, p_myInfo);
		eq_length++;
		printf("A customer enters a queue: the queue ID %1d, and length of the queue %2d. \n",p_myInfo->user_id,  eq_length);
		
	}else if (p_myInfo->class_type == 1){
		//business class
		//bus_arrived_time = getCurrentSimulationTime();
		// pthread_mutex_lock(&busMutex);
		arrived_time = getCurrentSimulationTime();
		enqueue(&Bqueue, p_myInfo);
		bq_length++;
		printf("A customer enters a queue: the queue ID %1d, and length of the queue %2d. \n",p_myInfo->user_id,  bq_length);
		
	}
	// pthread_mutex_unlock(&tryMutex);

	struct customer_info * c_myInfo;
	

	//lock when clerk all busy
	while (cq_length == 0){
		if (p_myInfo->class_type == 1){
			pthread_cond_wait(&bService, &tryMutex);
		}else if (p_myInfo->class_type == 0){
			pthread_cond_wait(&bService, &tryMutex);
		}
	}

	// service bus class customers
	if(bq_length != 0){
		bq_length--;
		dequeue(&Bqueue);
		cq_length--;
		c_myInfo = dequeue(&Cqueue);

	}else if(eq_length != 0){
		// pthread_cond_wait(&eService,&busMutex);
		eq_length--;
		dequeue(&Equeue);
		cq_length--;
		c_myInfo = dequeue(&Cqueue);
	}

	

	pthread_mutex_unlock(&tryMutex);


	
	double start_serve_time = getCurrentSimulationTime();

	//calculate wating time for each class and total
	total_waiting_time += start_serve_time - arrived_time;
    if (p_myInfo->class_type == 1){
        bus_total_time += start_serve_time - arrived_time;
    } else {
        econ_total_time += start_serve_time - arrived_time;
    }

	// printf("total_waiting_time: %f\n", total_waiting_time);
	


	printf("A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d. \n", start_serve_time, p_myInfo->user_id, c_myInfo->user_id);

	//sleep to simulate start time
	usleep(p_myInfo->service_time*100000);

	// simulate finish time
	double fin_time = getCurrentSimulationTime();
	printf("A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d. \n", fin_time, p_myInfo->user_id,c_myInfo->user_id);
	
	enqueue(&Cqueue, c_myInfo);
	cq_length++;
	// pthread_cond_signal(&bService);

	pthread_cond_signal(&bService);

	
	pthread_exit(0);

	return NULL;
}


int main(int argc, char* argv[]) {
    char* fileName;
	int cusNum;
	char customerNum[5];
	FILE* file;
	char buf[20];


    // if lacking inputs arguments
	if(argc != 2){
		printf("Invalid input");
		exit(0);
	}
    // file name is the second argu
	fileName = argv[1];
	file = fopen(fileName,"r");
	if (file == NULL){
        //test if fail to open file
		printf("Open file failed");
	}
	// get the first line of the file: how many customers are there in total
    fgets(buf, 20, file);
    char* token = strtok(buf,"\n");
    strcpy(customerNum, token);
    cusNum = atoi(customerNum);

	pthread_t customId[cusNum]; //initialize cust and clerk thread setting
	pthread_t clerkId[5];

	// create struct for each customers in a list of struct
    struct customer_info s[cusNum];
	//create struct for each clerk;
	struct customer_info clerk[5];

	read_file(cusNum,file,s);
	fclose(file);



	//initialize queue for two class
	cust_queue_init(&Equeue);
	cust_queue_init(&Bqueue);
	cust_queue_init(&Cqueue);


	// }
	//initialize clerk_info. Total 5 clerk

	for(int i =1; i<=5; i++){
		clerk[i].user_id = i;
		clerk[i].class_type = 0;
		clerk[i].arrival_time = 0;
		clerk[i].service_time = 0;
		enqueue(&Cqueue, &clerk[i]);
	}
	


	//create customer thread
	gettimeofday(&start_time, NULL);
	for(int i = 0; i < cusNum; i++){ // number of customers
		pthread_create(&customId[i], NULL, customer_entry, (void*) &s[i]); //custom_info: passing the customer information (e.g., customer ID, arrival time, service time, etc.) to customer thread
	}
	// wait for all customer threads to terminate

	for(int i = 0; i < cusNum; i++){ // number of customers
		pthread_join(customId[i],NULL); //custom_info: passing the customer information (e.g., customer ID, arrival time, service time, etc.) to customer thread
	}
	
	//print out puts
	double average_waiting_all = total_waiting_time / cusNum;
	printf("The average waiting time for all customers in the system is: %.2f seconds. \n",average_waiting_all);


	double bus_average = bus_total_time / bus_cus;
	printf("The average waiting time for all business-class customers is: %.2f seconds. \n", bus_average);


	double econ_average = econ_total_time / econ_cus;
	printf("The average waiting time for all economy-class customers is: %.2f seconds. \n",econ_average);




    return 0;
}

// function entry for customer threads

