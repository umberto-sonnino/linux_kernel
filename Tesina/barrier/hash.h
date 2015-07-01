#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ipc.h> //flags
#include <asm/errno.h>
#include <linux/mutex.h>

#define SIZE 769
#define TAG_SIZE 32

struct barrier_t
{
	key_t key;
	int flags;
	volatile unsigned long mask;
	struct barrier_t* next; // *prev; non so se servira'
	wait_queue_head_t* queues;
};

//In practice a very good algorithm, theorically not very sound
unsigned betto_hash(key_t key);

int destroy_map(struct barrier_t **table);
struct barrier_t* fetch_barrier(struct barrier_t **table, key_t key);
unsigned add_barrier(struct barrier_t **table, struct barrier_t *barr);
int remove_barrier(struct barrier_t **table, key_t key);



