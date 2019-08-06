#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "util.h"
#include "queue.h"
#include "multi-lookup.h"
#include <syscall.h>
#include <stdbool.h>
#define __GNU_SOURCE

queue Share_array;
bool wait_for_requesters = true; 

FILE* outputfp = NULL;
char* outputFile;

FILE* logfp = NULL;
char* logFile; 


pthread_mutex_t ArrayAccess;
pthread_mutex_t Output_file_Access;
pthread_mutex_t log_file_Access;
pthread_cond_t Full; 
pthread_cond_t Empty; 

void *requester(void* inputFile){
	
    char *Name_file = (char*)inputFile;
    FILE* inputfp = NULL;
    inputfp = fopen(Name_file, "r");
	char domain_name[MAX_NAME_LENGTH];
	
    while(fscanf(inputfp, LENGTH_SCAN, domain_name) != EOF){ 
        char* dup_str_pointer = strdup(domain_name);
        
        pthread_mutex_lock(&ArrayAccess); // Get the lock of the share array or try
        
        if(queue_is_full(&Share_array)){  // If the share array is now full, requester threads shouldn't push an element to the share array 
			
           pthread_cond_wait(&Full, &ArrayAccess); // This function blocks until the condition is signaled. It atomically releases the associated mutex lock before blocking, and atomically acquires it again before returning.
        }  
        
        queue_push(&Share_array, (void*) dup_str_pointer); // Adding domain name to the share array 
        
        pthread_cond_signal(&Empty); // Sent signal to the resolver threads that the share array isn't empty now
        
		pthread_mutex_unlock(&ArrayAccess); // unlock the lock for share array so other requester threads can push the element into the share array
		
		memset(domain_name, 0, sizeof(domain_name));
    }
    
    /*writing to log file*/
    pthread_mutex_lock(&log_file_Access);
    char ID[10]; 
    sprintf(ID,"%lu", syscall(SYS_gettid));
	//fprintf(logfp,"%s        %ld\n", Name_file,syscall(SYS_gettid));
	pthread_mutex_unlock(&log_file_Access);
	char* name_with_extension;
	name_with_extension = malloc(strlen(Name_file)+strlen(ID)+10);
	strcpy(name_with_extension, Name_file); 
	strcat(name_with_extension, "	");
	strcat(name_with_extension, ID); 
	strcat(name_with_extension, "\n"); 
	fputs_unlocked(name_with_extension, logfp);
	free(name_with_extension);
    fclose(inputfp);
    pthread_exit(0);
};

void *resolver(){
      char IP_address[MAX_IP_LENGTH] = ""; 
      char *domain_name = NULL;

      while(!queue_is_empty(&Share_array) || wait_for_requesters ){ // The resolver should not exit because of the empty share array while none of requester thread hasn't push an element to the share array yet.

			pthread_mutex_lock(&ArrayAccess);
          
            if(queue_is_empty(&Share_array)){ 
                pthread_cond_wait(&Empty, &ArrayAccess); // If the share array is now empty, wait until requester threads to push element into the share array 
            }

            domain_name = queue_pop(&Share_array);
            pthread_cond_signal(&Full); 
			pthread_mutex_unlock(&ArrayAccess);
			
			pthread_mutex_lock(&Output_file_Access); // Get the lock of the output file or try
			
            /* Lookup domain_name and get IP string */
            if(dnslookup(domain_name, IP_address, sizeof(IP_address)) == UTIL_FAILURE){
                fprintf(stderr, "Can't reslove this domain name: %s\n", domain_name);
                memset(IP_address, 0, sizeof(IP_address));
            }

            /* Writing to Output file */
            
            char* name_with_extension;
			name_with_extension = malloc(strlen(domain_name)+strlen(IP_address)+3);
			strcpy(name_with_extension, domain_name); 
			strcat(name_with_extension, ",");
			strcat(name_with_extension, IP_address); 
			strcat(name_with_extension, "\n"); 
            //fprintf(outputfp, "%s,%s\n", domain_name, IP_address);
            fputs_unlocked(name_with_extension, outputfp);
            free(domain_name);
            free(name_with_extension);
			pthread_mutex_unlock(&Output_file_Access);
			
      }
    pthread_exit(0);
};

int main(int argc, char* argv[]){

    if(argc < MINARGS){
      fprintf(stderr, "Not enough arguments: %d entered, at least %d needed\n", (argc - 1), (MINARGS-1));
      return EXIT_FAILURE;
    };

/* Open Output file */

    outputFile = argv[4];
    outputfp = fopen(outputFile, "w");
    
    if(!outputfp){
      fprintf(stderr, "Error Opening File");
      return EXIT_FAILURE;
    }
    
/* Open Log file */
    logFile = argv[3];
    logfp = fopen(logFile, "w");
    
    if(!logfp){
      fprintf(stderr, "Error Opening File");
      return EXIT_FAILURE;
    }

/*  Initializing Share array  */
    queue_init(&Share_array, QUEUEMAXSIZE);
    
/*  Initializing Mutexes  */     
    pthread_mutex_init(&ArrayAccess, NULL);
    pthread_mutex_init(&Output_file_Access, NULL);
    pthread_mutex_init(&log_file_Access, NULL);
    
/*  Initializing Condition variables*/ 
    pthread_cond_init(&Full, NULL);
    pthread_cond_init(&Empty, NULL);


/* Creating Threads */

	int num_of_requester = atoi(argv[1]);
	int num_of_resolver = atoi(argv[2]);
	
	/* Creating requester Thread pool*/
    pthread_t requesterThreads[num_of_requester];	
    
    /* Creating resolver Thread pool */
    pthread_t resolverThreads[num_of_resolver];	

    /* Allocating job to the requesterThreads */
    for (int i = 0; i < num_of_requester; i++){
		
        if(pthread_create(&requesterThreads[i], NULL, requester, (void*) argv[i+5])){
          fprintf(stderr, "Error creating requester thread %d\n", i);  // If successful, the pthread_create() function shall return zero; otherwise, sent error
          exit(EXIT_FAILURE);
        }
    }

    /* Allocating job to the resloverThreads */
    for (int j = 0; j < num_of_resolver; j++){
  
        if(pthread_create(&resolverThreads[j], NULL, resolver, NULL)){  
          fprintf(stderr, "Error creating reslover thread %d\n", j); 
          exit(EXIT_FAILURE);
        }
    }

/* Waiting for Threads */

    /* requester threads */
    for(int p = 0; p < num_of_requester; p++){

      if(pthread_join(requesterThreads[p], NULL)){ 
        fprintf(stderr, "Error Waiting on reqeuster thread %d\n", p); // If successful, the pthread_join() function shall return zero; otherwise, sent error
        exit(EXIT_FAILURE);
      }
    }

	wait_for_requesters = false;

    /* resolver threads */
    for(int q = 0; q < num_of_resolver; q++){
		
      if(pthread_join(resolverThreads[q], NULL)){
        fprintf(stderr, "Error Waiting on resolver thread %d\n", q);
        exit(EXIT_FAILURE);
      }
    }

/* Dealloc memory */
    fclose(outputfp);
    fclose(logfp);
    queue_cleanup(&Share_array);
    pthread_mutex_destroy(&Output_file_Access);
    pthread_mutex_destroy(&log_file_Access);
    pthread_mutex_destroy(&ArrayAccess);
    pthread_cond_destroy(&Full);
    pthread_cond_destroy(&Empty);

    exit(EXIT_SUCCESS);
};
