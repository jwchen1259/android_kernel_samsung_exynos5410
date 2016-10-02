#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sort.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

#include <mach/cpufreq.h>
#include <linux/suspend.h>

#undef DEBUG_LOAD
#undef DEBUG_HOTPLUG

#define	COLD_THRESHOLD	20
#define NORMALMAX_FREQ	1600000
#define NORMALMIN_FREQ	350000
#define POLLING_MSEC	100

struct cpu_load_info {
	cputime64_t cpu_idle;
	cputime64_t cpu_iowait;
	cputime64_t cpu_wall;
	cputime64_t cpu_nice;
	unsigned int load;
	unsigned long int load_factor;
	s64 cpu_up_timestamp;
};

static DEFINE_PER_CPU(struct cpu_load_info, cur_cpu_info);
static DEFINE_MUTEX(dm_hotplug_lock);

static int cpu_util[NR_CPUS];
static struct pm_qos_request max_cpu_qos_hotplug;
static unsigned int cur_load_freq = 0;
static bool lcd_is_on;
extern unsigned int get_hotplug_enabled(void);
extern unsigned int get_hotplug_cpu_up_load(void);
extern unsigned int get_hotplug_cpu_up_boost(void);
extern unsigned int get_normalmin_freq(void);
extern unsigned int get_hotplug_cpu_down_hysteresis(void);

static unsigned int hotplug_enabled_flag = 1;
static unsigned int hotplug_cpu_up_load_value = 0;
static unsigned int hotplug_cpu_up_boost_value = 50;
static unsigned int hotplug_cpu_down_hysteresis_value = 20;
static s64 hotplug_cpu_down_delay_value = 300000000;
static bool hotplug_need_boost;

enum hotplug_mode {
	CHP_NORMAL,
	CHP_LOW_POWER,
};

static enum hotplug_mode prev_mode;
static unsigned int delay = POLLING_MSEC;

static bool exynos_dm_hotplug_disable;

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static inline void pm_qos_update_max(int frequency)
{
	if (pm_qos_request_active(&max_cpu_qos_hotplug))
		pm_qos_update_request(&max_cpu_qos_hotplug, frequency);
	else
		pm_qos_add_request(&max_cpu_qos_hotplug, PM_QOS_CPU_FREQ_MAX, frequency);
}

static int fb_state_change(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if (val != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		lcd_is_on = false;
		//pr_info("LCD is off\n");
		break;
	case FB_BLANK_UNBLANK:
		/*
		 * LCD blank CPU qos is set by exynos-ikcs-cpufreq
		 * This line of code release max limit when LCD is
		 * turned on.
		 */
		lcd_is_on = true;
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block fb_block = {
	.notifier_call = fb_state_change,
};

static int __ref __cpu_hotplug(struct cpumask *be_out_cpus)
{
	int i = 0;
	int ret = 0;
	
	mutex_lock(&dm_hotplug_lock);
	if (exynos_dm_hotplug_disable ||
			cpumask_weight(be_out_cpus) >= NR_CPUS) {
		ret = -EPERM;
		goto out;
	}

	mutex_lock(&cpufreq_lock);
	for (i=1; i < NR_CPUS; i++) {
		if (cpumask_test_cpu(i, be_out_cpus)) {
			ret = cpu_down(i);
			if (ret)
				break;
		} else {
			if (!hotplug_enabled_flag || exynos_dm_hotplug_disable) {
				ret = cpu_up(i);
				if (ret)
					break;
			}
		}
	}
	mutex_unlock(&cpufreq_lock);
out:

	mutex_unlock(&dm_hotplug_lock);

	return ret;
}

static int dynamic_hotplug(enum hotplug_mode mode)
{
	int i;
	struct cpumask out_target;
	enum hotplug_mode ret = 0;

	cpumask_clear(&out_target);

	switch (mode) {
	case CHP_LOW_POWER:
		for (i=1; i < NR_CPUS; i++)
			cpumask_set_cpu(i, &out_target);
		ret = __cpu_hotplug(&out_target);
		break;
	case CHP_NORMAL:
	default:
		if (cpumask_weight(cpu_online_mask) < NR_CPUS)
			ret = __cpu_hotplug(&out_target);
		break;
	}

	return ret;
}

static int exynos_dm_hotplug_notifier(struct notifier_block *notifier,
					unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (!exynos_dm_hotplug_disable) {
			unsigned int i;
			for (i=1; i < NR_CPUS; i++) 
				if (cpu_online(i)) 
					cpu_down(i);
		}
		mutex_lock(&dm_hotplug_lock);
		exynos_dm_hotplug_disable = true;
		mutex_unlock(&dm_hotplug_lock);
		break;

	case PM_POST_SUSPEND:
		mutex_lock(&dm_hotplug_lock);
		exynos_dm_hotplug_disable = false;
		mutex_unlock(&dm_hotplug_lock);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_dm_hotplug_nb = {
	.notifier_call = exynos_dm_hotplug_notifier,
	.priority = 1,
};

static unsigned int low_stay = 0;

static enum hotplug_mode diagnose_condition(void)
{
	int ret;
	int normalmin_fq = get_normalmin_freq();

	ret = CHP_NORMAL;

	if (cur_load_freq > normalmin_fq && low_stay > 0)
		low_stay -= 1;
	else if (cur_load_freq <= normalmin_fq && low_stay <= 5)
		low_stay++;
	if (low_stay > 5) { // && !lcd_is_on)
		ret = CHP_LOW_POWER;
	}
	
	return ret;
}

static void do_hotplug(int cpus_needed, struct cpumask *free_cores) {
	unsigned int i;

#ifdef DEBUG_HOTPLUG
	pr_info("do_hotplug: online_cpus=%d, cpus_needed=%d, hotplug_enabled_flag=%d, hotplug_suspended=%d\n", 
		num_online_cpus(), cpus_needed, hotplug_enabled_flag, exynos_dm_hotplug_disable);
#endif

	mutex_lock(&dm_hotplug_lock);
	if (cpus_needed > 0 && prev_mode == CHP_NORMAL) {
		for (i = 1; i < NR_CPUS && cpus_needed > 0; i++) {
			struct cpu_load_info *i_load_info;
			if (cpu_online(i))
				continue;
#ifdef DEBUG_HOTPLUG
			pr_info("do_hotplug: cpu%d cpu_up: num_online_cpus()=%d, cpus_needed=%d\n", 
				i, num_online_cpus(), cpus_needed);
#endif
			cpu_up(i);
			i_load_info = &per_cpu(cur_cpu_info, i);
			i_load_info->cpu_up_timestamp = ktime_get_boottime().tv64;
			i_load_info->load_factor = 0;
			cpus_needed--;
#ifdef DEBUG_HOTPLUG
			pr_info("do_hotplug: cpu%d cpu_up: num_online_cpus()=%d, cpus_needed=%d, curr_time=%lli\n", 
				i, num_online_cpus(), cpus_needed, ktime_get_boottime().tv64);
#endif
		}
	} else if (cpus_needed < 0) {
#ifdef DEBUG_HOTPLUG
		pr_info("do_hotplug: cpus_needed < 0, NR_CPUS=%d\n", NR_CPUS);
#endif
		for_each_cpu(i, free_cores) {
			if (!i)
				continue;
#ifdef DEBUG_HOTPLUG
			pr_info("do_hotplug: cpu%d cpu_down: num_online_cpus()=%d, cpus_needed=%d\n", 
				i, num_online_cpus(), cpus_needed);
#endif
			cpu_down(i);
			cpus_needed++;
#ifdef DEBUG_HOTPLUG
			pr_info("do_hotplug: cpu%d cpu_down: num_online_cpus()=%d, cpus_needed=%d\n", 
				i, num_online_cpus(), cpus_needed);
#endif
		}
	}
	mutex_unlock(&dm_hotplug_lock);
#ifdef DEBUG_HOTPLUG
	pr_info("do_hotplug: online_cpus=%d, cpus_needed=%d\n", num_online_cpus(), cpus_needed);
#endif
}

static void hotplug_cpus(void) {
	unsigned int i, j = 0;
	struct cpumask free_cores;
	unsigned long cpu_up_boost_work = hotplug_cpu_up_boost_value * 1000;
	int cpus_needed;
	int cpus_online = num_online_cpus();

	for_each_online_cpu(i) {
		struct cpu_load_info *i_load_info = &per_cpu(cur_cpu_info, i);
		unsigned long freq_load = i_load_info->load_factor;
		unsigned long cpu_up_threshold;
		cpu_up_threshold = (hotplug_cpu_up_load_value + (hotplug_cpu_down_hysteresis_value * j)) * 1000;

#ifdef DEBUG_HOTPLUG
		pr_info("hotplug_cpus: cpu%d, online_cpus=%d, cpus_needed=%d, cpu_up_threshold=%lu, freq_load=%lu, hotplug_need_boost=%u\n",
			i, num_online_cpus(), cpus_needed, cpu_up_threshold, freq_load, hotplug_need_boost);
#endif

		if (freq_load >= cpu_up_threshold || freq_load > cpu_up_boost_work || hotplug_need_boost) {
			cpus_needed++;
			if (freq_load > cpu_up_boost_work || hotplug_need_boost)
				cpus_needed++;
			if (hotplug_need_boost)
				hotplug_need_boost = false;
		} else {
			unsigned long load
					= freq_load + (cpu_up_threshold * hotplug_cpu_down_hysteresis_value / 100);
#ifdef DEBUG_HOTPLUG
			pr_info("hotplug_cpus: cpu%d, cpus_needed=%d, cpu_up_threshold=%lu, load=%lu\n",
				i, cpus_needed, cpu_up_threshold, load);
#endif
			if (load < cpu_up_threshold) {
				cpus_needed--;
			}
		}
		j++;
	}
	cpumask_clear(&free_cores);

#ifdef DEBUG_HOTPLUG
	pr_info("hotplug_cpus: online_cpus=%d, cpus_needed=%d, free_cores=%d, hotplug_need_boost=%d\n",
			num_online_cpus(), cpus_needed, cpumask_weight(&free_cores), hotplug_need_boost);
#endif

	if (!cpus_needed
			|| (cpus_online == NR_CPUS && cpus_needed > 0)
			|| (cpus_online == 1 && cpus_needed < 0))
		return;

	if (cpus_needed < 0 && cpus_online > 1) {
		for (j = 1; j < cpumask_weight(cpu_online_mask); j++) {
			unsigned int n;
			unsigned int min_cpu_load = INT_MAX;
			s64 target_time = ktime_get_boottime().tv64 - hotplug_cpu_down_delay_value;

			for_each_online_cpu(i) {
				struct cpu_load_info *i_load_info;
				if (!i)
					continue;
				i_load_info = &per_cpu(cur_cpu_info, i);
#ifdef DEBUG_HOTPLUG
				pr_info("hotplug_cpus: cpu%d, i_load_info->cpu_up_timestamp=%lli, target_time=%llu\n", i, i_load_info->cpu_up_timestamp, target_time);
#endif
				if (!cpumask_test_cpu(i, &free_cores)
						&& i_load_info->load_factor <= min_cpu_load
						&& (!lcd_is_on || target_time > i_load_info->cpu_up_timestamp)) {
					min_cpu_load = i_load_info->load_factor;
					n = i;
				}
			}
			if (min_cpu_load < INT_MAX) {
				cpumask_set_cpu(n, &free_cores);
#ifdef DEBUG_HOTPLUG
				pr_info("hotplug_cpus: n=%d, free_cores=%d, min_cpu_load=%u\n",
					n, cpumask_weight(&free_cores), min_cpu_load);
#endif
			}
		}
	}
	
#ifdef DEBUG_HOTPLUG
	pr_info("hotplug_cpus: free_cores=%d\n", cpumask_weight(&free_cores));
#endif

	if (cpus_needed > 0 || cpumask_weight(&free_cores)) {
#ifdef DEBUG_HOTPLUG
	for_each_cpu(i, &free_cores)
		pr_info("hotplug_cpus: for_each_cpus: free_cores=%d\n", i);
#endif
		do_hotplug(cpus_needed, &free_cores);
	}
}


static void calc_load(void)
{
	struct cpufreq_policy *policy;
	unsigned int i;
	
	policy = cpufreq_cpu_get(0);
	

	if (!policy) {
		pr_err("Invalid policy\n");
		return;
	}

	cur_load_freq = policy->cur;

	for_each_cpu(i, policy->cpus) {
		struct cpu_load_info	*i_load_info;
		cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load;
		unsigned long int load_factor;

		i_load_info = &per_cpu(cur_cpu_info, i);

		cur_idle_time = get_cpu_idle_time(i, &cur_wall_time);
		//cur_iowait_time = get_cpu_iowait_time(i, &cur_wall_time);

		wall_time = (unsigned int)
			(cur_wall_time - i_load_info->cpu_wall);
		i_load_info->cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
			(cur_idle_time - i_load_info->cpu_idle);
		i_load_info->cpu_idle = cur_idle_time;

		//iowait_time = (unsigned int)(cur_iowait_time - i_load_info->cpu_iowait);
		//i_load_info->cpu_iowait = cur_iowait_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;
		i_load_info->load = load;

		load_factor = 1000 * load * (policy->cur / 100000) / (policy->max / 100000);
		i_load_info->load_factor = load_factor;
	}

	if (hotplug_enabled_flag)
		hotplug_cpus();
	
	cpufreq_cpu_put(policy);
	return;
}

static int thread_run_flag;

static int __cpuinit on_run(void *data)
{
	int on_cpu = 0;
	enum hotplug_mode exe_mode;

	struct cpumask thread_cpumask;

	cpumask_clear(&thread_cpumask);
	cpumask_set_cpu(on_cpu, &thread_cpumask);
	sched_setaffinity(0, &thread_cpumask);

	prev_mode = CHP_NORMAL;
	thread_run_flag = 1;

	while (thread_run_flag) {
		if (!exynos_dm_hotplug_disable) {

			hotplug_enabled_flag = get_hotplug_enabled();
			hotplug_cpu_up_load_value = get_hotplug_cpu_up_load();
			hotplug_cpu_up_boost_value = get_hotplug_cpu_up_boost();
			hotplug_cpu_down_hysteresis_value = get_hotplug_cpu_down_hysteresis();

			calc_load();
			exe_mode = diagnose_condition();

			if (exe_mode != prev_mode) {
#ifdef DM_HOTPLUG_DEBUG
				pr_debug("frequency info : %d, %s\n", cur_load_freq
					, (exe_mode<1)?"NORMAL":((exe_mode<2)?"LOW":"HIGH"));
#endif
				switch (exe_mode) {
				case CHP_LOW_POWER:
					delay = POLLING_MSEC * (lcd_is_on ? 0.5 : 4);
					break;
				case CHP_NORMAL:
				default:
					delay = POLLING_MSEC;
					break;
				}

				if (!hotplug_enabled_flag)
					dynamic_hotplug(exe_mode);
				//if (dynamic_hotplug(exe_mode) < 0)
				//	exe_mode = prev_mode;
			}

			prev_mode = exe_mode;
		}
		msleep(delay);
	}

	return 0;
}

void dm_cpu_hotplug_exit(void)
{
	thread_run_flag = 0;
	fb_unregister_client(&fb_block);
	unregister_pm_notifier(&exynos_dm_hotplug_nb);
}

void __cpuinit dm_cpu_hotplug_init(void)
{
	struct task_struct *k;

	k = kthread_run(&on_run, NULL, "thread_hotplug_func");
	if (IS_ERR(k))
		pr_err("Failed in creation of thread.\n");

	fb_register_client(&fb_block);
	lcd_is_on = true;

	exynos_dm_hotplug_disable = false;

	register_pm_notifier(&exynos_dm_hotplug_nb);
}
