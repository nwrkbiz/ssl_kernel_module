#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define SIG_TEST 44 // we define our own signal


// "header file"
class SigObj {
    
  private:
	int m_Cnt = 0;
	static SigObj* c_Me;
  public:

	// ctor
	SigObj()
	{
		struct sigaction sig;
		sig.sa_sigaction = receiveData; // register signal callback function
		sig.sa_flags = SA_SIGINFO; // signal type
		c_Me = this;
		sigaction(SIG_TEST, &sig, NULL); // custom signal SIG_TEST (needs to be the same as in kernel module)
	}

	void printOut()
	{
		printf("Program will exit now :)\n");
	}

	// callback function for signal
	static void receiveData(int n, siginfo_t *info, void *nothing) {

		printf("received signal %i count is %d \n", info->si_int, c_Me->m_Cnt);

		if(c_Me->m_Cnt++ > 5)
		{
			c_Me->printOut();
			exit(0);
		}
	}
};

// needs to be on top in cpp file
SigObj* SigObj::c_Me = NULL;


int main ( int argc, char **argv )
{

	SigObj mySig;

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
