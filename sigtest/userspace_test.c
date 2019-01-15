#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define SIG_TEST 44 // we define our own signal


// callback function for signal
void receiveData(int n, siginfo_t *info, void *unused) {
	printf("received signal %i\n", info->si_int);
	printf("Program will exit now :)\n");
	exit(0);
}

int main ( int argc, char **argv )
{
	struct sigaction sig;
	sig.sa_sigaction = receiveData; // register signal callback function
	sig.sa_flags = SA_SIGINFO; // signal type
	sigaction(SIG_TEST, &sig, NULL); // custom signal SIG_TEST (needs to be the same as in kernel module)

	// print pid to stdout for convenience
	printf("%i\n", getpid());

	// keep process alive and wait for signal from kernel
	while(1){
		// do some fancy work here
		// signal will stop executing this work 
		// and calls callback function
	};

	return 0;
}
