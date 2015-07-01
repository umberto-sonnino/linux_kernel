# linux_kernel

The linux kernel is an awesome beast. These project exists along with the Operating Systems II course (http://www.dis.uniroma1.it/~quaglia/DIDATTICA/SO-II-6CRM/) which focuses on the linux kernel internals.
In fact the whole project was to modify and extend the kernel so that it supports the two following things:
1 - Synchronization through a barrier: develop a new interface for barriers that can manage TAGs in the interval [0,31]. 
The interface is the following:
- int get_barrier(key_t key, int flags): install the barrier with the id key and return the barrier descriptor. flags defines exclusive installs or simple openings.
- int sleep_on_barrier(int bd, int tag): request to block on a specified barrier
- int awake_barrier(int bd, int tag): awake all the processes on a barrier with the specified tag
- int release_barrier(int bd): remove the barrier

Kernel signaling must be handled appropriately

2 - Semantic session on file

Develop additional software for the VFS Linux, so that it supports a semantic session for opening files of a specific file system managed via VFS. The interface for opening in session mode has to be realized through a custom made system call or using the existing open(). 
When a file is opened in session mode, the whole content is moved into volatile memory and not the buffer cache, and the updates are not visible to other open channels until that file is closed. 


The barriers have been implemented through the wait_queues in the linux kernel, and they are simple wrapper for that structure.
Additionally a hash_map has been used to keep track of the barriers in the kernel, so that the management is efficient enough for critical operations. For the same reason a bitmask is used to keep track of the condition for sleeping or waking up. A single bit is flagged or cleared depending on the action.
Interrupts are handled by checking the return values of functions such as wait_event_interruptible() which returns a >0 value if disturbed during its execution.
malloc and free are replaced by their kernel counterparts kmalloc() and kfree().

The semantic session was built mostly by modifying existing files:
- fs.h was modified to include a pointer to a struct sessione in the definition of the struct file
- namei.c: following the path through which the kernel opens a file, if a specified flag was present (O_SESSION, added inside the opensession.c definition), the allocation for memory is done accordingly. kmalloc is used along with __get_free_pages() to store two whole pages from the buddy system. 
During the opening of the file, the whole content is then read into the newly allocated buffer, via vfs_read and the manipulation of the memory boundaries with set_fs().
- file_table.c: following this time the path through which the kernel closes a file, we need to close the session as well, when open. The file needs to be firstly truncated, then flushed to disk and lastly the resources need to be freed.
- read_write.c: here all the operations of reading and writing from file are handled within volatile memory, so that a read and a write do not have to go back to the buffer cache (or in the worst case to the disk). 

Everything is synchronized by a number of mutexes and semaphores, which prevent the sessions from modifying and viewing incoherent data.

In order to add the system calls to the linux kernel 4.0.5, I've firstly defined the new interfaces in the new files. Then I've added two folders into the downloaded kernel, one barrier/ and another session/. With these two folders containing the needed files, I've added them to the kernel's makefile at the line core-y += [...] session/ barrier/.
The new system call needs to be added into the file [yourpathto]/linux-4.0.5/include/linux/syscalls.h as
asmlinkage int awake_barrier(int bd, int tag);
asmlinkage int get_barrier(key_t key, int flags);
asmlinkage int release_barrier(int md);
asmlinkage int sleep_on_barrier(int bd, int tag);
asmlinkage unsigned opensession(char* fn, int flags, mode_t m, void* buf);
asmlinkage int closesession(unsigned ss_id, void* buf, ssize_t count);
asmlinkage unsigned open_session(char* filename, int flags, mode_t mode);

Then at the end of the system call table located in [yourpathto]/linux-4.0.5/arch/x86/syscalls/syscall_64.tbl I've added the same calls, so the compiler knew:
323	common	sys_get_barrier	get_barrier
324	common	sys_awake_barrier	awake_barrier
325	common	sys_sleep_on	sleep_on_barrier
326	common	sys_release_barrier	release_barrier
327	common	sys_opensession	open_session
328 common	sys_opensession2	opensession
329	common	sys_closesession	closesession

Now everything can be compiled (it takes a while, depending on your configuration) 
> sudo -s 
[password]

> make menuconfig

> cd [yourpathto]/linux-4.0.5/

> make
[wait a while]

> make modules_install install


After a reboot, the new kernel should be running, and the appropriate tests can be run
