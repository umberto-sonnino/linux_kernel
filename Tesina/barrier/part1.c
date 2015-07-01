#include "hash.h"

/*
 * Sviluppare un sistema di sincronizzazione basato su barriera che sia 
 * in grado di gestire TAG di sincronizzazione differenziati nell'intervallo [0,31]. 
 * 
 */

static struct barrier_t *table[SIZE] = {NULL};
//One mutex for global creation/deletion and one set of mutexes for insertion/removal in table
static struct mutex barrier_locks[SIZE];
static DEFINE_MUTEX(create_barrier_lock);

/*
 * risveglio da barriera di processi con un determinato tag
 */
asmlinkage int awake_barrier(int bd, int tag)
{
	struct barrier_t *bar;
	unsigned pos = betto_hash(bd);
	if(tag < 0 || tag > 32)
	{
		printk(KERN_INFO "You're trying to access the wrong tag\n");
		return -ENXIO;
	}
	printk(KERN_INFO "[AWAKE_B]\n");
	
	//~ mutex_lock_interruptible(&barrier_lock);
	if(mutex_lock_interruptible(&(barrier_locks[pos])))
		return -ERESTARTSYS;
		
	bar = fetch_barrier(table, bd);
	
	if(bar == NULL)
	{
		printk(KERN_INFO "The bd you passed is nonexistant: %u\n", bd);
		//~ mutex_unlock(&barrier_lock);
		mutex_unlock(&(barrier_locks[pos]));

		return -ENXIO;
	}
	set_bit(tag, &(bar->mask));
	
	//wake_up() instead of wake_up_all() to avoid thundering herd problems
	wake_up(&(bar->queues[tag]));
	
	//~ mutex_unlock(&barrier_lock);
	mutex_unlock(&(barrier_locks[pos]));

	printk(KERN_INFO "[~AWAKE_B]\n");

	return 0;
}

/*
 * installazione del della barriera con chiave di identificazione key,
 * e ritorno del codice operativo (barrier-descriptor); 
 * flags definisce installazioni esclusive o non, oppure semplici aperture  
 */
 
asmlinkage int get_barrier(key_t key, int flags)
{
	struct barrier_t *nova;
	int i = 0;
	unsigned pos = betto_hash(key);

	mutex_lock_interruptible(&create_barrier_lock);
	
	if(fetch_barrier(table, key) == (struct barrier_t*)-1)
	{
		printk(KERN_INFO "[GET_B]\n");
		nova = kmalloc(sizeof(*nova), GFP_KERNEL);
		nova->queues = kmalloc(sizeof(wait_queue_head_t)*TAG_SIZE, GFP_KERNEL);
		nova->key = key;
		nova->flags = flags;
		nova->next = NULL;
		nova->mask = 0;
		mutex_init(&(barrier_locks[pos]));
		
		while(i<TAG_SIZE)
		{
			//~ DECLARE_WAIT_QUEUE_HEAD(dude);
			//~ nova->queues[i] = dude;
			init_waitqueue_head(&(nova->queues[i]));
			i++;
		}
		
		add_barrier(table, nova);
		mutex_unlock(&create_barrier_lock);
		printk(KERN_INFO "[GET_B]The key %u was empty but I just init'd\n", key);
		
		return key;
	}
	else
	{
		mutex_unlock(&create_barrier_lock);

		printk("[GET_B]Hey doode, this %d key's already been used!\n", key);
		return -EEXIST;
	}
	
}

/*
 *  richiesta di blocco sulla barriera, con indicazione del TAG relativo
 */ 
asmlinkage int sleep_on_barrier(int bd, int tag)
{
	struct barrier_t *bar;
	unsigned pos = betto_hash(bd);
	
	//Get the barrier bd and then sleep on the queue tag
	printk(KERN_INFO "[SLEEP_ON_B]\n");
	//~ mutex_lock_inteerruptible(&barrier_lock);
	if(mutex_lock_interruptible(&(barrier_locks[pos])))
		return -ERESTARTSYS;
	bar = fetch_barrier(table, bd);
	
	if(bar == NULL)
	{
		//~ mutex_unlock(&barrier_lock);
		mutex_unlock(&(barrier_locks[pos]));
		printk(KERN_INFO "The bd you passed is nonexistant: %u\n", bd);
		return -ENXIO;
	}
	printk(KERN_DEBUG "\t[Before] Maks is %lu\n", bar->mask);

	clear_bit(tag, &(bar->mask));

	//~ mutex_unlock(&barrier_lock);
	mutex_unlock(&(barrier_locks[pos]));

	printk(KERN_DEBUG "\t[After] Maks is %lu, and test: %d\n", bar->mask, test_bit(tag, &(bar->mask)) );
	
	if( wait_event_interruptible(bar->queues[tag], test_bit(tag, &(bar->mask))) )
		return -ERESTARTSYS;
	printk(KERN_INFO "[~SLEEP_ON_B]\n");

	
	return 0;
}

/*
 * disinstallazione della barriera
 */
asmlinkage int release_barrier(int md)
{
	int result;
	struct barrier_t *bar;

	//Devo rilasciare tutte le risorse associate con questa barriera
	printk(KERN_INFO "[RELEASE_B]\n");
	
	mutex_lock_interruptible(&create_barrier_lock);
	bar = fetch_barrier(table, md);
	if(!list_empty(&(bar->queues->task_list)))
	{
		mutex_unlock(&create_barrier_lock);
		return -EBUSY;
	}
	result = remove_barrier(table, md);
	mutex_unlock(&create_barrier_lock);
	
	printk(KERN_INFO "[~RELEASE_B]\n");

	if(result == -1)
	{
		printk(KERN_INFO "Something went wrong while trying to remove the barrier %d\n", md);
		return -ENXIO;
	}else
	{
		printk(KERN_INFO "Correctly removed everything\n");
		return 0;
	}
	
	
}
