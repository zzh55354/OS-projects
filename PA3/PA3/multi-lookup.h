 #ifndef MULTI_LOOKUP_H
 #define MULTI_LOOKUP_H

 #include <stdio.h>

 #define MINARGS 6
 #define MAX_RESOLVER_THREADS 10
 #define MAX_REQUESTER_THREADS 5
 #define MAX_NAME_LENGTH 1025 // Max Size of the the char array is 1025 include '\0' 
 #define LENGTH_SCAN "%1024s" // maximizes the length of the string to be scanned in 1024 characters
 #define MAX_IP_LENGTH INET6_ADDRSTRLEN

 /* Function to read in the domains passed in from a text file
  * And push them to a queue of hostnames
  */
  void *requester(void* input);

 /* Function to pop domain names from queue and write them to the
  * Destination file
  */
  void *resolver();

#endif
