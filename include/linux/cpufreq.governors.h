#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_SMARTASS2)
extern struct cpufreq_governor cpufreq_gov_smartass2;
#define CPUFREQ_DEFAULT_GOVERNOR        (&cpufreq_gov_smartass2)
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_LULZACTIVE)
extern struct cpufreq_governor cpufreq_gov_lulzactive;
#define CPUFREQ_DEFAULT_GOVERNOR        (&cpufreq_gov_lulzactive)
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_WHEATLEY)
extern struct cpufreq_governor cpufreq_gov_wheatley;
#define CPUFREQ_DEFAULT_GOVERNOR        (&cpufreq_gov_wheatley)
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_LAGFREE)
extern struct cpufreq_governor cpufreq_gov_lagfree;
#define CPUFREQ_DEFAULT_GOVERNOR  (&cpufreq_gov_lagfree)
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMANDX)
extern struct cpufreq_governor cpufreq_gov_ondemandX;
#define CPUFREQ_DEFAULT_GOVERNOR  (&cpufreq_gov_ondemandX)
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMANDPLUS)
extern struct cpufreq_governor cpufreq_gov_ondemandplus;
#define CPUFREQ_DEFAULT_GOVERNOR (&cpufreq_gov_ondemandplus)
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_INTELLIDEMAND)
extern struct cpufreq_governor cpufreq_gov_intellidemand;
#define CPUFREQ_DEFAULT_GOVERNOR (&cpufreq_gov_intellidemand)
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_INTELLIACTIVE)
extern struct cpufreq_governor cpufreq_gov_intelliactive;
#define CPUFREQ_DEFAULT_GOVERNOR (&cpufreq_gov_intelliactive)
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_LIONHEART)
extern struct cpufreq_governor cpufreq_gov_lionheart;
#define CPUFREQ_DEFAULT_GOVERNOR (&cpufreq_gov_lionheart)