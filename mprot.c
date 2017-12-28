#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#define ARG0_OFF	100
#define ARG1_OFF	101
#define RESULT_OFF	200

#define PAGE_SIZE 4096
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

/* App IDs are used as indexes into the buffer,
 * and they control the permissions.
 */
static int app_id;
static int app_other_id;

static void segv_handler(int sig_no, siginfo_t* info, void *context)
{
	char *buffer = (char*)((unsigned long)info->si_addr & ~(PAGE_SIZE - 1));

	if (mprotect(buffer, PAGE_SIZE, PROT_READ | PROT_WRITE) == -1)
		handle_error("mprotect");

	/* tell the other side we want to use the buffer */
	buffer[app_id] = 1;

	/* now, check if the buffer was already in-use */
	if (buffer[app_other_id]) {

		/* release the buffer */
		buffer[app_id] = 0;

		/* exiting here will make the buffer access spin
		 * the process until the buffer is "released",
		 * via the app_other_id index.
		 */
		mprotect(buffer, PAGE_SIZE, PROT_NONE);
		return;
	}
}

static inline void dump_mem(char *buffer)
{
	if (mprotect(buffer, PAGE_SIZE, PROT_READ) == -1)
		handle_error("mprotect");
	for (int i = 0; i < 10; i++)
		printf("%d = [0x%x]\n", i, buffer[100 + i]);
	if (mprotect(buffer, PAGE_SIZE, PROT_NONE) == -1)
		handle_error("mprotect");
}

/*
 * Memory should always satisfy the rule:
 *
 * buffer[RESULT_OFF] == buffer[ARG0] + buffer[ARG1_OFF]
 */
static void touch_mem(char *buffer)
{
	assert(buffer[RESULT_OFF] == buffer[ARG0_OFF] + buffer[ARG1_OFF]);

	buffer[ARG0_OFF] = rand() % 0x1f;
	buffer[ARG1_OFF] = rand() % 0x1f;
	buffer[RESULT_OFF] = buffer[ARG0_OFF] + buffer[ARG1_OFF];

	printf("%d = %d + %d\n", buffer[RESULT_OFF], buffer[ARG0_OFF], buffer[ARG1_OFF]);

	/* release the buffer */
	buffer[app_id] = 0;

	if (mprotect(buffer, PAGE_SIZE, PROT_NONE) == -1)
		handle_error("mprotect");
}

static char *init_mem(void)
{
	char *buffer;

	buffer = memalign(PAGE_SIZE, PAGE_SIZE);
	if (buffer == NULL)
		handle_error("memalign");
	memset(buffer, 0, PAGE_SIZE);
	if (mprotect(buffer, PAGE_SIZE, PROT_NONE) == -1)
		handle_error("mprotect");
	return buffer;
}

int main(int argc, char *argv[])
{
	struct sigaction sa;
	char *buffer;

	if (argc < 3) {
		printf("Please, give me local and remote IDs\n");
		exit(EXIT_SUCCESS);
	} else {
		app_id = atoi(argv[1]);
		app_other_id = atoi(argv[2]);
	}

	printf("Local ID %d. Remote ID %d\n", app_id, app_other_id);

	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = segv_handler;
	if (sigaction(SIGSEGV, &sa, NULL) == -1)
		handle_error("sigaction");

	srand(time(NULL));
	buffer = init_mem();

	printf("Memory initialized, got %p\n", buffer);

	while (1) {
		touch_mem(buffer);
	}

	exit(EXIT_SUCCESS);
}
