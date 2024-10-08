#ifndef _LINUX_SCHED_H
#define _LINUX_SCHED_H

/*
 * Define 'struct task_struct' and provide the main scheduler
 * APIs (schedule(), wakeup variants, etc.)
 */

#include <uapi/linux/sched.h>

#include <asm/current.h>

#include <linux/pid.h>
#include <linux/sem.h>
#include <linux/shm.h>
#include <linux/kcov.h>
#include <linux/mutex.h>
#include <linux/plist.h>
#include <linux/hrtimer.h>
#include <linux/seccomp.h>
#include <linux/nodemask.h>
#include <linux/rcupdate.h>
#include <linux/resource.h>
#include <linux/latencytop.h>
#include <linux/sched/prio.h>
#include <linux/signal_types.h>
#include <linux/mm_types_task.h>
#include <linux/task_io_accounting.h>

 /* task_struct member predeclarations (sorted alphabetically): */
struct audit_context;
struct backing_dev_info;
struct bio_list;
struct blk_plug;
struct cfs_rq;
struct fs_struct;
struct futex_pi_state;
struct io_context;
struct mempolicy;
struct nameidata;
struct nsproxy;
struct perf_event_context;
struct pid_namespace;
struct pipe_inode_info;
struct rcu_node;
struct reclaim_state;
struct robust_list_head;
struct sched_attr;
struct sched_param;
struct seq_file;
struct sighand_struct;
struct signal_struct;
struct task_delay_info;
struct task_group;

/*
 * Task state bitmask. NOTE! These bits are also
 * encoded in fs/proc/array.c: get_task_state().
 *
 * We have two separate sets of flags: task->state
 * is about runnability, while task->exit_state are
 * about the task exiting. Confusing, but this way
 * modifying one set can't modify the other one by
 * mistake.
 */

 /* Used in tsk->state: */
#define TASK_RUNNING			0 // 运行态 或 可运行态
#define TASK_INTERRUPTIBLE		1 // 可中断的睡眠态
#define TASK_UNINTERRUPTIBLE		2 // 不可中断的睡眠态
#define __TASK_STOPPED			4 // 停止态
#define __TASK_TRACED			8 // 跟踪态
/* Used in tsk->exit_state: */
#define EXIT_DEAD			16 // 退出态
#define EXIT_ZOMBIE			32 // 僵尸态
#define EXIT_TRACE			(EXIT_ZOMBIE | EXIT_DEAD) // 跟踪退出态
/* Used in tsk->state again: */
#define TASK_DEAD			64 // 死亡态
#define TASK_WAKEKILL			128 // 等待杀死态
#define TASK_WAKING			256 // 正在唤醒态
#define TASK_PARKED			512 // 停车态
#define TASK_NOLOAD			1024 // 无负载态
#define TASK_NEW			2048 // 新建态
#define TASK_STATE_MAX			4096 // 状态最大值

#define TASK_STATE_TO_CHAR_STR		"RSDTtXZxKWPNn" // 任务状态对应的字符

/* Convenience macros for the sake of set_current_state: */
#define TASK_KILLABLE			(TASK_WAKEKILL | TASK_UNINTERRUPTIBLE) // 可被杀死态
#define TASK_STOPPED			(TASK_WAKEKILL | __TASK_STOPPED) // 停止态
#define TASK_TRACED			(TASK_WAKEKILL | __TASK_TRACED) // 跟踪态

#define TASK_IDLE			(TASK_UNINTERRUPTIBLE | TASK_NOLOAD) // 空闲态

/* Convenience macros for the sake of wake_up(): */
#define TASK_NORMAL			(TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE) // 正常态
#define TASK_ALL			(TASK_NORMAL | __TASK_STOPPED | __TASK_TRACED) // 所有态

/* get_task_state(): */
#define TASK_REPORT			(TASK_RUNNING | TASK_INTERRUPTIBLE | \
					 TASK_UNINTERRUPTIBLE | __TASK_STOPPED | \
					 __TASK_TRACED | EXIT_ZOMBIE | EXIT_DEAD)

#define task_is_traced(task)		((task->state & __TASK_TRACED) != 0)

#define task_is_stopped(task)		((task->state & __TASK_STOPPED) != 0)

#define task_is_stopped_or_traced(task)	((task->state & (__TASK_STOPPED | __TASK_TRACED)) != 0)

#define task_contributes_to_load(task)	((task->state & TASK_UNINTERRUPTIBLE) != 0 && \
					 (task->flags & PF_FROZEN) == 0 && \
					 (task->state & TASK_NOLOAD) == 0)

#ifdef CONFIG_DEBUG_ATOMIC_SLEEP

#define __set_current_state(state_value)			\
	do {							\
		current->task_state_change = _THIS_IP_;		\
		current->state = (state_value);			\
	} while (0)
#define set_current_state(state_value)				\
	do {							\
		current->task_state_change = _THIS_IP_;		\
		smp_store_mb(current->state, (state_value));	\
	} while (0)

#else
/*
 * set_current_state() includes a barrier so that the write of current->state
 * is correctly serialised wrt the caller's subsequent test of whether to
 * actually sleep:
 *
 *   for (;;) {
 *	set_current_state(TASK_UNINTERRUPTIBLE);
 *	if (!need_sleep)
 *		break;
 *
 *	schedule();
 *   }
 *   __set_current_state(TASK_RUNNING);
 *
 * If the caller does not need such serialisation (because, for instance, the
 * condition test and condition change and wakeup are under the same lock) then
 * use __set_current_state().
 *
 * The above is typically ordered against the wakeup, which does:
 *
 *	need_sleep = false;
 *	wake_up_state(p, TASK_UNINTERRUPTIBLE);
 *
 * Where wake_up_state() (and all other wakeup primitives) imply enough
 * barriers to order the store of the variable against wakeup.
 *
 * Wakeup will do: if (@state & p->state) p->state = TASK_RUNNING, that is,
 * once it observes the TASK_UNINTERRUPTIBLE store the waking CPU can issue a
 * TASK_RUNNING store which can collide with __set_current_state(TASK_RUNNING).
 *
 * This is obviously fine, since they both store the exact same value.
 *
 * Also see the comments of try_to_wake_up().
 */
#define __set_current_state(state_value) do { current->state = (state_value); } while (0)
#define set_current_state(state_value)	 smp_store_mb(current->state, (state_value))
#endif

 /* Task command name length: */
#define TASK_COMM_LEN			16

extern cpumask_var_t			cpu_isolated_map;

extern void scheduler_tick(void);

#define	MAX_SCHEDULE_TIMEOUT		LONG_MAX

extern long schedule_timeout(long timeout);
extern long schedule_timeout_interruptible(long timeout);
extern long schedule_timeout_killable(long timeout);
extern long schedule_timeout_uninterruptible(long timeout);
extern long schedule_timeout_idle(long timeout);
asmlinkage void schedule(void);
extern void schedule_preempt_disabled(void);

extern int __must_check io_schedule_prepare(void);
extern void io_schedule_finish(int token);
extern long io_schedule_timeout(long timeout);
extern void io_schedule(void);

/**
 * struct prev_cputime - snapshot of system and user cputime
 * @utime: time spent in user mode
 * @stime: time spent in system mode
 * @lock: protects the above two fields
 *
 * Stores previous user/system time values such that we can guarantee
 * monotonicity.
 */
struct prev_cputime {
#ifndef CONFIG_VIRT_CPU_ACCOUNTING_NATIVE
	u64				utime;
	u64				stime;
	raw_spinlock_t			lock;
#endif
};

/**
 * struct task_cputime - collected CPU time counts
 * @utime:		time spent in user mode, in nanoseconds
 * @stime:		time spent in kernel mode, in nanoseconds
 * @sum_exec_runtime:	total time spent on the CPU, in nanoseconds
 *
 * This structure groups together three kinds of CPU time that are tracked for
 * threads and thread groups.  Most things considering CPU time want to group
 * these counts together and treat all three of them in parallel.
 */
struct task_cputime {
	u64				utime;
	u64				stime;
	unsigned long long		sum_exec_runtime;
};

/* Alternate field names when used on cache expirations: */
#define virt_exp			utime
#define prof_exp			stime
#define sched_exp			sum_exec_runtime

struct sched_info {
#ifdef CONFIG_SCHED_INFO
	/* Cumulative counters: */

	/* # of times we have run on this CPU: */
	unsigned long			pcount;

	/* Time spent waiting on a runqueue: */
	unsigned long long		run_delay;

	/* Timestamps: */

	/* When did we last run on a CPU? */
	unsigned long long		last_arrival;

	/* When were we last queued to run? */
	unsigned long long		last_queued;

#endif /* CONFIG_SCHED_INFO */
};

/*
 * Integer metrics need fixed point arithmetic, e.g., sched/fair
 * has a few: load, load_avg, util_avg, freq, and capacity.
 *
 * We define a basic fixed point arithmetic range, and then formalize
 * all these metrics based on that basic range.
 */
# define SCHED_FIXEDPOINT_SHIFT		10
# define SCHED_FIXEDPOINT_SCALE		(1L << SCHED_FIXEDPOINT_SHIFT)

struct load_weight {
	unsigned long			weight;
	u32				inv_weight;
};

/*
 * The load_avg/util_avg accumulates an infinite geometric series
 * (see __update_load_avg() in kernel/sched/fair.c).
 *
 * [load_avg definition]
 *
 *   load_avg = runnable% * scale_load_down(load)
 *
 * where runnable% is the time ratio that a sched_entity is runnable.
 * For cfs_rq, it is the aggregated load_avg of all runnable and
 * blocked sched_entities.
 *
 * load_avg may also take frequency scaling into account:
 *
 *   load_avg = runnable% * scale_load_down(load) * freq%
 *
 * where freq% is the CPU frequency normalized to the highest frequency.
 *
 * [util_avg definition]
 *
 *   util_avg = running% * SCHED_CAPACITY_SCALE
 *
 * where running% is the time ratio that a sched_entity is running on
 * a CPU. For cfs_rq, it is the aggregated util_avg of all runnable
 * and blocked sched_entities.
 *
 * util_avg may also factor frequency scaling and CPU capacity scaling:
 *
 *   util_avg = running% * SCHED_CAPACITY_SCALE * freq% * capacity%
 *
 * where freq% is the same as above, and capacity% is the CPU capacity
 * normalized to the greatest capacity (due to uarch differences, etc).
 *
 * N.B., the above ratios (runnable%, running%, freq%, and capacity%)
 * themselves are in the range of [0, 1]. To do fixed point arithmetics,
 * we therefore scale them to as large a range as necessary. This is for
 * example reflected by util_avg's SCHED_CAPACITY_SCALE.
 *
 * [Overflow issue]
 *
 * The 64-bit load_sum can have 4353082796 (=2^64/47742/88761) entities
 * with the highest load (=88761), always runnable on a single cfs_rq,
 * and should not overflow as the number already hits PID_MAX_LIMIT.
 *
 * For all other cases (including 32-bit kernels), struct load_weight's
 * weight will overflow first before we do, because:
 *
 *    Max(load_avg) <= Max(load.weight)
 *
 * Then it is the load_weight's responsibility to consider overflow
 * issues.
 */
struct sched_avg {
	u64				last_update_time;
	u64				load_sum;
	u32				util_sum;
	u32				period_contrib;
	unsigned long			load_avg;
	unsigned long			util_avg;
};

struct sched_statistics {
#ifdef CONFIG_SCHEDSTATS
	u64				wait_start;
	u64				wait_max;
	u64				wait_count;
	u64				wait_sum;
	u64				iowait_count;
	u64				iowait_sum;

	u64				sleep_start;
	u64				sleep_max;
	s64				sum_sleep_runtime;

	u64				block_start;
	u64				block_max;
	u64				exec_max;
	u64				slice_max;

	u64				nr_migrations_cold;
	u64				nr_failed_migrations_affine;
	u64				nr_failed_migrations_running;
	u64				nr_failed_migrations_hot;
	u64				nr_forced_migrations;

	u64				nr_wakeups;
	u64				nr_wakeups_sync;
	u64				nr_wakeups_migrate;
	u64				nr_wakeups_local;
	u64				nr_wakeups_remote;
	u64				nr_wakeups_affine;
	u64				nr_wakeups_affine_attempts;
	u64				nr_wakeups_passive;
	u64				nr_wakeups_idle;
#endif
};

struct sched_entity {
	/* For load-balancing: */
	struct load_weight		load;
	struct rb_node			run_node;
	struct list_head		group_node;
	unsigned int			on_rq;

	u64				exec_start;
	u64				sum_exec_runtime;
	u64				vruntime;
	u64				prev_sum_exec_runtime;

	u64				nr_migrations;

	struct sched_statistics		statistics;

#ifdef CONFIG_FAIR_GROUP_SCHED
	int				depth;
	struct sched_entity *parent;
	/* rq on which this entity is (to be) queued: */
	struct cfs_rq *cfs_rq;
	/* rq "owned" by this entity/group: */
	struct cfs_rq *my_q;
#endif

#ifdef CONFIG_SMP
	/*
	 * Per entity load average tracking.
	 *
	 * Put into separate cache line so it does not
	 * collide with read-mostly values above.
	 */
	struct sched_avg		avg ____cacheline_aligned_in_smp;
#endif
};

struct sched_rt_entity {
	struct list_head		run_list;
	unsigned long			timeout;
	unsigned long			watchdog_stamp;
	unsigned int			time_slice;
	unsigned short			on_rq;
	unsigned short			on_list;

	struct sched_rt_entity *back;
#ifdef CONFIG_RT_GROUP_SCHED
	struct sched_rt_entity *parent;
	/* rq on which this entity is (to be) queued: */
	struct rt_rq *rt_rq;
	/* rq "owned" by this entity/group: */
	struct rt_rq *my_q;
#endif
};

struct sched_dl_entity {
	struct rb_node			rb_node;

	/*
	 * Original scheduling parameters. Copied here from sched_attr
	 * during sched_setattr(), they will remain the same until
	 * the next sched_setattr().
	 */
	u64				dl_runtime;	/* Maximum runtime for each instance	*/
	u64				dl_deadline;	/* Relative deadline of each instance	*/
	u64				dl_period;	/* Separation of two instances (period) */
	u64				dl_bw;		/* dl_runtime / dl_deadline		*/

	/*
	 * Actual scheduling parameters. Initialized with the values above,
	 * they are continously updated during task execution. Note that
	 * the remaining runtime could be < 0 in case we are in overrun.
	 */
	s64				runtime;	/* Remaining runtime for this instance	*/
	u64				deadline;	/* Absolute deadline for this instance	*/
	unsigned int			flags;		/* Specifying the scheduler behaviour	*/

	/*
	 * Some bool flags:
	 *
	 * @dl_throttled tells if we exhausted the runtime. If so, the
	 * task has to wait for a replenishment to be performed at the
	 * next firing of dl_timer.
	 *
	 * @dl_boosted tells if we are boosted due to DI. If so we are
	 * outside bandwidth enforcement mechanism (but only until we
	 * exit the critical section);
	 *
	 * @dl_yielded tells if task gave up the CPU before consuming
	 * all its available runtime during the last job.
	 */
	int				dl_throttled;
	int				dl_boosted;
	int				dl_yielded;

	/*
	 * Bandwidth enforcement timer. Each -deadline task has its
	 * own bandwidth to be enforced, thus we need one timer per task.
	 */
	struct hrtimer			dl_timer;
};

union rcu_special {
	struct {
		u8			blocked;
		u8			need_qs;
		u8			exp_need_qs;

		/* Otherwise the compiler can store garbage here: */
		u8			pad;
	} b; /* Bits. */
	u32 s; /* Set of bits. */
};

enum perf_event_task_context {
	perf_invalid_context = -1,
	perf_hw_context = 0,
	perf_sw_context,
	perf_nr_task_contexts,
};

struct wake_q_node {
	struct wake_q_node *next;
};

struct task_struct {
#ifdef CONFIG_THREAD_INFO_IN_TASK
	/*
	 * For reasons of header soup (see current_thread_info()), this
	 * must be the first element of task_struct.
	 */
	struct thread_info		thread_info;
#endif
	/* -1 unrunnable, 0 runnable, >0 stopped: */
	volatile long			state;  // 进程状态
	void *stack; // 内核栈指针
	atomic_t			usage; // 引用计数
	/* Per task flags (PF_*), defined further below: */
	unsigned int			flags; // 进程标志
	unsigned int			ptrace; // ptrace标志

#ifdef CONFIG_SMP
	struct llist_node		wake_entry;	// 唤醒链表
	int				on_cpu; // 是否在CPU上
#ifdef CONFIG_THREAD_INFO_IN_TASK
	/* Current CPU: */
	unsigned int			cpu; // 当前CPU
#endif
	unsigned int			wakee_flips; // 唤醒次数
	unsigned long			wakee_flip_decay_ts; // 唤醒时间
	struct task_struct *last_wakee; // 上次唤醒的进程

	int				wake_cpu; // 唤醒CPU
#endif
	int				on_rq; // 是否在就绪队列中

	int				prio; // 调度优先级
	int				static_prio; // 静态优先级
	int				normal_prio; // 正常优先级
	unsigned int			rt_priority; // 实时优先级

	const struct sched_class *sched_class; // 调度类
	struct sched_entity		se; // 公平调度实体
	struct sched_rt_entity		rt; // 实时调度实体
#ifdef CONFIG_CGROUP_SCHED
	struct task_group *sched_task_group;
#endif
	struct sched_dl_entity		dl; // 限期调度实体

#ifdef CONFIG_PREEMPT_NOTIFIERS
	/* List of struct preempt_notifier: */
	struct hlist_head		preempt_notifiers; // 抢占通知链表
#endif

#ifdef CONFIG_BLK_DEV_IO_TRACE
	unsigned int			btrace_seq; // 块IO跟踪序列号
#endif

	unsigned int			policy; // 调度策略
	int				nr_cpus_allowed; // 允许运行的CPU个数
	cpumask_t			cpus_allowed; // 允许运行的CPU掩码

#ifdef CONFIG_PREEMPT_RCU
	int				rcu_read_lock_nesting; // rcu读锁嵌套
	union rcu_special		rcu_read_unlock_special; // rcu读锁特殊
	struct list_head		rcu_node_entry; // rcu节点链表
	struct rcu_node *rcu_blocked_node; // rcu阻塞节点
#endif /* #ifdef CONFIG_PREEMPT_RCU */

#ifdef CONFIG_TASKS_RCU
	unsigned long			rcu_tasks_nvcsw; // rcu任务nvcsw
	bool				rcu_tasks_holdout; // rcu任务holdout
	struct list_head		rcu_tasks_holdout_list; // rcu任务holdout链表
	int				rcu_tasks_idle_cpu; // rcu任务空闲CPU
#endif /* #ifdef CONFIG_TASKS_RCU */

	struct sched_info		sched_info; // 调度信息

	struct list_head		tasks; // 系统中所有进程和线程双向链表，当前进程结点，可用next和prev访问下一个和上一个进程
#ifdef CONFIG_SMP
	struct plist_node		pushable_tasks;
	struct rb_node			pushable_dl_tasks;
#endif

	struct mm_struct *mm; // 用户虚拟地址空间，内核线程的此字段为空指针
	struct mm_struct *active_mm; // 用户活跃虚地址空间，和mm一致；内核线程在没有运行时是空指针，在运行时指向从上一个进程借用的引用计数+1的空间

	/* Per-thread vma caching: */
	struct vmacache			vmacache; // vma缓存

#ifdef SPLIT_RSS_COUNTING
	struct task_rss_stat		rss_stat;
#endif
	int				exit_state; // 退出状态
	int				exit_code; // 退出码
	int				exit_signal; // 退出信号
	/* The signal sent when the parent dies: */
	int				pdeath_signal; // 父进程死亡时发送的信号
	/* JOBCTL_*, siglock protected: */
	unsigned long			jobctl; // 作业控制

	/* Used for emulating ABI behavior of previous Linux versions: */
	unsigned int			personality;

	/* Scheduler bits, serialized by scheduler locks: */
	unsigned			sched_reset_on_fork : 1; // fork时重置调度器
	unsigned			sched_contributes_to_load : 1; // 是否贡献负载
	unsigned			sched_migrated : 1; // 是否迁移
	unsigned			sched_remote_wakeup : 1; // 远程唤醒
	/* Force alignment to the next boundary: */
	unsigned : 0; // 强制对齐

	/* Unserialized, strictly 'current' */

	/* Bit to tell LSMs we're in execve(): */
	unsigned			in_execve : 1; // 是否在执行execve
	unsigned			in_iowait : 1; // 是否在io等待
#ifndef TIF_RESTORE_SIGMASK
	unsigned			restore_sigmask : 1;
#endif
#ifdef CONFIG_MEMCG
	unsigned			memcg_may_oom : 1;
#ifndef CONFIG_SLOB
	unsigned			memcg_kmem_skip_account : 1;
#endif
#endif
#ifdef CONFIG_COMPAT_BRK
	unsigned			brk_randomized : 1;
#endif
#ifdef CONFIG_CGROUPS
	/* disallow userland-initiated cgroup migration */
	unsigned			no_cgroup_migration : 1;
#endif

	unsigned long			atomic_flags; /* Flags requiring atomic access. */

	struct restart_block		restart_block; // 重启块

	pid_t				pid; // 进程ID
	pid_t				tgid; // 线程组ID，线程组领导的PID（ group_leader ）

#ifdef CONFIG_CC_STACKPROTECTOR
	/* Canary value for the -fstack-protector GCC feature: */
	unsigned long			stack_canary;
#endif
	/*
	 * Pointers to the (original) parent process, youngest child, younger sibling,
	 * older sibling, respectively.  (p->father can be replaced with
	 * p->real_parent->pid)
	 */

	 /* Real parent process: */
	struct task_struct __rcu *real_parent; // 真实父进程

	/* Recipient of SIGCHLD, wait4() reports: */
	struct task_struct __rcu *parent; // 父进程

	/*
	 * Children/sibling form the list of natural children:
	 */
	struct list_head		children; // 子进程链表
	struct list_head		sibling; // 兄弟进程链表
	struct task_struct *group_leader; // 线程组领导的进程指针，指向线程组的领导进程

	/*
	 * 'ptraced' is the list of tasks this task is using ptrace() on.
	 *
	 * This includes both natural children and PTRACE_ATTACH targets.
	 * 'ptrace_entry' is this task's link on the p->parent->ptraced list.
	 */
	struct list_head		ptraced; // 被ptrace的进程链表
	struct list_head		ptrace_entry; // ptrace链表

	/* PID/PID hash table linkage. */
	struct pid_link			pids[PIDTYPE_MAX]; // 进程号，进程组标识符（组长的进程标识符），会话标识符 PIDTYPE_PID, PIDTYPE_PGID, PIDTYPE_SID,
	struct list_head		thread_group; // 双向线程组链表，当前进程所在的线程组链表节点，可用next和prev访问下一个和上一个线程组
	struct list_head		thread_node; // 线程链表

	struct completion *vfork_done; // vfork完成事件

	/* CLONE_CHILD_SETTID: */
	int __user *set_child_tid; // 设置子进程TID

	/* CLONE_CHILD_CLEARTID: */
	int __user *clear_child_tid; // 清除子进程TID

	u64				utime; // 用户态运行时间
	u64				stime; // 内核态运行时间
#ifdef CONFIG_ARCH_HAS_SCALED_CPUTIME
	u64				utimescaled; // 用户态运行时间
	u64				stimescaled;  // 内核态运行时间
#endif
	u64				gtime; // 系统态运行时间
	struct prev_cputime		prev_cputime; // 上次CPU时间
#ifdef CONFIG_VIRT_CPU_ACCOUNTING_GEN
	seqcount_t			vtime_seqcount;
	unsigned long long		vtime_snap;
	enum {
		/* Task is sleeping or running in a CPU with VTIME inactive: */
		VTIME_INACTIVE = 0,
		/* Task runs in userspace in a CPU with VTIME active: */
		VTIME_USER,
		/* Task runs in kernelspace in a CPU with VTIME active: */
		VTIME_SYS,
	} vtime_snap_whence;
#endif

#ifdef CONFIG_NO_HZ_FULL
	atomic_t			tick_dep_mask;
#endif
	/* Context switch counts: */
	unsigned long			nvcsw; // 非自愿上下文切换次数
	unsigned long			nivcsw; // 自愿上下文切换次数

	/* Monotonic time in nsecs: */
	u64				start_time; // 启动时间

	/* Boot based time in nsecs: */
	u64				real_start_time; // 真实启动时间

	/* MM fault and swap info: this can arguably be seen as either mm-specific or thread-specific: */
	unsigned long			min_flt; // 未映射的页面错误次数
	unsigned long			maj_flt; // 映射的页面错误次数

#ifdef CONFIG_POSIX_TIMERS
	struct task_cputime		cputime_expires;
	struct list_head		cpu_timers[3];
#endif

	/* Process credentials: */

	/* Tracer's credentials at attach: */
	const struct cred __rcu *ptracer_cred; // 跟踪者的凭证

	/* Objective and real subjective task credentials (COW): */
	const struct cred __rcu *real_cred; // 真实凭证

	/* Effective (overridable) subjective task credentials (COW): */
	const struct cred __rcu *cred; // 凭证

	/*
	 * executable name, excluding path.
	 *
	 * - normally initialized setup_new_exec()
	 * - access it with get_task_comm()
	 * - lock it with task_lock()
	 */
	char				comm[TASK_COMM_LEN]; // 进程名

	struct nameidata *nameidata; // 名字数据

#ifdef CONFIG_SYSVIPC
	struct sysv_sem			sysvsem;
	struct sysv_shm			sysvshm;
#endif
#ifdef CONFIG_DETECT_HUNG_TASK
	unsigned long			last_switch_count;
#endif
	/* Filesystem information: */
	struct fs_struct *fs; // 文件系统信息，主要是当前根目录和工作目录

	/* Open file information: */
	struct files_struct *files; // 打开文件表

	/* Namespaces: */
	struct nsproxy *nsproxy; // 命名空间

	/* Signal handlers: */
	struct signal_struct *signal; // 信号
	struct sighand_struct *sighand; // 信号处理
	sigset_t			blocked; // 阻塞信号
	sigset_t			real_blocked; // 真实阻塞信号
	/* Restored if set_restore_sigmask() was used: */
	sigset_t			saved_sigmask; // 保存的信号掩码
	struct sigpending		pending; // 未决信号, 每个信号在进程中注册都会把信号值加入到进程的未决信号集
	unsigned long			sas_ss_sp; // 信号栈指针
	size_t				sas_ss_size; // 信号栈大小
	unsigned int			sas_ss_flags; // 信号栈标志

	struct callback_head *task_works;

	struct audit_context *audit_context;
#ifdef CONFIG_AUDITSYSCALL
	kuid_t				loginuid;
	unsigned int			sessionid;
#endif
	struct seccomp			seccomp;

	/* Thread group tracking: */
	u32				parent_exec_id;
	u32				self_exec_id;

	/* Protection against (de-)allocation: mm, files, fs, tty, keyrings, mems_allowed, mempolicy: */
	spinlock_t			alloc_lock;

	/* Protection of the PI data structures: */
	raw_spinlock_t			pi_lock;

	struct wake_q_node		wake_q;

#ifdef CONFIG_RT_MUTEXES
	/* PI waiters blocked on a rt_mutex held by this task: */
	struct rb_root			pi_waiters;
	struct rb_node *pi_waiters_leftmost;
	/* Updated under owner's pi_lock and rq lock */
	struct task_struct *pi_top_task;
	/* Deadlock detection and priority inheritance handling: */
	struct rt_mutex_waiter *pi_blocked_on;
#endif

#ifdef CONFIG_DEBUG_MUTEXES
	/* Mutex deadlock detection: */
	struct mutex_waiter *blocked_on;
#endif

#ifdef CONFIG_TRACE_IRQFLAGS
	unsigned int			irq_events;
	unsigned long			hardirq_enable_ip;
	unsigned long			hardirq_disable_ip;
	unsigned int			hardirq_enable_event;
	unsigned int			hardirq_disable_event;
	int				hardirqs_enabled;
	int				hardirq_context;
	unsigned long			softirq_disable_ip;
	unsigned long			softirq_enable_ip;
	unsigned int			softirq_disable_event;
	unsigned int			softirq_enable_event;
	int				softirqs_enabled;
	int				softirq_context;
#endif

#ifdef CONFIG_LOCKDEP
# define MAX_LOCK_DEPTH			48UL
	u64				curr_chain_key;
	int				lockdep_depth;
	unsigned int			lockdep_recursion;
	struct held_lock		held_locks[MAX_LOCK_DEPTH];
	gfp_t				lockdep_reclaim_gfp;
#endif

#ifdef CONFIG_UBSAN
	unsigned int			in_ubsan;
#endif

	/* Journalling filesystem info: */
	void *journal_info;

	/* Stacked block device info: */
	struct bio_list *bio_list;

#ifdef CONFIG_BLOCK
	/* Stack plugging: */
	struct blk_plug *plug;
#endif

	/* VM state: */
	struct reclaim_state *reclaim_state;

	struct backing_dev_info *backing_dev_info;

	struct io_context *io_context;

	/* Ptrace state: */
	unsigned long			ptrace_message;
	siginfo_t *last_siginfo;

	struct task_io_accounting	ioac;
#ifdef CONFIG_TASK_XACCT
	/* Accumulated RSS usage: */
	u64				acct_rss_mem1;
	/* Accumulated virtual memory usage: */
	u64				acct_vm_mem1;
	/* stime + utime since last update: */
	u64				acct_timexpd;
#endif
#ifdef CONFIG_CPUSETS
	/* Protected by ->alloc_lock: */
	nodemask_t			mems_allowed;
	/* Seqence number to catch updates: */
	seqcount_t			mems_allowed_seq;
	int				cpuset_mem_spread_rotor;
	int				cpuset_slab_spread_rotor;
#endif
#ifdef CONFIG_CGROUPS
	/* Control Group info protected by css_set_lock: */
	struct css_set __rcu *cgroups;
	/* cg_list protected by css_set_lock and tsk->alloc_lock: */
	struct list_head		cg_list;
#endif
#ifdef CONFIG_INTEL_RDT_A
	int				closid;
#endif
#ifdef CONFIG_FUTEX
	struct robust_list_head __user *robust_list;
#ifdef CONFIG_COMPAT
	struct compat_robust_list_head __user *compat_robust_list;
#endif
	struct list_head		pi_state_list;
	struct futex_pi_state *pi_state_cache;
#endif
#ifdef CONFIG_PERF_EVENTS
	struct perf_event_context *perf_event_ctxp[perf_nr_task_contexts];
	struct mutex			perf_event_mutex;
	struct list_head		perf_event_list;
#endif
#ifdef CONFIG_DEBUG_PREEMPT
	unsigned long			preempt_disable_ip;
#endif
#ifdef CONFIG_NUMA
	/* Protected by alloc_lock: */
	struct mempolicy *mempolicy;
	short				il_next;
	short				pref_node_fork;
#endif
#ifdef CONFIG_NUMA_BALANCING
	int				numa_scan_seq;
	unsigned int			numa_scan_period;
	unsigned int			numa_scan_period_max;
	int				numa_preferred_nid;
	unsigned long			numa_migrate_retry;
	/* Migration stamp: */
	u64				node_stamp;
	u64				last_task_numa_placement;
	u64				last_sum_exec_runtime;
	struct callback_head		numa_work;

	struct list_head		numa_entry;
	struct numa_group *numa_group;

	/*
	 * numa_faults is an array split into four regions:
	 * faults_memory, faults_cpu, faults_memory_buffer, faults_cpu_buffer
	 * in this precise order.
	 *
	 * faults_memory: Exponential decaying average of faults on a per-node
	 * basis. Scheduling placement decisions are made based on these
	 * counts. The values remain static for the duration of a PTE scan.
	 * faults_cpu: Track the nodes the process was running on when a NUMA
	 * hinting fault was incurred.
	 * faults_memory_buffer and faults_cpu_buffer: Record faults per node
	 * during the current scan window. When the scan completes, the counts
	 * in faults_memory and faults_cpu decay and these values are copied.
	 */
	unsigned long *numa_faults;
	unsigned long			total_numa_faults;

	/*
	 * numa_faults_locality tracks if faults recorded during the last
	 * scan window were remote/local or failed to migrate. The task scan
	 * period is adapted based on the locality of the faults with different
	 * weights depending on whether they were shared or private faults
	 */
	unsigned long			numa_faults_locality[3];

	unsigned long			numa_pages_migrated;
#endif /* CONFIG_NUMA_BALANCING */

	struct tlbflush_unmap_batch	tlb_ubc;

	struct rcu_head			rcu;

	/* Cache last used pipe for splice(): */
	struct pipe_inode_info *splice_pipe;

	struct page_frag		task_frag;

#ifdef CONFIG_TASK_DELAY_ACCT
	struct task_delay_info *delays;
#endif

#ifdef CONFIG_FAULT_INJECTION
	int				make_it_fail;
#endif
	/*
	 * When (nr_dirtied >= nr_dirtied_pause), it's time to call
	 * balance_dirty_pages() for a dirty throttling pause:
	 */
	int				nr_dirtied;
	int				nr_dirtied_pause;
	/* Start of a write-and-pause period: */
	unsigned long			dirty_paused_when;

#ifdef CONFIG_LATENCYTOP
	int				latency_record_count;
	struct latency_record		latency_record[LT_SAVECOUNT];
#endif
	/*
	 * Time slack values; these are used to round up poll() and
	 * select() etc timeout values. These are in nanoseconds.
	 */
	u64				timer_slack_ns;
	u64				default_timer_slack_ns;

#ifdef CONFIG_KASAN
	unsigned int			kasan_depth;
#endif

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	/* Index of current stored address in ret_stack: */
	int				curr_ret_stack;

	/* Stack of return addresses for return function tracing: */
	struct ftrace_ret_stack *ret_stack;

	/* Timestamp for last schedule: */
	unsigned long long		ftrace_timestamp;

	/*
	 * Number of functions that haven't been traced
	 * because of depth overrun:
	 */
	atomic_t			trace_overrun;

	/* Pause tracing: */
	atomic_t			tracing_graph_pause;
#endif

#ifdef CONFIG_TRACING
	/* State flags for use by tracers: */
	unsigned long			trace;

	/* Bitmask and counter of trace recursion: */
	unsigned long			trace_recursion;
#endif /* CONFIG_TRACING */

#ifdef CONFIG_KCOV
	/* Coverage collection mode enabled for this task (0 if disabled): */
	enum kcov_mode			kcov_mode;

	/* Size of the kcov_area: */
	unsigned int			kcov_size;

	/* Buffer for coverage collection: */
	void *kcov_area;

	/* KCOV descriptor wired with this task or NULL: */
	struct kcov *kcov;
#endif

#ifdef CONFIG_MEMCG
	struct mem_cgroup *memcg_in_oom;
	gfp_t				memcg_oom_gfp_mask;
	int				memcg_oom_order;

	/* Number of pages to reclaim on returning to userland: */
	unsigned int			memcg_nr_pages_over_high;
#endif

#ifdef CONFIG_UPROBES
	struct uprobe_task *utask;
#endif
#if defined(CONFIG_BCACHE) || defined(CONFIG_BCACHE_MODULE)
	unsigned int			sequential_io;
	unsigned int			sequential_io_avg;
#endif
#ifdef CONFIG_DEBUG_ATOMIC_SLEEP
	unsigned long			task_state_change;
#endif
	int				pagefault_disabled;
#ifdef CONFIG_MMU
	struct task_struct *oom_reaper_list;
#endif
#ifdef CONFIG_VMAP_STACK
	struct vm_struct *stack_vm_area;
#endif
#ifdef CONFIG_THREAD_INFO_IN_TASK
	/* A live task holds one reference: */
	atomic_t			stack_refcount;
#endif
#ifdef CONFIG_LIVEPATCH
	int patch_state;
#endif
#ifdef CONFIG_SECURITY
	/* Used by LSM modules for access restriction: */
	void *security;
#endif
	/* CPU-specific state of this task: */
	struct thread_struct		thread;

	/*
	 * WARNING: on x86, 'thread_struct' contains a variable-sized
	 * structure.  It *MUST* be at the end of 'task_struct'.
	 *
	 * Do not put anything below here!
	 */
};

static inline struct pid *task_pid(struct task_struct *task) {
	return task->pids[PIDTYPE_PID].pid;
}

static inline struct pid *task_tgid(struct task_struct *task) {
	return task->group_leader->pids[PIDTYPE_PID].pid;
}

/*
 * Without tasklist or RCU lock it is not safe to dereference
 * the result of task_pgrp/task_session even if task == current,
 * we can race with another thread doing sys_setsid/sys_setpgid.
 */
static inline struct pid *task_pgrp(struct task_struct *task) {
	return task->group_leader->pids[PIDTYPE_PGID].pid;
}

static inline struct pid *task_session(struct task_struct *task) {
	return task->group_leader->pids[PIDTYPE_SID].pid;
}

/*
 * the helpers to get the task's different pids as they are seen
 * from various namespaces
 *
 * task_xid_nr()     : global id, i.e. the id seen from the init namespace;
 * task_xid_vnr()    : virtual id, i.e. the id seen from the pid namespace of
 *                     current.
 * task_xid_nr_ns()  : id seen from the ns specified;
 *
 * set_task_vxid()   : assigns a virtual id to a task;
 *
 * see also pid_nr() etc in include/linux/pid.h
 */
pid_t __task_pid_nr_ns(struct task_struct *task, enum pid_type type, struct pid_namespace *ns);

static inline pid_t task_pid_nr(struct task_struct *tsk) {
	return tsk->pid;
}

static inline pid_t task_pid_nr_ns(struct task_struct *tsk, struct pid_namespace *ns) {
	return __task_pid_nr_ns(tsk, PIDTYPE_PID, ns);
}

static inline pid_t task_pid_vnr(struct task_struct *tsk) {
	return __task_pid_nr_ns(tsk, PIDTYPE_PID, NULL);
}


static inline pid_t task_tgid_nr(struct task_struct *tsk) {
	return tsk->tgid;
}

extern pid_t task_tgid_nr_ns(struct task_struct *tsk, struct pid_namespace *ns);

static inline pid_t task_tgid_vnr(struct task_struct *tsk) {
	return pid_vnr(task_tgid(tsk));
}

/**
 * pid_alive - check that a task structure is not stale
 * @p: Task structure to be checked.
 *
 * Test if a process is not yet dead (at most zombie state)
 * If pid_alive fails, then pointers within the task structure
 * can be stale and must not be dereferenced.
 *
 * Return: 1 if the process is alive. 0 otherwise.
 */
static inline int pid_alive(const struct task_struct *p) {
	return p->pids[PIDTYPE_PID].pid != NULL;
}

static inline pid_t task_ppid_nr_ns(const struct task_struct *tsk, struct pid_namespace *ns) {
	pid_t pid = 0;

	rcu_read_lock();
	if (pid_alive(tsk))
		pid = task_tgid_nr_ns(rcu_dereference(tsk->real_parent), ns);
	rcu_read_unlock();

	return pid;
}

static inline pid_t task_ppid_nr(const struct task_struct *tsk) {
	return task_ppid_nr_ns(tsk, &init_pid_ns);
}

static inline pid_t task_pgrp_nr_ns(struct task_struct *tsk, struct pid_namespace *ns) {
	return __task_pid_nr_ns(tsk, PIDTYPE_PGID, ns);
}

static inline pid_t task_pgrp_vnr(struct task_struct *tsk) {
	return __task_pid_nr_ns(tsk, PIDTYPE_PGID, NULL);
}


static inline pid_t task_session_nr_ns(struct task_struct *tsk, struct pid_namespace *ns) {
	return __task_pid_nr_ns(tsk, PIDTYPE_SID, ns);
}

static inline pid_t task_session_vnr(struct task_struct *tsk) {
	return __task_pid_nr_ns(tsk, PIDTYPE_SID, NULL);
}

/* Obsolete, do not use: */
static inline pid_t task_pgrp_nr(struct task_struct *tsk) {
	return task_pgrp_nr_ns(tsk, &init_pid_ns);
}

/**
 * is_global_init - check if a task structure is init. Since init
 * is free to have sub-threads we need to check tgid.
 * @tsk: Task structure to be checked.
 *
 * Check if a task structure is the first user space task the kernel created.
 *
 * Return: 1 if the task structure is init. 0 otherwise.
 */
static inline int is_global_init(struct task_struct *tsk) {
	return task_tgid_nr(tsk) == 1;
}

extern struct pid *cad_pid;

/*
 * Per process flags
 */
#define PF_IDLE			0x00000002	/* I am an IDLE thread */
#define PF_EXITING		0x00000004	/* Getting shut down */
#define PF_EXITPIDONE		0x00000008	/* PI exit done on shut down */
#define PF_VCPU			0x00000010	/* I'm a virtual CPU */
#define PF_WQ_WORKER		0x00000020	/* I'm a workqueue worker */
#define PF_FORKNOEXEC		0x00000040	/* Forked but didn't exec */
#define PF_MCE_PROCESS		0x00000080      /* Process policy on mce errors */
#define PF_SUPERPRIV		0x00000100	/* Used super-user privileges */
#define PF_DUMPCORE		0x00000200	/* Dumped core */
#define PF_SIGNALED		0x00000400	/* Killed by a signal */
#define PF_MEMALLOC		0x00000800	/* Allocating memory */
#define PF_NPROC_EXCEEDED	0x00001000	/* set_user() noticed that RLIMIT_NPROC was exceeded */
#define PF_USED_MATH		0x00002000	/* If unset the fpu must be initialized before use */
#define PF_USED_ASYNC		0x00004000	/* Used async_schedule*(), used by module init */
#define PF_NOFREEZE		0x00008000	/* This thread should not be frozen */
#define PF_FROZEN		0x00010000	/* Frozen for system suspend */
#define PF_KSWAPD		0x00020000	/* I am kswapd */
#define PF_MEMALLOC_NOFS	0x00040000	/* All allocation requests will inherit GFP_NOFS */
#define PF_MEMALLOC_NOIO	0x00080000	/* All allocation requests will inherit GFP_NOIO */
#define PF_LESS_THROTTLE	0x00100000	/* Throttle me less: I clean memory */
#define PF_KTHREAD		0x00200000	/* I am a kernel thread */
#define PF_RANDOMIZE		0x00400000	/* Randomize virtual address space */
#define PF_SWAPWRITE		0x00800000	/* Allowed to write to swap */
#define PF_NO_SETAFFINITY	0x04000000	/* Userland is not allowed to meddle with cpus_allowed */
#define PF_MCE_EARLY		0x08000000      /* Early kill for mce process policy */
#define PF_MUTEX_TESTER		0x20000000	/* Thread belongs to the rt mutex tester */
#define PF_FREEZER_SKIP		0x40000000	/* Freezer should not count it as freezable */
#define PF_SUSPEND_TASK		0x80000000      /* This thread called freeze_processes() and should not be frozen */

 /*
  * Only the _current_ task can read/write to tsk->flags, but other
  * tasks can access tsk->flags in readonly mode for example
  * with tsk_used_math (like during threaded core dumping).
  * There is however an exception to this rule during ptrace
  * or during fork: the ptracer task is allowed to write to the
  * child->flags of its traced child (same goes for fork, the parent
  * can write to the child->flags), because we're guaranteed the
  * child is not running and in turn not changing child->flags
  * at the same time the parent does it.
  */
#define clear_stopped_child_used_math(child)	do { (child)->flags &= ~PF_USED_MATH; } while (0)
#define set_stopped_child_used_math(child)	do { (child)->flags |= PF_USED_MATH; } while (0)
#define clear_used_math()			clear_stopped_child_used_math(current)
#define set_used_math()				set_stopped_child_used_math(current)

#define conditional_stopped_child_used_math(condition, child) \
	do { (child)->flags &= ~PF_USED_MATH, (child)->flags |= (condition) ? PF_USED_MATH : 0; } while (0)

#define conditional_used_math(condition)	conditional_stopped_child_used_math(condition, current)

#define copy_to_stopped_child_used_math(child) \
	do { (child)->flags &= ~PF_USED_MATH, (child)->flags |= current->flags & PF_USED_MATH; } while (0)

  /* NOTE: this will return 0 or PF_USED_MATH, it will never return 1 */
#define tsk_used_math(p)			((p)->flags & PF_USED_MATH)
#define used_math()				tsk_used_math(current)

/* Per-process atomic flags. */
#define PFA_NO_NEW_PRIVS		0	/* May not gain new privileges. */
#define PFA_SPREAD_PAGE			1	/* Spread page cache over cpuset */
#define PFA_SPREAD_SLAB			2	/* Spread some slab caches over cpuset */


#define TASK_PFA_TEST(name, func)					\
	static inline bool task_##func(struct task_struct *p)		\
	{ return test_bit(PFA_##name, &p->atomic_flags); }

#define TASK_PFA_SET(name, func)					\
	static inline void task_set_##func(struct task_struct *p)	\
	{ set_bit(PFA_##name, &p->atomic_flags); }

#define TASK_PFA_CLEAR(name, func)					\
	static inline void task_clear_##func(struct task_struct *p)	\
	{ clear_bit(PFA_##name, &p->atomic_flags); }

TASK_PFA_TEST(NO_NEW_PRIVS, no_new_privs)
TASK_PFA_SET(NO_NEW_PRIVS, no_new_privs)

TASK_PFA_TEST(SPREAD_PAGE, spread_page)
TASK_PFA_SET(SPREAD_PAGE, spread_page)
TASK_PFA_CLEAR(SPREAD_PAGE, spread_page)

TASK_PFA_TEST(SPREAD_SLAB, spread_slab)
TASK_PFA_SET(SPREAD_SLAB, spread_slab)
TASK_PFA_CLEAR(SPREAD_SLAB, spread_slab)

static inline void
current_restore_flags(unsigned long orig_flags, unsigned long flags) {
	current->flags &= ~flags;
	current->flags |= orig_flags & flags;
}

extern int cpuset_cpumask_can_shrink(const struct cpumask *cur, const struct cpumask *trial);
extern int task_can_attach(struct task_struct *p, const struct cpumask *cs_cpus_allowed);
#ifdef CONFIG_SMP
extern void do_set_cpus_allowed(struct task_struct *p, const struct cpumask *new_mask);
extern int set_cpus_allowed_ptr(struct task_struct *p, const struct cpumask *new_mask);
#else
static inline void do_set_cpus_allowed(struct task_struct *p, const struct cpumask *new_mask) {
}
static inline int set_cpus_allowed_ptr(struct task_struct *p, const struct cpumask *new_mask) {
	if (!cpumask_test_cpu(0, new_mask))
		return -EINVAL;
	return 0;
}
#endif

#ifndef cpu_relax_yield
#define cpu_relax_yield() cpu_relax()
#endif

extern int yield_to(struct task_struct *p, bool preempt);
extern void set_user_nice(struct task_struct *p, long nice);
extern int task_prio(const struct task_struct *p);

/**
 * task_nice - return the nice value of a given task.
 * @p: the task in question.
 *
 * Return: The nice value [ -20 ... 0 ... 19 ].
 */
static inline int task_nice(const struct task_struct *p) {
	return PRIO_TO_NICE((p)->static_prio);
}

extern int can_nice(const struct task_struct *p, const int nice);
extern int task_curr(const struct task_struct *p);
extern int idle_cpu(int cpu);
extern int sched_setscheduler(struct task_struct *, int, const struct sched_param *);
extern int sched_setscheduler_nocheck(struct task_struct *, int, const struct sched_param *);
extern int sched_setattr(struct task_struct *, const struct sched_attr *);
extern struct task_struct *idle_task(int cpu);

/**
 * is_idle_task - is the specified task an idle task?
 * @p: the task in question.
 *
 * Return: 1 if @p is an idle task. 0 otherwise.
 */
static inline bool is_idle_task(const struct task_struct *p) {
	return !!(p->flags & PF_IDLE);
}

extern struct task_struct *curr_task(int cpu);
extern void ia64_set_curr_task(int cpu, struct task_struct *p);

void yield(void);

union thread_union {
#ifndef CONFIG_THREAD_INFO_IN_TASK
	struct thread_info thread_info;
#endif
	unsigned long stack[THREAD_SIZE / sizeof(long)];
};

#ifdef CONFIG_THREAD_INFO_IN_TASK
static inline struct thread_info *task_thread_info(struct task_struct *task) {
	return &task->thread_info;
}
#elif !defined(__HAVE_THREAD_FUNCTIONS)
# define task_thread_info(task)	((struct thread_info *)(task)->stack)
#endif

/*
 * find a task by one of its numerical ids
 *
 * find_task_by_pid_ns():
 *      finds a task by its pid in the specified namespace
 * find_task_by_vpid():
 *      finds a task by its virtual pid
 *
 * see also find_vpid() etc in include/linux/pid.h
 */

extern struct task_struct *find_task_by_vpid(pid_t nr);
extern struct task_struct *find_task_by_pid_ns(pid_t nr, struct pid_namespace *ns);

extern int wake_up_state(struct task_struct *tsk, unsigned int state);
extern int wake_up_process(struct task_struct *tsk);
extern void wake_up_new_task(struct task_struct *tsk);

#ifdef CONFIG_SMP
extern void kick_process(struct task_struct *tsk);
#else
static inline void kick_process(struct task_struct *tsk) {}
#endif

extern void __set_task_comm(struct task_struct *tsk, const char *from, bool exec);

static inline void set_task_comm(struct task_struct *tsk, const char *from) {
	__set_task_comm(tsk, from, false);
}

extern char *get_task_comm(char *to, struct task_struct *tsk);

#ifdef CONFIG_SMP
void scheduler_ipi(void);
extern unsigned long wait_task_inactive(struct task_struct *, long match_state);
#else
static inline void scheduler_ipi(void) {}
static inline unsigned long wait_task_inactive(struct task_struct *p, long match_state) {
	return 1;
}
#endif

/*
 * Set thread flags in other task's structures.
 * See asm/thread_info.h for TIF_xxxx flags available:
 */
static inline void set_tsk_thread_flag(struct task_struct *tsk, int flag) {
	set_ti_thread_flag(task_thread_info(tsk), flag);
}

static inline void clear_tsk_thread_flag(struct task_struct *tsk, int flag) {
	clear_ti_thread_flag(task_thread_info(tsk), flag);
}

static inline int test_and_set_tsk_thread_flag(struct task_struct *tsk, int flag) {
	return test_and_set_ti_thread_flag(task_thread_info(tsk), flag);
}

static inline int test_and_clear_tsk_thread_flag(struct task_struct *tsk, int flag) {
	return test_and_clear_ti_thread_flag(task_thread_info(tsk), flag);
}

static inline int test_tsk_thread_flag(struct task_struct *tsk, int flag) {
	return test_ti_thread_flag(task_thread_info(tsk), flag);
}

static inline void set_tsk_need_resched(struct task_struct *tsk) {
	set_tsk_thread_flag(tsk, TIF_NEED_RESCHED);
}

static inline void clear_tsk_need_resched(struct task_struct *tsk) {
	clear_tsk_thread_flag(tsk, TIF_NEED_RESCHED);
}

static inline int test_tsk_need_resched(struct task_struct *tsk) {
	return unlikely(test_tsk_thread_flag(tsk, TIF_NEED_RESCHED));
}

/*
 * cond_resched() and cond_resched_lock(): latency reduction via
 * explicit rescheduling in places that are safe. The return
 * value indicates whether a reschedule was done in fact.
 * cond_resched_lock() will drop the spinlock before scheduling,
 * cond_resched_softirq() will enable bhs before scheduling.
 */
#ifndef CONFIG_PREEMPT
extern int _cond_resched(void);
#else
static inline int _cond_resched(void) { return 0; }
#endif

#define cond_resched() ({			\
	___might_sleep(__FILE__, __LINE__, 0);	\
	_cond_resched();			\
})

extern int __cond_resched_lock(spinlock_t *lock);

#define cond_resched_lock(lock) ({				\
	___might_sleep(__FILE__, __LINE__, PREEMPT_LOCK_OFFSET);\
	__cond_resched_lock(lock);				\
})

extern int __cond_resched_softirq(void);

#define cond_resched_softirq() ({					\
	___might_sleep(__FILE__, __LINE__, SOFTIRQ_DISABLE_OFFSET);	\
	__cond_resched_softirq();					\
})

static inline void cond_resched_rcu(void) {
#if defined(CONFIG_DEBUG_ATOMIC_SLEEP) || !defined(CONFIG_PREEMPT_RCU)
	rcu_read_unlock();
	cond_resched();
	rcu_read_lock();
#endif
}

/*
 * Does a critical section need to be broken due to another
 * task waiting?: (technically does not depend on CONFIG_PREEMPT,
 * but a general need for low latency)
 */
static inline int spin_needbreak(spinlock_t *lock) {
#ifdef CONFIG_PREEMPT
	return spin_is_contended(lock);
#else
	return 0;
#endif
}

static __always_inline bool need_resched(void) {
	return unlikely(tif_need_resched());
}

/*
 * Wrappers for p->thread_info->cpu access. No-op on UP.
 */
#ifdef CONFIG_SMP

static inline unsigned int task_cpu(const struct task_struct *p) {
#ifdef CONFIG_THREAD_INFO_IN_TASK
	return p->cpu;
#else
	return task_thread_info(p)->cpu;
#endif
}

extern void set_task_cpu(struct task_struct *p, unsigned int cpu);

#else

static inline unsigned int task_cpu(const struct task_struct *p) {
	return 0;
}

static inline void set_task_cpu(struct task_struct *p, unsigned int cpu) {
}

#endif /* CONFIG_SMP */

/*
 * In order to reduce various lock holder preemption latencies provide an
 * interface to see if a vCPU is currently running or not.
 *
 * This allows us to terminate optimistic spin loops and block, analogous to
 * the native optimistic spin heuristic of testing if the lock owner task is
 * running or not.
 */
#ifndef vcpu_is_preempted
# define vcpu_is_preempted(cpu)	false
#endif

extern long sched_setaffinity(pid_t pid, const struct cpumask *new_mask);
extern long sched_getaffinity(pid_t pid, struct cpumask *mask);

#ifndef TASK_SIZE_OF
#define TASK_SIZE_OF(tsk)	TASK_SIZE
#endif

#endif
