#include <linux/kernel.h>
#include <linux/ipc.h> //flags
#include <asm/errno.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>


#define SIZE 769
#define TAG_SIZE 32
