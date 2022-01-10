#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "museumsim.h" 


struct shared_data {   

	     pthread_mutex_t ticket_mutex;
	     int tickets;

		 int guides_in_museum;  			 // Number of guides in the museum 
		 int visitors_in_museum; 			 // Number of visitors in the museums
		 int visitors_waiting_outside;  	 // Visitors waiting to enter museum
		 int guides_waiting_to_leave_museum; // Guides watiing to leave --> all guides must leave together
		 int guides;						 // Total number of guides 

		 int guidesAvaliable;				 // To keep track of the guides avaliable
		 int guide_admitted_flag;			 // To signal to a waiting visitor that a guide is admitting them
	
		 pthread_cond_t visitorLeft;		 // A visitor broadcast to a guide that a visitor has left the museum

		 pthread_cond_t guideLeft;			 // Purpose 1: A guide broadcasts to other guide that they left --> so both leave at same time
		 									 // Purpose 2: A guide broadcasts to waiting guides that they left --> new guides can enter
	
		 pthread_cond_t guideAdmits;	 	 // A guide broadcasts to visitors that they are admitting a visitor

		 pthread_cond_t visitorWaiting;		 // A visitor broadcasts to a guide that they are waiting in the queue
	 
		 //pthread_cond_t newGuide; 			 // Broadcasts that a new guide is avaliable when no guides currently are
		 									 // Hoped this would fix case for num_guides=3
};

static struct shared_data shared;


/**
 * Set up the shared variables for your implementation.
 * 
 * `museum_init` will be called before any threads of the simulation
 * are spawned.
 */
void museum_init(int num_guides, int num_visitors) 
{
	pthread_mutex_init(&shared.ticket_mutex, NULL);
	shared.tickets = MIN(VISITORS_PER_GUIDE * num_guides, num_visitors);

	shared.guides_in_museum = 0; 
	shared.visitors_in_museum = 0; 
	shared.visitors_waiting_outside = 0;
	shared.guides_waiting_to_leave_museum = 0;
	shared.guides = num_guides;
	shared.guidesAvaliable = 0;
	shared.guide_admitted_flag = 0;

	pthread_cond_init(&shared.visitorLeft, NULL);
	pthread_cond_init(&shared.guideLeft, NULL); 
	pthread_cond_init(&shared.guideAdmits, NULL);
	pthread_cond_init(&shared.visitorWaiting, NULL);
	//pthread_cond_init(&shared.newGuide, NULL);
}


/**
 * Tear down the shared variables for your implementation.
 * 
 * `museum_destroy` will be called after all threads of the simulation
 * are done executing.
 */
void museum_destroy()
{
    pthread_mutex_destroy(&shared.ticket_mutex);
    pthread_cond_destroy(&shared.visitorLeft);
    pthread_cond_destroy(&shared.guideLeft);
    pthread_cond_destroy(&shared.guideAdmits);
    pthread_cond_destroy(&shared.visitorWaiting);
	//pthread_cond_destroy(&shared.newGuide);
}


/**
 * Implements the visitor arrival, touring, and leaving sequence.
 */
void visitor(int id) 
{ 
	visitor_arrives(id); 
	pthread_mutex_lock(&shared.ticket_mutex);
	
	// If there are no tickets left, leave without touring
	if (shared.tickets == 0) { 
		// TESTING: printf("NO TICKETS LEFT\n");
		visitor_leaves(id); 
        pthread_mutex_unlock(&shared.ticket_mutex);
        return;
	}
	
	shared.visitors_waiting_outside++; 	// Increment the visitors waiting to enter
	shared.tickets--;					// Give visitor a ticket 

	// Make sure there is a guide avaliable for touring before notifying guide
	//while(shared.guidesAvaliable == 0){
		// TESTING: printf("waiting on guide avaliable");
		//pthread_cond_wait(&shared.newGuide, &shared.ticket_mutex); 
	//}
	
	// Broadcast that there is a waiting visitor to all guides 
	pthread_cond_broadcast(&shared.visitorWaiting);   
 
	// The visitor waits for a guide to admit them
	while(shared.guide_admitted_flag == 0 && shared.guides_in_museum == 0) { 
		// TESTING: printf("Visitor waiting to be admitted\n");
		pthread_cond_wait(&shared.guideAdmits, &shared.ticket_mutex); 
	}

	// Reset flag to 0 for next visitor admittance
	shared.guide_admitted_flag = 0; 

	// Unlock muetx for concurrent tours to take place (Relock afterwards)
	pthread_mutex_unlock(&shared.ticket_mutex); 
	visitor_tours(id); 
	pthread_mutex_lock(&shared.ticket_mutex);

	visitor_leaves(id);
	shared.visitors_in_museum--;  // Decrement the visitors that are in the museum

	// Broadcast to guides waiting to leave in case they are waiting on visitors to leave 
	pthread_cond_broadcast(&shared.visitorLeft);
	pthread_mutex_unlock(&shared.ticket_mutex);
}

/**
 * Implements the guide arrival, entering, admitting, and leaving sequence.
 */ 
void guide(int id)
{
	 guide_arrives(id);
	 pthread_mutex_lock(&shared.ticket_mutex); 

	// Guide can leave without entering if:  1) No tickets are left  2) No visitors waiting
	 if(shared.tickets == 0 && shared.visitors_waiting_outside == 0){    
		 // TESTING: printf("Guide leaves without entering\n");
         guide_leaves(id);
         pthread_mutex_unlock(&shared.ticket_mutex); 
         return; 
	 } 


	// Wait outside if: 1) Museum has max number of guides or 2) There are guides waiting to leave the museum   
	 while(shared.guides_in_museum == GUIDES_ALLOWED_INSIDE) { //|| shared.guides_waiting_to_leave_museum > 0) {   
		 // TESTING: printf("Guide waiting to enter museum"); 
		 pthread_cond_wait(&shared.guideLeft, &shared.ticket_mutex);  
	 } 

	 guide_enters(id);
	 shared.guides_in_museum++;  		  // Increment the guides that are in the museum
	 //shared.guidesAvaliable++;
	 //pthread_cond_broadcast(&shared.newGuide);  

	 int visitors_admitted_by_guide = 0;  // Keep track of visitors admitted by the guide

	 // Admit visitors until 1) The guide is at max value or  2) There are no more people left to admit  
	 while(VISITORS_PER_GUIDE > visitors_admitted_by_guide && (shared.tickets > 0 || shared.visitors_waiting_outside > 0)){

		 // Flag to indicate if there are no visitors left to admit
		 int break_from_loop = 0; 

		 // Check to see if there are visitors left to admit, if not, set flag and break from loop
		 if(shared.tickets == 0 && shared.visitors_waiting_outside == 0){  
			 break_from_loop = 1; 
			 break; 
		 } 
		 
		 // int museum_capacity = shared.guides_in_museum * VISITORS_PER_GUIDE; --> Was going to use for higher num_guide cases

		 // Wait for 1) Visitors to enter the waiting queue or 2) For the museum to get space 
		 while(shared.visitors_waiting_outside == 0) {//&& museum_capacity != shared.visitors_in_museum) { 
			 // TESTING: printf("Guide waiting to admit visitors\n"); 
			 // Another test to see if there are visitors left to admit, if not, set flag and break from loop
			 if(shared.tickets == 0 && shared.visitors_waiting_outside == 0){  
			 	break_from_loop = 1; 
			 	break; 
		 	 } 
			 pthread_cond_wait(&shared.visitorWaiting, &shared.ticket_mutex); 
		 }

		 // If flag was previously set, there are no more visitors left to admit, break from loop
		 if(break_from_loop == 1){ 
			 // TESTING: printf("No tickets or waiting visitors so exit loop\n");
			 break;
		 }

		 //Guide admits, update variables 
		 guide_admits(id);
		 shared.visitors_waiting_outside--; 
		 shared.visitors_in_museum++;
		 visitors_admitted_by_guide++; 			// Increment counter
		 shared.guide_admitted_flag = 1;		// Set flag so that visitor can stop waiting to enter

		 // Was going to implement for num_guides = 3:
		 //if(visitors_admitted_by_guide >= VISITORS_PER_GUIDE){
			//shared.guidesAvaliable--;
	 	 //} 
	
		 // TESTING: printf("Broadcast that to visitor that guide admitted them\n"); 
		 // Broadcast to visitor that the guide admitted them
		 pthread_cond_broadcast(&shared.guideAdmits); 
	 } 

	 shared.guides_waiting_to_leave_museum++;		// Keep track of guides finished/that are waiting to leave
	 pthread_cond_broadcast(&shared.guideLeft);		// Let other guides know that one has finished 
 

	 // TESTING
	 // printf("\n");
	 // printf("TICKETS LEFT %d ", shared.tickets);
	 // printf("\n");
	 // printf("visitors waiting outside %d ", shared.visitors_waiting_outside);
	 // printf("\n");
	 // printf("visitors inside museum %d ", shared.visitors_in_museum);
	 // printf("\n");
	 // printf("guides waiting to leave %d ", shared.guides_waiting_to_leave_museum);
	 // printf("\n");
	 // printf("guides in museum %d ", shared.guides_in_museum);
	 // printf("\n"); 
	 // printf("\n");

	 // To deal with the situation that there is only one guide:
	 if(shared.guides == 1){

		 // Wait for all visitors to leave museum first 
		while(shared.visitors_in_museum != 0){
			// TESTING: printf("guide waiting to leave\n");
			pthread_cond_wait(&shared.visitorLeft, &shared.ticket_mutex); 
		}

		guide_leaves(id); 
		shared.guides_in_museum--;	// Decrement the amount of guides in museum

	 } else{	//To deal with the situation where there is more than one guide

		// Wait for all visitors to leave before guides can leave
		while(shared.visitors_in_museum > 0) { 		//&& shared.guides_waiting_to_leave_museum != shared.guides_in_museum){
			printf("guide waiting to leave\n");
			pthread_cond_wait(&shared.visitorLeft, &shared.ticket_mutex);  
		}

		// Wait for all guides in the museum to be ready to leave
		if(shared.guides_in_museum > 1){
			while(shared.guides_waiting_to_leave_museum < GUIDES_ALLOWED_INSIDE){
				pthread_cond_wait(&shared.guideLeft, &shared.ticket_mutex); 
			}
		}

		guide_leaves(id); 
		shared.guides_in_museum--;					// Decrement the guides in the museum
		shared.guides_waiting_to_leave_museum--;	// Decrement the guides that are waiting to leave

	 }

	pthread_mutex_unlock(&shared.ticket_mutex); 
	return;
}