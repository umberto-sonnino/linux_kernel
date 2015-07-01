#include "hash.h"

unsigned betto_hash(key_t key)
{
	unsigned h = 0;
	int digit;
	unsigned t = key;
	
	for(t;t!=0;t/=10)
	{
		digit = t%10;
		//~ printk("The current digit: %d \n", digit);
		h = 33 * h + digit;
	}
	//~ printk("The value passeed was %u\n", key);
	
	return h % SIZE;
}

int destroy_map(struct barrier_t **my_ht)
{
	int i = 0;
	
	while(i < SIZE)
	{
		if(my_ht[i] != NULL)
		{
			//there's still some leftovers, something weird is going on?
			struct barrier_t *tmp = my_ht[i];
			do
			{
				printk("Leftover at %d. Freeing...\n", i);
				my_ht[i] = my_ht[i]->next;
				tmp->next = NULL;
				kfree(tmp);
			}while((tmp = my_ht[i]) != NULL);
		}
		
		++i;
	}
	return 0;
}

struct barrier_t* fetch_barrier(struct barrier_t **my_ht, key_t key)
{
	unsigned pos = betto_hash(key);
	struct barrier_t *tmp = my_ht[pos];

	while(tmp != NULL)
	{
		printk("Let's take a look!\n");
		if(tmp->key == key)
		{
			printk("Here's what ya wanted: %d \n", key);
			return tmp;
		}
		tmp = tmp->next;
	}
	printk("Your key wasn't found\n");
	
	return (struct barrier_t*)-1;	
}

unsigned add_barrier(struct barrier_t **my_ht, struct barrier_t *barr)
{
	struct barrier_t *head;
	unsigned pos = betto_hash(barr->key);
	
	if(my_ht[pos] == NULL)
	{
		printk("[ADD @ %u] Nice and easy\n", pos);
		my_ht[pos] = barr;
	}else
	{
		printk("Chaining @%d...\n", pos);
		head = my_ht[pos];
		barr->next = head->next;
		head->next = barr;
	}
	
	return barr->key;
}

int remove_barrier(struct barrier_t **my_ht, key_t key)
{
	unsigned pos = betto_hash(key);
	struct barrier_t *tmp = my_ht[pos];
	
	if(my_ht[pos]->key == key)
	{
		//Deleting this element
		printk("Found the %d guy here. Removing...\n", key);
		my_ht[pos] = tmp->next;
		kfree(tmp);
		return 0;
	}else
	{
		struct barrier_t *cur = my_ht[pos];
		
		printk("I'm goin' in\n");
		while( (tmp = cur->next) != NULL)
		{
			if(tmp->key == key)
			{
				cur->next = tmp->next;
				kfree(tmp);
				return 0;
			}
			cur = cur->next;
		}
	}
	
	return -1;
}
