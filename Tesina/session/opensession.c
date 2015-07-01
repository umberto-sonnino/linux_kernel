#include "opensession.h"
#include <linux/namei.h>
/*
 * Se si apre un file, si mantiene una sessione aperta. 
 * Se ci sono vari processi in concorrenza che vogliono aprire lo stesso file,
 * non c'e' bisogno di ricreare la sessione, ma semplicemente di instanziarne 
 * una ad hoc con due nuove pagine di sistema per quel determinato processo.
 * Ovviamente ci serve un Mutex per gestire il tutto per far si che non ci siano 
 * accessi concorrenti alle risorse. 
 */ 
 
static struct pid_node *map[SO_MAP_SIZE] = {NULL};

static unsigned betto_hash(pid_t key)
{
	unsigned h = 0;
	int digit;
	unsigned t = key;
	
	for(;t!=0;t/=10)
	{
		digit = t%10;
		h = 33 * h + digit;
	}
	
	return h % SO_MAP_SIZE;
}

struct pid_node* add_pid_node(pid_t key)
{
	struct pid_node *tmp;
	unsigned h = betto_hash(key);
	struct pid_node *n =  kmalloc(sizeof(struct pid_node), GFP_KERNEL);
	n->pid = key;
	n->open_files = 0;
	n->files = NULL;
	
	if( !map[h] )
	{
		printk(KERN_INFO "%d) I'm adding everything right now\n", key);
		n->next = NULL;
		map[h] = n;
	}else
	{
		//Something already exists, insert at the beginning
		printk(KERN_INFO "%d) Inserting at the right cell\n", key);
		tmp = map[h];
		map[h] = n;
		n->next = tmp;
	}
	
	return n;
}

struct pid_node *get_pid_node(pid_t key)
{
	unsigned h = betto_hash(key);
	struct pid_node *tmp = map[h];
	
	printk(KERN_INFO "I'm going in...\n");
	
	while(tmp != NULL)
	{
		if(tmp->pid == key)
		{
			printk(KERN_INFO "Found your node here %d\n", key);
			return tmp;
		}
		tmp = tmp->next;
	}
	
	printk(KERN_INFO "Couldn't find anything boss...\n");
	
	return 0;
}

void remove_node(pid_t key)
{
	struct pid_node *tmp, *prev;
	struct stream *minion;
	unsigned h = betto_hash(key);
	
	if(!map[h])
	{
		printk(KERN_INFO "Your guy %d wasn't here...\n", key);
		return;
	}

	tmp = map[h];
	prev = map[h];
	
	while(tmp)
	{
		if(tmp->pid == key)
		{
			printk(KERN_INFO "Found it! Removing\n"); 
			prev->next = tmp->next;
			minion = tmp->files;
			while(minion)
			{
				tmp->files = minion->next;
				free_pages((unsigned long)minion->data, SO_M_ORDER );
				kfree(minion);
				minion = minion->next; 
				printk(KERN_INFO "\tRemoving minion\n");
			}
			kfree(tmp);
			return;
		}
		prev = tmp;
		tmp = tmp->next;
	}
	
	printk(KERN_INFO "Couldn't find what you were looking for...\n");
	
}

int add_sess_stream(pid_t key, const char *fn)
{
	struct pid_node *tmp = get_pid_node(key);
	struct stream *s = kmalloc(sizeof(struct stream), GFP_KERNEL);
	s->filename = fn;
	s->id = tmp->open_files++;
	s->data = (void*)__get_free_pages(GFP_KERNEL, SO_M_ORDER);
	printk(KERN_INFO "File %s added to %d as %lu\n", fn, key, s->id);
	
	if(tmp->files)
	{
		s->next = tmp->files;
	}
	
	tmp->files = s;
	
	return s->id;
	
}
struct stream* get_sess_stream(key_t key, unsigned m_id)
{
	struct pid_node *tmp = get_pid_node(key);
	struct stream *s = tmp->files;
	
	if(!s)
	{
		printk(KERN_INFO "There was nothing here\n");
		return 0;
	}
	
	while(s)
	{
		if((s->id == m_id))
		{
			printk(KERN_INFO "Found your stream here: %s, %lu\n", s->filename, s->id);
			return s;
		}
		printk("This wasn't it %lu\n", s->id);
		s = s->next;
	}
	
	printk(KERN_INFO "I couldn't find the stream you were looking for: (%d) @%d...\n", key, m_id);
	
	return 0;
	
}

int remove_sess_stream(key_t key, unsigned m_id)
{
	struct stream *prev, *s;
	struct pid_node *tmp = get_pid_node(key);
	if(!tmp)
	{
		printk(KERN_INFO "Trying to access a non-existant pid\n");
		return -1;
	}
	s = tmp->files;
	
	if(!s)
	{
		printk(KERN_INFO "You're trying to remove something that's not here??\n");
		return -1;
	}
	
	prev = tmp->files;
	while(s)
	{
		if(s->id == m_id)
		{
			printk(KERN_INFO "Found your stream (%d) @%d. Removing it..!\n", key, m_id);
			prev->next = s->next;
			free_pages((unsigned long)s->data, SO_M_ORDER);
			kfree(s);
			return 1;
		}
		prev = s;
		s = s->next;
		printk(KERN_INFO "Not here yet, iterating...\n");
	}
	
	printk(KERN_INFO "You're looking for something that doesn't exist\n");
	return 0;
}

asmlinkage unsigned opensession(char* fn, int flags, mode_t m, void* buf)
{
	loff_t pos;
	struct stream *str;
	struct pid_node *n;
	
	mm_segment_t old_fs = get_fs();
	
	mutex_lock_interruptible(&session_lock);
	
	n = get_pid_node(current->pid);
	printk(KERN_DEBUG "[Open Session] %d has begun\n", current->pid);
	
	if(!n)
	{
		n = add_pid_node(current->pid);
	}
	
	str = kmalloc(sizeof(struct stream), GFP_KERNEL);
	str->flags = flags;
	str->mode = m;
	str->filename = fn;
	str->data = (void*)__get_free_pages(GFP_KERNEL, SO_M_ORDER);
	str->id = n->open_files++;
	
	if(n->files)
		str->next = n->files;
	n->files = str;
	
	if(!str->data)
	{
		printk(KERN_INFO "There was some kind of error with page allocations\n");
		kfree(str);
		mutex_unlock(&session_lock);
		
		return -ENOMEM;
	}
	
	set_fs(KERNEL_DS);
	str->fp = filp_open(fn, flags, m);
	if(IS_ERR(str->fp))
	{
		printk(KERN_DEBUG "%pX\n", ERR_PTR((long)str->fp));
		
		set_fs(old_fs);
		free_pages((unsigned long)str->data, SO_M_ORDER);
		kfree(str);
		mutex_unlock(&session_lock);
		
		return -EIO;
	}
	
	//I need to read the contents of the file into the buffer
	//The buffer is allocated through get_free_pages()
	pos = 0;
	if( !(O_WRONLY & flags) )
	{
		printk(KERN_INFO "Flags are alright and I can read\n");
		if( vfs_read (str->fp, str->data, SO_MAX_DATA, &pos) >= 0 )
		{
			mutex_unlock(&session_lock);
			set_fs(old_fs);
			copy_to_user(buf, str->data, SO_MAX_DATA);

			printk(KERN_DEBUG "[Open Session] %d has ended!\n", current->pid);
			
			return str->id;
		}else
		{
			printk(KERN_INFO "Something went wrong while trying to read\n");
			set_fs(old_fs);
			free_pages((unsigned long)str->data, SO_M_ORDER);
			kfree(str);
			mutex_unlock(&session_lock);
			
			return -EIO;
		}
	}
	
	printk(KERN_INFO "[Open Session] Ending %d that's not allowed to read\n", current->pid);

	set_fs(old_fs);
	mutex_unlock(&session_lock);
	
	return str->id;
}						


asmlinkage int closesession(unsigned ss_id, void* buf, ssize_t count)
{
	void* d;
	int ret;
	loff_t offset;
	struct pid_node *n; 
	struct stream *s;
	mm_segment_t old_fs = get_fs();
	
	mutex_lock_interruptible(&session_lock);
	n = get_pid_node(current->pid);
	s = get_sess_stream(current->pid, ss_id);
	
	if(!s || !n)
	{
		printk(KERN_DEBUG "Something with the node or stream\n");
		mutex_unlock(&session_lock);
		return -ENODEV;
	}
	
	printk(KERN_DEBUG "[Close Session] %lu has begun. Closing: %s at %p\n", s->id, s->filename, s->fp);
	
	d = kmalloc(count, GFP_KERNEL); 
	if(!d)
	{
		printk(KERN_DEBUG "Failing on initializing d\n");
		kfree(d);
		mutex_unlock(&session_lock);
		return -EFAULT;
	}	
	
	set_fs(KERNEL_DS);
	if( (s->flags & O_WRONLY) || (s->flags & O_RDWR) )
	{
		if(!buf)
		{
			printk(KERN_DEBUG "Failing on receiving buf\n");
			kfree(d);
			mutex_unlock(&session_lock);
			return -EFAULT;
		}
		if( (ret = copy_from_user(d, buf, count)) )
		{
			printk(KERN_DEBUG "I've had some trouble with copying data from user space! %d\n", ret);
			kfree(d);
			mutex_unlock(&session_lock);
			return -EFAULT;
		}
		
		printk("Buffer of length %zu contains: %s\n", count, (char*)d);
		ret = vfs_write(s->fp, d, count,  &offset);
		printk(KERN_INFO "Written to file\n");

		if(ret < 0)
		{
			printk(KERN_DEBUG "File at: %p ---- Buffer at: %p", s->fp, buf);
			printk(KERN_DEBUG "Apparently something went wrong while writing: %d\n", ret);
			set_fs(old_fs);
			kfree(d);
			mutex_unlock(&session_lock);
			return -EIO;
		}
	}
	
	kfree(d);
	filp_close(s->fp, 0);
	printk(KERN_INFO "File closed!\n");

	set_fs(old_fs);
	remove_sess_stream(current->pid, s->id);
	if(!n->files)
	{
		printk(KERN_INFO "No more open files in %d pid_node, removing\n", current->pid);
		remove_node(current->pid);
	}
	printk(KERN_DEBUG "[Close Session] %d has ended\n", current->pid);
	mutex_unlock(&session_lock);
	
	return 0;
}

asmlinkage unsigned open_session(char* fname, int flags, mode_t mode)
{
	struct path p;
	struct kstat ks;
	struct fd f;
	unsigned id = 0;
	kern_path(fname, 0, &p);
	vfs_getattr(&p, &ks);
	if(ks.size >= SO_MAX_DATA)
	{
		printk(KERN_DEBUG "The file you're trying to open is too big. Cannot open!\n");
		return -1;
	}
	mutex_lock(&session_lock);
	id = do_sys_open(AT_FDCWD, fname, flags, mode);
	
	if(signal_pending(current))
	{
		printk(KERN_DEBUG "[OPEN] There's a signal pending!\n");
		f = __to_fd(__fdget_pos(id));
		filp_close(f.file, current->files);
		
		return -EINTR;
	}
	mutex_unlock(&session_lock);
	
	return id;
}
