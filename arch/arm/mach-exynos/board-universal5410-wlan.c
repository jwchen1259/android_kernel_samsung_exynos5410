#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>

#include <plat/devs.h>
#include <plat/sdhci.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <linux/gpio.h>

#include "board-universal5410-wlan.h"

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define WLAN_STATIC_DHD_INFO_BUF	7
#define WLAN_SCAN_BUF_SIZE		(64 * 1024)
#define WLAN_DHD_INFO_BUF_SIZE	(16 * 1024)
#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
void *wlan_static_dhd_info_buf;

#define ENABLE_4335BT_WAR
#ifdef ENABLE_4335BT_WAR
static int bt_off = 0;
extern int bt_is_running;
#endif /* ENABLE_4335BT_WAR */

static void *brcm_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;

	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;

	if (section == WLAN_STATIC_DHD_INFO_BUF) {
		if (size > WLAN_DHD_INFO_BUF_SIZE) {
			pr_err("request DHD_INFO size(%lu) is bigger than static size(%d).\n", size, WLAN_DHD_INFO_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int brcm_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}

	wlan_static_scan_buf0 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;

	wlan_static_scan_buf1 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf1)
		goto err_mem_alloc;

	wlan_static_dhd_info_buf = kmalloc(WLAN_DHD_INFO_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_info_buf)
		goto err_mem_alloc;

	printk(KERN_INFO"%s: WIFI MEM Allocated\n", __func__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

static unsigned int wlan_on_gpio_table[][4] = {
	{GPIO_WLAN_EN , GPIO_WLAN_EN_AF, GPIO_LEVEL_HIGH, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_HOST_WAKE, GPIO_WLAN_HOST_WAKE_AF,
	 GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN},
};

static unsigned int wlan_off_gpio_table[][4] = {
	{GPIO_WLAN_EN , GPIO_WLAN_EN_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_HOST_WAKE, 0 , GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN},
};

static unsigned int wlan_sdio_on_table[][4] = {
	{GPIO_WLAN_SDIO_CLK, GPIO_WLAN_SDIO_CLK_AF,
	 GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_CMD, GPIO_WLAN_SDIO_CMD_AF,
	 GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D0, GPIO_WLAN_SDIO_D0_AF,
	 GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D1, GPIO_WLAN_SDIO_D1_AF,
	 GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D2, GPIO_WLAN_SDIO_D2_AF,
	 GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D3, GPIO_WLAN_SDIO_D3_AF,
	 GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

static unsigned int wlan_sdio_off_table[][4] = {
	{GPIO_WLAN_SDIO_CLK, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_CMD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D0, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D1, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D2, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D3, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

static void s3c_config_gpio_alive_table
(int array_size, unsigned int
(*gpio_table)[4])
{
	u32 i, gpio;
	printk(KERN_INFO"gpio_table = [%d] \r\n" , array_size);
	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));
		s3c_gpio_setpull(gpio, gpio_table[i][3]);
		if (gpio_table[i][2] != GPIO_LEVEL_NONE)
			gpio_set_value(gpio, gpio_table[i][2]);
	}
}

#ifdef ENABLE_4335BT_WAR
static int brcm_wlan_power(int onoff,bool b0rev)
#else
static int brcm_wlan_power(int onoff)
#endif
{
	printk(KERN_INFO"------------------------------------------------");
	printk(KERN_INFO"------------------------------------------------\n");
	printk(KERN_INFO"%s Enter: power %s\n", __func__, onoff ? "on" : "off");

	if (onoff) {
#ifdef ENABLE_4335BT_WAR
		/*  Xtal start up workaround patch */
		if(b0rev == true && gpio_get_value(GPIO_BT_EN) == 0)
		{
			bt_off = 1;
			gpio_set_value(GPIO_BT_EN, GPIO_LEVEL_HIGH);
			printk("[brcm_wlan_power] BT_REG_ON -> On\n");
			udelay(50);
		}
		else {
			bt_off = 0;
		}
#endif /* ENABLE_4335BT_WAR */
		s3c_config_gpio_alive_table
			(ARRAY_SIZE(wlan_on_gpio_table), wlan_on_gpio_table);
		s5p_gpio_set_pd_cfg(GPIO_WLAN_EN, S5P_GPIO_PD_PREV_STATE);
		udelay(200);
		gpio_set_value(GPIO_WLAN_EN, GPIO_LEVEL_HIGH);
		printk(KERN_DEBUG"WLAN: GPIO_WLAN_EN = %d\n",
		gpio_get_value(GPIO_WLAN_EN));
	} else {
		gpio_set_value(GPIO_WLAN_EN, GPIO_LEVEL_LOW);
		s3c_config_gpio_alive_table
			(ARRAY_SIZE(wlan_off_gpio_table), wlan_off_gpio_table);
		s5p_gpio_set_pd_cfg(GPIO_WLAN_EN, S5P_GPIO_PD_OUTPUT0);
		printk(KERN_DEBUG"WLAN: GPIO_WLAN_EN = %d\n",
		gpio_get_value(GPIO_WLAN_EN));
		udelay(100);
	}

#ifdef ENABLE_4335BT_WAR
	/*  Xtal start up workaround patch */
	if(onoff && (bt_off == 1) && (bt_is_running == 0)) {
		udelay(100);
		gpio_set_value(GPIO_BT_EN, GPIO_LEVEL_LOW);
		printk("[brcm_wlan_power]  BT_REG_ON -> Off\n");
	}
#endif /* ENABLE_4335BT_WAR */

	return 0;
}

static int brcm_wlan_reset(int onoff)
{
	gpio_set_value(GPIO_WLAN_EN,
			onoff ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
	return 0;
}

static int brcm_wlan_set_carddetect(int onoff)
{
	if (onoff) {
		s3c_config_gpio_alive_table(
ARRAY_SIZE(wlan_sdio_on_table), wlan_sdio_on_table);
	} else {
		s3c_config_gpio_alive_table(
ARRAY_SIZE(wlan_sdio_off_table), wlan_sdio_off_table); }

#if 0
	udelay(200);
#else
	mdelay(10);
#endif

	mmc_force_presence_change(&exynos5_device_dwmci1, onoff);
	msleep(500); /* wait for carddetect */

	return 0;
}

/* Customized Locale table : OPTIONAL feature */
#define COUNTRY_BUF_SZ        4
struct cntry_locales_custom {
	char iso_abbrev[COUNTRY_BUF_SZ];
	char custom_locale[COUNTRY_BUF_SZ];
	int  custom_locale_rev;
};

static struct cntry_locales_custom brcm_wlan_translate_custom_table[] = {
	/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XV", 17},	/* Universal if Country code is unknown or empty */
	{"IR", "XV", 17},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XV", 17},	/* Universal if Country code is SUDAN */
	{"SY", "XV", 17},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"GL", "XV", 17},	/* Universal if Country code is GREENLAND */
	{"PS", "XV", 17},	/* Universal if Country code is PALESTINE */
	{"TL", "XV", 17},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XV", 17},	/* Universal if Country code is MARSHALL ISLANDS */
	{"PK", "XV", 17},	/* Universal if Country code is PAKISTAN */
	{"CK", "XV", 17},	/* Universal if Country code is Cook Island (13.4.27)*/
	{"CU", "XV", 17},	/* Universal if Country code is Cuba (13.4.27)*/
	{"FK", "XV", 17},	/* Universal if Country code is Falkland Island (13.4.27)*/
	{"FO", "XV", 17},	/* Universal if Country code is Faroe Island (13.4.27)*/
	{"GI", "XV", 17},	/* Universal if Country code is Gibraltar (13.4.27)*/
	{"IM", "XV", 17},	/* Universal if Country code is Isle of Man (13.4.27)*/
	{"CI", "XV", 17},	/* Universal if Country code is Ivory Coast (13.4.27)*/
	{"JE", "XV", 17},	/* Universal if Country code is Jersey (13.4.27)*/
	{"KP", "XV", 17},	/* Universal if Country code is North Korea (13.4.27)*/
	{"FM", "XV", 17},	/* Universal if Country code is Micronesia (13.4.27)*/
	{"MM", "XV", 17},	/* Universal if Country code is Myanmar (13.4.27)*/
	{"NU", "XV", 17},	/* Universal if Country code is Niue (13.4.27)*/
	{"NF", "XV", 17},	/* Universal if Country code is Norfolk Island (13.4.27)*/
	{"PN", "XV", 17},	/* Universal if Country code is Pitcairn Islands (13.4.27)*/
	{"PM", "XV", 17},	/* Universal if Country code is Saint Pierre and Miquelon (13.4.27)*/
	{"SS", "XV", 17},	/* Universal if Country code is South_Sudan (13.4.27)*/
	{"AL", "AL", 2},
	{"DZ", "DZ", 1},
	{"AS", "AS", 12},	/* changed 2 -> 12*/
	{"AI", "AI", 1},
	{"AG", "AG", 2},
	{"AR", "AR", 21},
	{"AW", "AW", 2},
	{"AU", "AU", 6},
	{"AT", "AT", 4},
	{"AZ", "AZ", 2},
	{"BS", "BS", 2},
	{"BH", "BH", 4},	/* changed 24 -> 4*/
	{"BD", "BD", 2},
	{"BY", "BY", 3},
	{"BE", "BE", 4},
	{"BM", "BM", 12},
	{"BA", "BA", 2},
	{"BR", "BR", 4},
	{"VG", "VG", 2},
	{"BN", "BN", 4},
	{"BG", "BG", 4},
	{"KH", "KH", 2},
	{"CA", "CA", 31},
	{"KY", "KY", 3},
	{"CN", "CN", 24},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"HR", "HR", 4},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DK", "DK", 4},
	{"EE", "EE", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FR", "FR", 5},
	{"GF", "GF", 2},
	{"DE", "DE", 7},
	{"GR", "GR", 4},
	{"GD", "GD", 2},
	{"GP", "GP", 2},
	{"GU", "GU", 12},
	{"HK", "HK", 2},
	{"HU", "HU", 4},
	{"IS", "IS", 4},
	{"IN", "IN", 3},
	{"ID", "KR", 25},	/* ID/1 -> KR/24 */
	{"IE", "IE", 5},
	{"IL", "BO", 0},	/* IL/7 -> BO/0 */
	{"IT", "IT", 4},
	{"JP", "JP", 58},
	{"JO", "JO", 3},
	{"KW", "KW", 5},
	{"LA", "LA", 2},
	{"LV", "LV", 4},
	{"LB", "LB", 5},
	{"LS", "LS", 2},
	{"LI", "LI", 4},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"MO", "MO", 2},
	{"MK", "MK", 2},
	{"MW", "MW", 1},
	{"MY", "MY", 3},
	{"MV", "MV", 3},
	{"MT", "MT", 4},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MU", "MU", 2},
	{"YT", "YT", 2},
	{"MX", "MX", 20},
	{"MD", "MD", 2},
	{"MC", "MC", 1},
	{"ME", "ME", 2},
	{"MA", "MA", 2},
	{"NP", "NP", 3},
	{"NL", "NL", 4},
	{"AN", "AN", 2},
	{"NZ", "NZ", 4},
	{"NO", "NO", 4},
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PG", "PG", 2},
	{"PY", "PY", 2},
	{"PE", "PE", 20},
	{"PH", "PH", 5},
	{"PL", "PL", 4},
	{"PT", "PT", 4},
	{"PR", "PR", 20},
	{"RE", "RE", 2},
	{"RO", "RO", 4},
	{"SN", "SN", 2},
	{"RS", "RS", 2},
	{"SG", "SG", 4},
	{"SK", "SK", 4},
	{"SI", "SI", 4},
	{"ES", "ES", 4},
	{"LK", "LK", 1},
	{"SE", "SE", 4},
	{"CH", "CH", 4},
	{"TW", "TW", 1},
	{"TH", "TH", 5},
	{"TT", "TT", 3},
	{"TR", "TR", 7},
	{"AE", "AE", 6},
	{"UG", "UG", 2},
	{"GB", "GB", 6},
	{"UY", "UY", 1},
	{"VI", "VI", 13},
	{"VA", "VA", 2},
	{"VE", "VE", 3},
	{"VN", "VN", 4},
	{"MA", "MA", 1},
	{"ZM", "ZM", 2},
	{"EC", "EC", 21},
	{"SV", "SV", 19},
	{"KR", "KR", 57},
	{"RU", "RU", 13},
	{"UA", "UA", 8},
	{"GT", "GT", 1},
	{"MN", "MN", 1},
	{"NI", "NI", 2},
	{"US", "Q2", 57},
};

static void *brcm_wlan_get_country_code(char *ccode)
{
	int size, i;
	static struct cntry_locales_custom country_code;

	size = ARRAY_SIZE(brcm_wlan_translate_custom_table);

	if ((size == 0) || (ccode == NULL))
		return NULL;

	for (i = 0; i < size; i++) {
		if (!strcmp(ccode, brcm_wlan_translate_custom_table[i].iso_abbrev))
			return &brcm_wlan_translate_custom_table[i];
	}

	memset(&country_code, 0, sizeof(struct cntry_locales_custom));
	strlcpy(country_code.custom_locale, ccode, COUNTRY_BUF_SZ);

	return &country_code;
}

static struct resource brcm_wlan_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.start	= IRQ_EINT(16),
		.end	= IRQ_EINT(16),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct wifi_platform_data brcm_wlan_control = {
	.set_power	= brcm_wlan_power,
	.set_reset	= brcm_wlan_reset,
	.set_carddetect	= brcm_wlan_set_carddetect,
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= brcm_wlan_mem_prealloc,
#endif
	.get_country_code = brcm_wlan_get_country_code,
};

static struct platform_device brcm_device_wlan = {
	.name		= "bcmdhd_wlan",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(brcm_wlan_resources),
	.resource	= brcm_wlan_resources,
	.dev		= {
		.platform_data = &brcm_wlan_control,
	},
};

int __init brcm_wlan_init(void)
{
	printk(KERN_INFO"%s: start\n", __func__);

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	brcm_init_wlan_mem();
#endif

	return platform_device_register(&brcm_device_wlan);
}
