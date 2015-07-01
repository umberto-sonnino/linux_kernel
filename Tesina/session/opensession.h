#ifndef _SO_SESS_
#define _SO_SESS_
#include <linux/fs.h>
#include <linux/kernel.h>
#include <uapi/asm-generic/errno-base.h> //Error Codes
#include <linux/slab.h> //kmalloc
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <linux/sched.h>
#include <linux/file.h>

//~ Order of 2 means 4 pages -> 16k memory
#define SO_M_ORDER 2
#define SO_MAX_DATA (1<<SO_M_ORDER)*PAGE_SIZE
#define SO_MAP_SIZE 769
#define O_SESSION 00000004

static DEFINE_MUTEX(session_lock);

struct sessione
{
	void* pages;
	int flags;
	loff_t offset;
	unsigned long size;
	struct rw_semaphore *sem;
};

struct stream
{
	unsigned long id;
	const char *filename;
	int flags;
	mode_t mode;
	void *data;
	struct file *fp;
	struct stream *next;
};

struct pid_node 
{
	pid_t pid;
	unsigned open_files;
	struct pid_node *next;
	struct stream *files;
};

//~ System Calls
asmlinkage unsigned opensession(char* fn, int flags, mode_t m, void* buf);
asmlinkage int closesession(unsigned ss_id, void* buf, ssize_t count);
asmlinkage unsigned open_session(char* fn, int flags, mode_t mode);
//~ Hashmap related calls
struct pid_node* get_pid_node(pid_t key);
struct pid_node* add_pid_node(pid_t key);
void remove_node(pid_t key);
//~ Stream related calls
int add_sess_stream(pid_t key, const char *filename);
struct stream* get_sess_stream(pid_t key, unsigned id);
int remove_sess_stream(pid_t key, unsigned id);

#endif

