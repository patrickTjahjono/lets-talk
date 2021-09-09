#include "list.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <stdio.h> 
#include <readline/readline.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//thread ids
pthread_t keyIn;
pthread_t UDPin;
pthread_t UDPout;
pthread_t sysOut;

//thread termination flag
int terminate;

//network variables
int local;
struct addrinfo hints, *res;
int sfd;

int remote;
struct addrinfo houts, *resout;

//locks and conditions
pthread_mutex_t receiverLock, senderLock;
pthread_cond_t printcond, keycond;

//list
List* Lsender;
List* Lreceiver;

void encrypt(char* msg)
{
	for(int i =0 ; i<strlen(msg);i++)
		msg[i]= (msg[i] + 32) % 256;
}

void decrypt(char* msg)
{
	for(int i =0 ; i<strlen(msg);i++)
	{
		msg[i]= msg[i] - 32;
		if(msg[i] < 0 )
			msg[i]=msg[i]+256;
	}
}

void *keyboardThread(void *nothing)
{
	pthread_detach(pthread_self());
	char inputList[4000];
	char* temp;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
	while(1)
	{	
		if(terminate==1)
		{
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
			pthread_exit(NULL);
			continue;// skip iteration until cancelled
		}
		temp = readline("");//first line
		if(strlen(temp) == 0)
		{
			free(temp);
			continue; // skip input
		}
		strcpy(inputList,temp);
		strcat(inputList,"\n");
		free(temp);
		temp = readline("");//second line to get loop started
		while(strlen(temp) != 0 )//check every line for blank line
		{
			if(temp == NULL)//EOF
				break;
			strcat(inputList,temp);//add line to the msg
			strcat(inputList,"\n");
			free(temp);
			temp = readline("");
		}
		free(temp);
		//remote user status checker
		if(strcmp(inputList,"!status\n") == 0)
		{	
			int connectionSfd = socket(resout->ai_family, resout->ai_socktype, resout->ai_protocol);//create a test connection socket
			if(connectionSfd == -1)
			{	printf("test connection socket creation fail");
				exit(1);
			}
			if(bind(connectionSfd, resout->ai_addr, resout->ai_addrlen) == -1)//check if binding fail
				printf("online\n");// online if the binding using test socket  fail
			else 
				printf("offline\n");// offline if the binding using test socket successful
			close(connectionSfd);
			continue;
		}
		
		//inserting input into list
		pthread_mutex_lock(&senderLock);// lock senderLock to be used in this thread
		if(List_prepend(Lsender,  inputList) == -1)//check if text is successfuly added into the list
		{
			printf("fail to add text to sender");
			exit(1);
		}
		pthread_cond_signal(&keycond);//increment keycond
		pthread_mutex_unlock(&senderLock);//unlock senderLock so that senderThread can access and send

	}
}

void *senderThread(void *nothing)
{
	pthread_detach(pthread_self());
	char msg[4000];
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
	while(1)
	{	
		if(terminate==1)
		{
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
			pthread_exit(NULL);
			continue;// skip iteration until cancelled
		}
		pthread_mutex_lock(&senderLock);
		if(List_count(Lsender)==0)//if zero wait for signal
			pthread_cond_wait(&keycond,&senderLock);
		//sending message
		strcpy(msg,(char*) List_trim(Lsender));
		msg[strlen(msg)] = '\0';
		if(strstr(msg,"!exit\n")!=NULL )//if !exit found send it to own receiver
		{
			int connectionSfd = socket(resout->ai_family, resout->ai_socktype, resout->ai_protocol);//create a test connection socket
			if(connectionSfd == -1)
			{	printf("test connection socket creation fail");
				exit(1);
			}
			if(bind(connectionSfd, resout->ai_addr, resout->ai_addrlen) != -1)//check if binding fail
			{
				exit(1);
			}
		}
			
		encrypt(msg);
		if(sendto(sfd, msg, sizeof(msg),0,resout->ai_addr, resout->ai_addrlen) == -1) 
		{
			printf("sendto fail to send some message");
		}
		pthread_mutex_unlock(&senderLock);
	}
}
		
void *receiverThread(void *nothing)
{

	char inputList[4000];
	int numbytes;
	while(1)
	{	
		while ((numbytes = recvfrom(sfd, inputList, sizeof(inputList),0,resout->ai_addr, &(resout->ai_addrlen))) != -1) 
		{
			//receiving message
			pthread_mutex_lock(&receiverLock);
			inputList[numbytes] = '\0';
			decrypt(inputList);
			if(strstr(inputList,"!exit\n") != NULL )
			{//sending exit function back to the sender so that receiver thread can return NULL
				terminate=1;
				encrypt(inputList);

				sendto(sfd, inputList, sizeof(inputList),0,resout->ai_addr, resout->ai_addrlen);
				pthread_mutex_unlock(&receiverLock);
				break;
			}
			if(List_prepend(Lreceiver, inputList) == -1)//check if text is successfuly added into the list
			{
				printf("fail to add text to receiver");
				exit(1);
			}
			pthread_cond_signal(&printcond);
			pthread_mutex_unlock(&receiverLock);
		}
		if(terminate==1)
			break;
	}
	printf("cancelling threads");fflush(stdout);
	pthread_cancel(UDPin);
	pthread_join(UDPin,NULL);
	pthread_cancel(sysOut);
	pthread_join(sysOut,NULL);
	pthread_cancel(keyIn);
	pthread_join(keyIn,NULL);
	pthread_exit(NULL);
}		
		

void *printerThread(void *nothing)
{
	pthread_detach(pthread_self());
	char msg[4000];
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
	while(1)
	{	
		if(terminate==1)
		{
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
			pthread_exit(NULL);
			continue;// skip iteration until cancelled
		}
		pthread_mutex_lock(&receiverLock);// locking receiverLock to be accessed
		if(List_count(Lreceiver)==0)
			pthread_cond_wait(&printcond, &receiverLock );
		while(List_count(Lreceiver) > 0)
		{//check if there is any message on the list		
		strcpy(msg,(char*) List_trim(Lreceiver));
		printf("%s", msg);
		fflush(stdout);
		}
		pthread_mutex_unlock(&receiverLock);
	}
}


int main(int argc, char* argv[] ){
	//argv = (MYPORT) (remote machine) (REMOTEPORT)

	// Setting network
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	memset(&houts, 0, sizeof houts);
	houts.ai_family = AF_UNSPEC;
	houts.ai_socktype = SOCK_DGRAM;
	
	
	// Setting socket for receiver
	if((local = getaddrinfo(NULL, argv[1], &hints, &res)) != 0) //argv[1] = MYPORT
	{
		printf("receiver: getaddrinfo fail to identify host");
		return 1;
	}
	if((sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
	{
		printf("fail to create socket receiver");
	} 
	if(bind(sfd, res->ai_addr, res->ai_addrlen) == -1)// binding to local host
	{
		close(sfd);
		printf("fail to bind");
	}
	
	// Setting socket for sender
	struct hostent *ho = gethostbyname(argv[2]);//argv[2] = remote machine
	
	if((remote = getaddrinfo(inet_ntoa(*(struct in_addr*)ho->h_addr_list[0]), argv[3], &houts, &resout)) !=0) //argv[3] = REMOTEPORT
	{
		printf("sender: getaddrinfo fail to identify host");
		return 1;
	}


	Lsender = List_create();
	Lreceiver = List_create();
	
	if (pthread_mutex_init(&senderLock, NULL) != 0){
		printf("initialize sender lock fail");
		exit(1);
	}
	if(pthread_mutex_init(&receiverLock, NULL) != 0){
		printf("initialize receiver Lock fail");
		exit(1);
	}

	if (pthread_cond_init(&keycond,  NULL) != 0){
		printf("initialize sender cond fail");
		exit(1);
	}
	if(pthread_cond_init(&printcond,NULL) != 0){
		printf("initialize receiver cond fail");
		exit(1);
	}

	terminate = 0;
	
	pthread_create(&keyIn, NULL, keyboardThread,NULL);
	pthread_create(&UDPin, NULL, senderThread,NULL);
	pthread_create(&UDPout, NULL, receiverThread,NULL);
	pthread_create(&sysOut, NULL, printerThread,NULL);
	
	
	
	pthread_join(UDPout,NULL);
	printf("printer joined");
	fflush(stdout);
		
	close(sfd);
	freeaddrinfo(resout);
	freeaddrinfo(res);
	List_free(Lsender,free);
	List_free(Lreceiver,free);
	printf("program is done");fflush(stdout);
	return 0;
}
