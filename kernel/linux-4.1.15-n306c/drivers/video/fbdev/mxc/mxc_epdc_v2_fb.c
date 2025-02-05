/*
 * Copyright (C) 2014-2016 Freescale Semiconductor, Inc.
 *
 * Copyright 2017 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
/*
 * Based on STMP378X LCDIF
 * Copyright 2008 Embedded Alley Solutions, Inc All Rights Reserved.
 */

//#define DEBUG
#include <linux/busfreq-imx.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/cpufreq.h>
#include <linux/firmware.h>
#include <linux/kthread.h>
#include <linux/dmaengine.h>
#include <linux/pxp_dma.h>
#include <linux/pm_runtime.h>
#include <linux/mxcfb.h>
#include <linux/mxcfb_epdc.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/mfd/max17135.h>
#include <linux/mfd/tps6518x.h>
#include <linux/fsl_devices.h>
#include <linux/bitops.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_data/dma-imx.h>
#include <asm/cacheflush.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>



#include "../../../../arch/arm/mach-imx/hardware.h"
#include "epdc_v2_regs.h"
#include "eink_processing2.h"


#include "../../../../kernel/ntx/ntx_timestamp.h"

#define GDEBUG 0
#define giDbgLvl 	1000
#include <linux/gallen_dbg.h>

#if defined(TPS65185_VDROP_PROC_IN_KERNEL)//[
	#define VDROP_PROC_IN_KERNEL		1
#endif //] defined(TPS65185_VDROP_PROC_IN_KERNEL)

#define QOS_ENABLE		1
//#define QOS_MX6SL			1
#define QOS_MX6SLL		1
#define QOS_MX6ULL		1

#ifdef QOS_MX6SL//[
	#define QOS_SOC_MX6SL()	cpu_is_imx6sl()
#else //][!QOS_MX6SL
	#define QOS_SOC_MX6SL()	0
#endif //]QOS_MX6SL

#ifdef QOS_MX6SLL//[
	#define QOS_SOC_MX6SLL()	cpu_is_imx6sll()
#else //][!QOS_MX6SLL
	#define QOS_SOC_MX6SLL()	0
#endif //]QOS_MX6SLL

#ifdef QOS_MX6ULL//[
	#define QOS_SOC_MX6ULL()	cpu_is_imx6ull()
#else //][!QOS_MX6ULL
	#define QOS_SOC_MX6ULL()	0
#endif //]QOS_MX6ULL

#define EPDC_VPRINT(fb_data,print_lvl,fmt,args...)		\
	if(fb_data && fb_data->verbose_lvl>=print_lvl) {\
		printk(fmt,##args);\
	}


#define NIGHT_MODE_SUPPORT		1

#ifdef NIGHT_MODE_SUPPORT //[
	#define NIGHT_MODE_XON_TIMING		1
#endif //] NIGHT_MODE_SUPPORT

#define VCOM_OFF_DELAY_US	150
#define NIGHTMODE_TVG_MS	0

#define DROP_OLD_COLLISION		1

#define ADJ_COLLISION_REGION_PATCH		1
#define DITHER_GC16_COLLISION_PATCH		1

#define TCE_UNDERRUN_PREVENT_PATCH	2
#define TCE_UNDERRUN_PREVENT_PIXCLK	100000000
#define TCE_UNDERRUN_PREVENT_X_RES	1920
#define TCE_UNDERRUN_PREVENT_Y_RES	1440
				/*
				 * 1 : sleep a while after collision .
				 * 2 : postpone the submit work .
				 *
				 */

// TCE_UNDERRUN_RECOVERY 
// 	1: recovery epdc .
//	2: re-sending full update to fix dirty page .
#define TCE_UNDERRUN_RECOVERY								2
#define TCE_UNDERRUN_RECOVERY_UPDATE_DELAYMS						1000

#define TCE_UNDERRUN_RECOVER_MARKERNO			0

//#define TCE_UNDERRUN_PREVENT_WORKFUNC				1
#define TCE_UNDERRUN_PREVENT_WORKFUNC_BUSYMS				50
#define TCE_UNDERRUN_PREVENT_WORKFUNC_FREEMS				1
#define TCE_UNDERRUN_PREVENT_WORKFUNC_LOOPCNT				9

#define EPDC_V2_ENABLE_HW_DITHER			1

#define EPDC_STANDARD_MODE

#define USE_PS_AS_OUTPUT

/*
 * Enable this define to have a default panel
 * loaded during driver initialization
 */
#define DEFAULT_PANEL_HW_INIT

//#define ED078KH1_75HZ		1

#define SG_NUM				14 /* 2+4+4+4  */
#define NUM_SCREENS_MIN	2

#define EPDC_V1_NUM_LUTS	16
#define EPDC_V1_MAX_NUM_UPDATES 20
#define EPDC_V2_NUM_LUTS	64
#define EPDC_V2_MAX_NUM_UPDATES 64
#define EPDC_MAX_NUM_BUFFERS	2
#define INVALID_LUT		(-1)
#define DRY_RUN_NO_LUT		100

/* Maximum update buffer image width due to v2.0 and v2.1 errata ERR005313. */
#define EPDC_V2_MAX_UPDATE_WIDTH	2047
#define EPDC_V2_ROTATION_ALIGNMENT	8

#define DEFAULT_TEMP_INDEX	0
#define DEFAULT_TEMP		20 /* room temp in deg Celsius */
//#define DEFAULT_TEMP_AUTO_UPDATE_PERIOD	60 /* 60 seconds */
#define DEFAULT_TEMP_AUTO_UPDATE_PERIOD	FB_TEMP_AUTO_UPDATE_DISABLE /* temperature auto update default is disabled */

#define INIT_UPDATE_MARKER	0x12345678
#define PAN_UPDATE_MARKER	0x12345679

#define POWER_STATE_OFF	0
#define POWER_STATE_ON	1

#define MERGE_OK	0
#define MERGE_FAIL	1
#define MERGE_BLOCK	2

//#define EPD_SUSPEND_BLANK			1

#define TCEUNDERRUN_PROC_STAT_OK			0	// non tce underrun
#define TCEUNDERRUN_PROC_STAT_INT			1 // interrupted .
#define TCEUNDERRUN_PROC_STAT_PS1			2	// in tce underrun procedure stage 1
#define TCEUNDERRUN_PROC_STAT_PS2			3 // in tce underrun procedure stage 2 
#define TCEUNDERRUN_PROC_STAT_PSF			10 // tce underrun procedure stage finished . 

#define AUTO_NTX_MODES		1

#define NTX_WFM_MODE_INIT			0
#define NTX_WFM_MODE_DU				1
#define NTX_WFM_MODE_GC16			2
#define NTX_WFM_MODE_GC4			3
#define NTX_WFM_MODE_A2				4
#define NTX_WFM_MODE_GL16			5
#define NTX_WFM_MODE_GLR16		6
#define NTX_WFM_MODE_GLD16		7
#define NTX_WFM_MODE_DU4			8
#define NTX_WFM_MODE_GCK16		9
#define NTX_WFM_MODE_GLKW16		10
#define NTX_WFM_MODE_TOTAL		11
static int giNTX_waveform_modeA[NTX_WFM_MODE_TOTAL];

volatile static int giLast_waveform_mode = -1;

unsigned char gbModeVersion=0 ;
unsigned char gbWFM_REV ;
unsigned char gbFPL_Platform ;

extern int gSleep_Mode_Suspend;

static u64 used_luts = 0x1;	/* do not use LUT0 */
static unsigned long default_bpp = 16;
static int vcom_nominal;

#define TCE_STATE_NORMAL			0
#define TCE_STATE_CRITICAL		1
static volatile int giTCE_State = TCE_STATE_NORMAL;

static const char gszDisplayReg[]="DISPLAY";
static const char gszVcomReg[]="VCOM";
static const char gszVP3V3Reg[]="V3P3";
static const char gszTMSTReg[]="TMST";

static const char gszDisplayReg_sy7636[]="DISPLAY_SY7636";
static const char gszVcomReg_sy7636[]="VCOM_SY7636";
static const char gszVP3V3Reg_sy7636[]="V3P3_SY7636";
static const char gszTMSTReg_sy7636[]="TMST_SY7636";

static const char gszDisplayReg_jd9930[]="DISPLAY_JD9930";
static const char gszVcomReg_jd9930[]="VCOM_JD9930";
static const char gszVP3V3Reg_jd9930[]="V3P3_JD9930";
static const char gszTMSTReg_jd9930[]="TMST_JD9930";

static char *gpszDisplayReg = gszDisplayReg;
static char *gpszVcomReg = gszVcomReg;
static char *gpszVP3V3Reg = gszVP3V3Reg;
static char *gpszTMSTReg = gszTMSTReg;


extern int ktime_get_diffus(ktime_t starttime);

struct update_marker_data {
	struct list_head full_list;
	struct list_head upd_list;
	u32 update_marker;
	struct completion update_completion;
	int lut_num;
	bool collision_test;
	bool waiting;
};

struct update_desc_list {
	struct list_head list;
	struct mxcfb_update_data upd_data;/* Update parameters */
	u32 epdc_offs;		/* Added to buffer ptr to resolve alignment */
	u32 epdc_stride;	/* Depends on rotation & whether we skip PxP */
	struct list_head upd_marker_list; /* List of markers for this update */
	u32 update_order;	/* Numeric ordering value for update */
};

/* This structure represents a list node containing both
 * a memory region allocated as an output buffer for the PxP
 * update processing task, and the update description (mode, region, etc.) */
struct update_data_list {
	struct list_head list;
	dma_addr_t phys_addr;	/* Pointer to phys address of processed Y buf */
	void *virt_addr;
	struct update_desc_list *update_desc;
	int lut_num;		/* Assigned before update is processed into working buffer */
	u64 collision_mask;	/* Set when update creates collision */
				/* Mask of the LUTs the update collides with */
};

struct mxc_epdc_fb_data {
	struct fb_info info;
	struct fb_var_screeninfo epdc_fb_var; /* Internal copy of screeninfo
						so we can sync changes to it */
	u32 pseudo_palette[16];
	char fw_str[24];
	struct list_head list;
	struct imx_epdc_fb_mode *cur_mode;
	struct imx_epdc_fb_platform_data *pdata;
	int blank;
	u32 max_pix_size;
	ssize_t map_size;
	dma_addr_t phys_start;
	void *virt_start;
	u32 fb_offset;
	int default_bpp;
	int native_width;
	int native_height;
	int num_screens;
	int epdc_irq;
	struct device *dev;
	int power_state;
	int wait_for_powerdown;
	struct completion powerdown_compl;
	struct clk *epdc_clk_axi;
	struct clk *epdc_clk_pix;
	struct regulator *display_regulator;
	struct regulator *vcom_regulator;
	struct regulator *v3p3_regulator;
	bool v3p3_fixed ;
	struct regulator *tmst_regulator;
	bool fw_default_load;
	int rev;

	/* FB elements related to EPDC updates */
	int num_luts;
	int max_num_updates;
	bool in_init;
	bool hw_ready;
	bool hw_initializing;
	bool waiting_for_idle;
	u32 auto_mode;
	u32 upd_scheme;
	struct list_head upd_pending_list;
	struct list_head upd_buf_queue;
	struct list_head upd_buf_free_list;
	struct list_head upd_buf_collision_list;
	struct update_data_list *cur_update;
	struct mutex queue_mutex;
	int trt_entries;
	int temp_auto_update_period;
	unsigned long last_time_temp_auto_update;
	int temp_index;
	u8 *temp_range_bounds;

#ifdef MXCFB_WAVEFORM_MODES_NTX //[
	struct mxcfb_waveform_modes_ntx wv_modes;
#else //][!MXCFB_WAVEFORM_MODES_NTX
	struct mxcfb_waveform_modes wv_modes;
#endif //] MXCFB_WAVEFORM_MODES_NTX

	int wfm;

	bool wv_modes_update;
	bool waveform_is_advanced;
	/* Elements related to gen2 waveform data */
	u8 *waveform_vcd_buffer;
	u8 *waveform_acd_buffer;
	u32 waveform_magic_number;
	u32 waveform_mc;
	u32 waveform_trc;
	int vpdd_len; /* in ms */
	int xon_len;  /* in ms */
	u8 *waveform_xwi_buffer;
	char *waveform_xwi_string;
	unsigned waveform_xwi_string_length;

	u32 *waveform_buffer_virt;
	u32 waveform_buffer_phys;
	u32 waveform_buffer_size;
	u32 *working_buffer_virt;
	u32 working_buffer_phys;
	u32 working_buffer_size;
	u32 *tmp_working_buffer_virt;
	u32 tmp_working_buffer_phys;
	dma_addr_t *phys_addr_updbuf;
	void **virt_addr_updbuf;
	u32 upd_buffer_num;
	u32 max_num_buffers;
	dma_addr_t phys_addr_copybuf;	/* Phys address of copied update data */
	void *virt_addr_copybuf;	/* Used for PxP SW workaround */
	dma_addr_t phys_addr_y4;
	void *virt_addr_y4;
	dma_addr_t phys_addr_y4c;
	void *virt_addr_y4c;
	dma_addr_t phys_addr_black;
	void *virt_addr_black;
	u32 order_cnt;
	struct list_head full_marker_list;
	u32 *lut_update_order;		/* Array size = number of luts */
	u64 epdc_colliding_luts;
	u64 luts_complete_wb;
	u64 luts_complete;
	u64 luts_updating;
	struct completion updates_done;
	struct delayed_work epdc_done_work;
	struct workqueue_struct *epdc_submit_workqueue;
	struct work_struct epdc_submit_work;
	struct workqueue_struct *epdc_intr_workqueue;
	struct work_struct epdc_intr_work;
//#ifdef FW_IN_RAM//[
	struct workqueue_struct *epdc_firmware_workqueue;
	//struct work_struct epdc_firmware_work;
	struct delayed_work epdc_firmware_work;
	struct semaphore firmware_work_lock;
//#endif //]FW_IN_RAM
	bool waiting_for_wb;
	bool waiting_for_lut;
	bool waiting_for_lut15;
	struct completion update_res_free;
	struct completion lut15_free;
	struct completion eof_event;
	int eof_sync_period;
	struct mutex power_mutex;
	bool powering_down;
	bool updates_active;
	int pwrdown_delay;
	unsigned long tce_prevent;
	bool restrict_width; /* work around rev >=2.0 width and
				stride restriction  */

	/* FB elements related to PxP DMA */
	struct completion pxp_tx_cmpl;
	struct pxp_channel *pxp_chan;
	struct pxp_config_data pxp_conf;
	struct dma_async_tx_descriptor *txd;
	dma_cookie_t cookie;
	struct scatterlist sg[SG_NUM];
	struct mutex pxp_mutex; /* protects access to PxP */

	/* external mode or internal mode */
	int epdc_wb_mode;
	struct pxp_collision_info col_info;
	u32 hist_status;
	u32 pixel_nums;

	struct regmap *gpr;
	u8 req_gpr;
	u8 req_bit;
	u32 dwSafeTicksEP3V3; // the safe ticks we must to wait for EP3V3 .
	u32 dwJiffies_To_TurnOFF_EP3V3;// the jiffies value if >= this value , we can turn off the EP3V3 .
	struct delayed_work epdc_reupdate_work;
	struct mxcfb_rect latest_update_region;
	u32 active_updating_w,active_updating_h;
	struct mxcfb_rect lut_rect[64];
	//u32 lut_x[64],lut_y[64];
	//u32 lut_w[64],lut_h[64];
	u64 lut_status;

	struct platform_device *pdev;


	int verbose_lvl;

	int lastest_lut_num;
	int tce_underrun_proc_stat;

#ifdef TCE_UNDERRUN_PREVENT_WORKFUNC
	struct delayed_work tce_safe_work;
	unsigned long tce_safe_ms,tce_safe_freems;
	int tce_safe_loops;
	int tce_safe_work_running;
	int tce_safe_work_cancel;
	spinlock_t tce_safe_lock;
#endif //] TCE_UNDERRUN_PREVENT_WORKFUNC

	int iVCOM_offset_uV;
#ifdef NIGHT_MODE_XON_TIMING //[
	unsigned gpio_xon;
	struct gpio_desc *gpio_xon_desc;
	struct hrtimer hrt_xon_on_ctrl;
	struct hrtimer hrt_xon_off_ctrl;
	unsigned long xon_on_delay_us;
	unsigned long xon_off_delay_us;
	unsigned long xon_off_day_delay_us;
#endif //] NIGHT_MODE_XON_TIMING

	/*
	 * night_mode_test :
	 * -2 : do not have any night mode control
	 *  -1 : xon inital high level . 
	 *  0 : xon initail low level .
	 *  1 : control xon and power off sequence for night mode .
	 *  >=2 : force invert white and black for night mode pattern test . 
	 *  >=3 : force use gck/glw16 instead of gc16/gl16 , reagld/reagl . 
	 */
	int night_mode_test;

	int force_invert;
	int vcom_off_with_data;
	struct mxcfb_update_marker_data last_nightmode_upd_marker;
};

struct waveform_data_header {
	unsigned int wi0;
	unsigned int wi1;
	unsigned int wi2;
	unsigned int wi3;
	unsigned int wi4;
	unsigned int wi5;
	unsigned int wi6;
	unsigned int xwia:24;
	unsigned int cs1:8;
	unsigned int wmta:24;
	unsigned int fvsn:8;
	unsigned int luts:8;
	unsigned int mc:8;
	unsigned int trc:8;
	unsigned int awf:8;
	unsigned int eb:8;
	unsigned int sb:8;
	unsigned int reserved0_1:8;
	unsigned int reserved0_2:8;
	unsigned int reserved0_3:8;
	unsigned int reserved0_4:8;
	unsigned int reserved0_5:8;
	unsigned int cs2:8;
};

struct mxcfb_waveform_data_file {
	struct waveform_data_header wdh;
	u32 *data;	/* Temperature Range Table + Waveform Data */
};

#define WAVEFORM_HDR_LUT_ADVANCED_ALGO_MASK 0xc

#if 1 //[

//#define EPD_TIMING_ED068OG1_NUMCE3	1

static struct fb_videomode ed060sct_mode = {
.name = "E60SCT",
.refresh = 85,
.xres = 800,
.yres = 600,
.pixclock = 26680000,
.left_margin = 8,
.right_margin = 96,
.upper_margin = 4,
.lower_margin = 13,
.hsync_len = 4,
.vsync_len = 1,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

static struct fb_videomode ed060scq_mode = {
.name = "E60SCQ",
.refresh = 85,
.xres = 800,
.yres = 600,
.pixclock = 25000000,
.left_margin = 8,
.right_margin = 60,
.upper_margin = 4,
.lower_margin = 10,
.hsync_len = 8,
.vsync_len = 4,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

static struct fb_videomode ed060sc8_mode = {
.name = "E60SC8",
.refresh = 85,
.xres = 800,
.yres = 600,
.pixclock = 30000000,
.left_margin = 8,
.right_margin = 164,
.upper_margin = 4,
.lower_margin = 18,
.hsync_len = 4,
.vsync_len = 1,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

// for ED060XC5 release by Freescale Grace 20120726 .

static struct fb_videomode ed060xc1_mode = {
.name = "E60XC1",
.refresh = 85,
.xres = 1024,
.yres = 768,
.pixclock = 40000000,
.left_margin = 12,
.right_margin = 72,
.upper_margin = 4,
.lower_margin = 5,
.hsync_len = 8,
.vsync_len = 2,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

static struct fb_videomode ed060xc5_mode = {
.name = "E60XC5",
.refresh = 85,
.xres = 1024,
.yres = 758,
.pixclock = 40000000,
.left_margin = 12,
.right_margin = 76,
.upper_margin = 4,
.lower_margin = 5,
.hsync_len = 12,
.vsync_len = 2,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

static struct fb_videomode e60_v110_mode = {
.name = "E60_V110",
.refresh = 50,
.xres = 800,
.yres = 600,
.pixclock = 18604700,
.left_margin = 8,
.right_margin = 176,
.upper_margin = 4,
.lower_margin = 2,
.hsync_len = 4,
.vsync_len = 1,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

static struct fb_videomode ed050xxx_mode = {
	.name="ED050XXXX",
	.refresh=85,
	.xres=800,
	.yres=600,
	.pixclock=26666667,
	.left_margin=4,
	.right_margin=98,
	.upper_margin=4,
	.lower_margin=9,
	.hsync_len=8,
	.vsync_len=2,
	.sync=0,
	.vmode=FB_VMODE_NONINTERLACED,
	.flag=0,
};


#ifdef EPD_TIMING_ED068TG1 //[
static struct fb_videomode ed068tg1_mode = {
.name = "ED068TG1",
.refresh=85,
.xres=1440,
.yres=1080,
.pixclock=96000000,
.left_margin=24,
.right_margin=267,
.upper_margin=4,
.lower_margin=5,
.hsync_len=24,
.vsync_len=2,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};
#elif defined(EPD_TIMING_ED068OG1_NUMCE3) //][
/* i.MX508 waveform data timing data structures for ed068og1_numce3 */
/* Created on - Monday, October 15, 2012 10:36:24
   Warning: this pixel clock is derived from 480 MHz parent! */

static struct fb_videomode ed068og1_numce3_mode = {
.name="ED068OG1_NUMCE3",
.refresh=85,
.xres=1440,
.yres=1080,
.pixclock=96000000,
.left_margin=24,
.right_margin=267,
.upper_margin=4,
.lower_margin=5,
.hsync_len=24,
.vsync_len=2,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};
#else
static struct fb_videomode ed068th1_mode = {
.name = "ED068TH1",
.refresh=85,
.xres=1440,
.yres=1080,
.pixclock=88000000,
.left_margin=24,
.right_margin=181,
.upper_margin=4,
.lower_margin=5,
.hsync_len=24,
.vsync_len=2,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};
#endif//]EPD_TIMING_ED068OG1_NUMCE3

static struct fb_videomode peng060d_mode = {
.name = "PENG060D",
.refresh=85,
.xres=1448,
.yres=1072,
.pixclock=80000000,
.left_margin=16,
.right_margin=102,
.upper_margin=4,
.lower_margin=4,
.hsync_len=28,
.vsync_len=2,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};

static struct fb_videomode ef133ut1sce_mode = {
.name="EF133UT1SCE",
.refresh=65,
.xres=1600,
.yres=1200,
.pixclock=72222223,
.left_margin=8,
.right_margin=97,
.upper_margin=4,
.lower_margin=7,
.hsync_len=12,
.vsync_len=1,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};

static struct fb_videomode ed078kh1_75Hz_mode = {
.name = "ED078KH1_75HZ",
.refresh=75,
.xres=1872,
.yres=1404,
.pixclock=120000000,
.left_margin=52,
.right_margin=75,
.upper_margin=4,
.lower_margin=14,
.hsync_len=60,
.vsync_len=2,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};
static struct fb_videomode ed078kh1_mode = {
.name = "ED078KH1",
.refresh=85,
.xres=1872,
.yres=1404,
.pixclock=133400000,
.left_margin=44,
.right_margin=89,
.upper_margin=4,
.lower_margin=5,
.hsync_len=44,
.vsync_len=1,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};
static struct fb_videomode r031_peng078f01_mode = {
	.name = "R031_PENG078F01",
	.refresh = 85,
	.xres = 1600,
	.yres = 1200,
	.pixclock = 96000000,
	.left_margin = 24,
	.right_margin = 70,
	.upper_margin = 4,
	.lower_margin = 4,
	.hsync_len = 40,
	.vsync_len = 1,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};
static struct fb_videomode es080kh1_mode = {
	.name="es080kh1",
	.refresh=85,
	.xres=1920,
	.yres=1440,
	.pixclock=160000000,
	.left_margin=36,
	.right_margin=248,
	.upper_margin=4,
	.lower_margin=8,
	.hsync_len=52,
	.vsync_len=1,
	.sync=0,
	.vmode=FB_VMODE_NONINTERLACED,
	.flag=0,
};
static struct fb_videomode ntx_ed060sh2_mode = {
	.name="ed060sh2",
	.refresh=85,
	.xres=800,
	.yres=600,
	.pixclock=25000000,
	.left_margin=20,
	.right_margin=52,
	.upper_margin=4,
	.lower_margin=3,
	.hsync_len=12,
	.vsync_len=1,
	.sync=0,
	.vmode=FB_VMODE_NONINTERLACED,
	.flag=0,
};
#if 0 
// parent clock use 400MHz
static struct fb_videomode ed070kh1_mode = {
	.name="ed070kh1",
	.refresh=85,
	.xres=1264,
	.yres=1680,
	.pixclock=133333334,
	.left_margin=28,
	.right_margin=193,
	.upper_margin=4,
	.lower_margin=12,
	.hsync_len=72,
	.vsync_len=1,
	.sync=0,
	.vmode=FB_VMODE_NONINTERLACED,
	.flag=0,
};
#else 
// parent clock use 540MHz
static struct fb_videomode ed070kh1_mode = {
	.name="ed070kh1",
	.refresh=85,
	.xres=1264,
	.yres=1680,
	.pixclock=135000000,
	.left_margin=28,
	.right_margin=204,
	.upper_margin=4,
	.lower_margin=12,
	.hsync_len=72,
	.vsync_len=1,
	.sync=0,
	.vmode=FB_VMODE_NONINTERLACED,
	.flag=0,
};
#endif
static struct fb_videomode ed070kh3_mode = {
	.name="20210422_ED070KH3",
	.refresh=85,
	.xres=1680,
	.yres=1264,
	.pixclock=120000000,
	.left_margin=24,
	.right_margin=200,
	.upper_margin=4,
	.lower_margin=14,
	.hsync_len=36,
	.vsync_len=1,
	.sync=0,
	.vmode=FB_VMODE_NONINTERLACED,
	.flag=0,
};

static struct fb_videomode ed060xhc_mode = {
	.name="20211104_ED060XHC",
	.refresh=85,
	.xres=1448,
	.yres=1072,
	.pixclock=80000000,
	.left_margin=24,
	.right_margin=80,
	.upper_margin=4,
	.lower_margin=12,
	.hsync_len=36,
	.vsync_len=1,
	.sync=0,
	.vmode=FB_VMODE_NONINTERLACED,
	.flag=0,
};

static struct imx_epdc_fb_mode panel_modes[] = {
////////////////////
{ // 0 
& ed060sc8_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
465,        /* gdclk_hp_offs */
250,        /* gdsp_offs changed delay to 8.3 uS */
0,            /* gdoe_offs */
8,            /* gdclk_offs changed delay to 4.5 SDCLK */
1,            /* num_ce */
},

////////////////////
{ // 1
& e60_v110_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
465,        /* gdclk_hp_offs */
250,        /* gdsp_offs changed delay to 8.3 uS */
0,            /* gdoe_offs */
8,            /* gdclk_offs changed delay to 4.5 SDCLK */
1,            /* num_ce */
},

////////////////////
{ // 2
& ed060xc5_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
524,        /* gdclk_hp_offs */
25,        /* gdsp_offs changed delay to 8.3 uS */
0,            /* gdoe_offs */
19,            /* gdclk_offs changed delay to 4.5 SDCLK */
1,            /* num_ce */
},

////////////////////
{ // 3
& ed060xc1_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
492,        /* gdclk_hp_offs */
29,        /* gdsp_offs changed delay to 8.3 uS */
0,            /* gdoe_offs */
23,            /* gdclk_offs changed delay to 4.5 SDCLK */
1,            /* num_ce */
},

////////////////////
{// 4
	&ed050xxx_mode, 	/* struct fb_videomode *mode */
		4, 	/* vscan_holdoff */
		10, 	/* sdoed_width */
		20, 	/* sdoed_delay */
		10, 	/* sdoez_width */
		20, 	/* sdoez_delay */
		420, 	/* GDCLK_HP */
		20, 	/* GDSP_OFF */
		0, 	/* GDOE_OFF */
		11, 	/* gdclk_offs */
		3, 	/* num_ce */
},	

#ifdef EPD_TIMING_ED068TG1 //[
{ // 5
& ed068tg1_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
665,        /* GDCLK_HP */
718,        /* GDSP_OFF */
0,            /* GDOE_OFF */
199,        /* gdclk_offs */
1,            /* num_ce */
},
#elif defined(EPD_TIMING_ED068OG1_NUMCE3)//][
{ // 5
& ed068og1_numce3_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
665,        /* GDCLK_HP */
210,        /* GDSP_OFF */
0,            /* GDOE_OFF */
199,        /* gdclk_offs */
3,            /* num_ce */
},
#else//][ !EPD_TIMING_ED068OG1_NUMCE3
{ // 5
& ed068th1_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
609,        /* GDCLK_HP */
660,        /* GDSP_OFF */
0,            /* GDOE_OFF */
184,        /* gdclk_offs */
1,            /* num_ce */
},
#endif//] EPD_TIMING_ED068OG1_NUMCE3

{ // 6
&peng060d_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
562,        /* GDCLK_HP */
662,        /* GDSP_OFF */
0,            /* GDOE_OFF */
225,        /* gdclk_offs */
3,            /* num_ce */
},
{ // 7
&ef133ut1sce_mode,
4,      /* vscan_holdoff */
10,     /* sdoed_width */
20,     /* sdoed_delay */
10,     /* sdoez_width */
20,     /* sdoez_delay */
743,    /* GDCLK_HP */
475,    /* GDSP_OFF */
0,      /* GDOE_OFF */
15,     /* gdclk_offs */
1,      /* num_ce */
},
{ // 8
&ed060scq_mode,
4,      /* vscan_holdoff */
10,     /* sdoed_width */
20,     /* sdoed_delay */
10,     /* sdoez_width */
20,     /* sdoez_delay */
438,    /* GDCLK_HP */
263,    /* GDSP_OFF */
0,      /* GDOE_OFF */
23,     /* gdclk_offs */
3,      /* num_ce */
},
{ // 9
&ed078kh1_mode,
4,      /* vscan_holdoff */
10,     /* sdoed_width */
20,     /* sdoed_delay */
10,     /* sdoez_width */
20,     /* sdoez_delay */
772,    /* GDCLK_HP */
757,    /* GDSP_OFF */
0,      /* GDOE_OFF */
199,     /* gdclk_offs */
1,      /* num_ce */
},
{ // 10
&ed060sct_mode,
4,      /* vscan_holdoff */
10,     /* sdoed_width */
20,     /* sdoed_delay */
10,     /* sdoez_width */
20,     /* sdoez_delay */
372,    /* GDCLK_HP */
367,    /* GDSP_OFF */
0,      /* GDOE_OFF */
111,     /* gdclk_offs */
1,      /* num_ce */
},
{ // 11
	&r031_peng078f01_mode,
	4,      /* vscan_holdoff */
	10,     /* sdoed_width */
	20,     /* sdoed_delay */
	10,     /* sdoez_width */
	20,     /* sdoez_delay */
	691,    /* gdclk_hp_offs */
	592,     /* gdsp_offs */
	0,      /* gdoe_offs */
	123,      /* gdclk_offs */
	2,      /* num_ce */
},
{ // 12
&ed078kh1_75Hz_mode,
4,      /* vscan_holdoff */
10,     /* sdoed_width */
20,     /* sdoed_delay */
10,     /* sdoez_width */
20,     /* sdoez_delay */
583,    /* GDCLK_HP */
939,    /* GDSP_OFF */
0,      /* GDOE_OFF */
376,     /* gdclk_offs */
3,      /* num_ce */
},
{ // 13
&es080kh1_mode, 	/* struct fb_videomode *mode */
4, 	/* vscan_holdoff */
10, 	/* sdoed_width */
20, 	/* sdoed_delay */
10, 	/* sdoez_width */
20, 	/* sdoez_delay */
972, 	/* GDCLK_HP */
721, 	/* GDSP_OFF */
0, 	/* GDOE_OFF */
71, 	/* gdclk_offs */
1, 	/* num_ce */
},
{ // 14
&ntx_ed060sh2_mode, 	/* struct fb_videomode *mode */
4, 	/* vscan_holdoff */
10, 	/* sdoed_width */
20, 	/* sdoed_delay */
10, 	/* sdoez_width */
20, 	/* sdoez_delay */
388, 	/* GDCLK_HP */
295, 	/* GDSP_OFF */
0, 	/* GDOE_OFF */
51, 	/* gdclk_offs */
1, 	/* num_ce */
},
{ // 15
&ed070kh1_mode, 	/* struct fb_videomode *mode */
#if 0 
// parent clock use 400MHz
4, 	/* vscan_holdoff */
10, 	/* sdoed_width */
20, 	/* sdoed_delay */
10, 	/* sdoez_width */
20, 	/* sdoez_delay */
737, 	/* GDCLK_HP */
547, 	/* GDSP_OFF */
0, 	/* GDOE_OFF */
83, 	/* gdclk_offs */
1, 	/* num_ce */
#else
// parent clock use 540MHz
4, 	/* vscan_holdoff */
10, 	/* sdoed_width */
20, 	/* sdoed_delay */
10, 	/* sdoez_width */
20, 	/* sdoez_delay */
746, 	/* GDCLK_HP */
553, 	/* GDSP_OFF */
0, 	/* GDOE_OFF */
83, 	/* gdclk_offs */
1,	/* num_ce */
#endif
},
	{ // 16
		&ed070kh3_mode, 	/* struct fb_videomode *mode */
		4, 	/* vscan_holdoff */
		10, 	/* sdoed_width */
		20, 	/* sdoed_delay */
		10, 	/* sdoez_width */
		20, 	/* sdoez_delay */
		912, 	/* GDCLK_HP */
		611, 	/* GDSP_OFF */
		0, 	/* GDOE_OFF */
		59, 	/* gdclk_offs */
		1, 	/* num_ce */
	},

	{ // 17 .
		&ed060xhc_mode, 	/* struct fb_videomode *mode */
		4, 	/* vscan_holdoff */
		10, 	/* sdoed_width */
		20, 	/* sdoed_delay */
		10, 	/* sdoez_width */
		20, 	/* sdoez_delay */
		576, 	/* GDCLK_HP */
		637, 	/* GDSP_OFF */
		0, 	/* GDOE_OFF */
		203, 	/* gdclk_offs */
		1, 	/* num_ce */
	},
	
};
 

#else //][!

static struct fb_videomode ed078kh1_mode = {
	.name = "ED078KH1",
	.refresh=85,
	.xres=1872,
	.yres=1404,
	.pixclock=133400000,
	.left_margin=44,
	.right_margin=89,
	.upper_margin=4,
	.lower_margin=5,
	.hsync_len=44,
	.vsync_len=1,
	.sync=0,
	.vmode=FB_VMODE_NONINTERLACED,
	.flag=0,
};
static struct fb_videomode ed060xh2c1mode = {
	.name = "ED060XH2C1",
	.refresh = 85,
	.xres = 1024,
	.yres = 758,
	.pixclock = 40000000,
	.left_margin = 12,
	.right_margin = 76,
	.upper_margin = 4,
	.lower_margin = 5,
	.hsync_len = 12,
	.vsync_len = 2,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

static struct fb_videomode e60_v110_mode = {
	.name = "E60_V110",
	.refresh = 50,
	.xres = 800,
	.yres = 600,
	.pixclock = 18604700,
	.left_margin = 8,
	.right_margin = 178,
	.upper_margin = 4,
	.lower_margin = 10,
	.hsync_len = 20,
	.vsync_len = 4,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

static struct fb_videomode e60_v220_mode = {
	.name = "E60_V220",
	.refresh = 85,
	.xres = 800,
	.yres = 600,
	.pixclock = 30000000,
	.left_margin = 8,
	.right_margin = 164,
	.upper_margin = 4,
	.lower_margin = 8,
	.hsync_len = 4,
	.vsync_len = 1,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

static struct fb_videomode e060scm_mode = {
	.name = "E060SCM",
	.refresh = 85,
	.xres = 800,
	.yres = 600,
	.pixclock = 26666667,
	.left_margin = 8,
	.right_margin = 100,
	.upper_margin = 4,
	.lower_margin = 8,
	.hsync_len = 4,
	.vsync_len = 1,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

static struct fb_videomode e97_v110_mode = {
	.name = "E97_V110",
	.refresh = 50,
	.xres = 1200,
	.yres = 825,
	.pixclock = 32000000,
	.left_margin = 12,
	.right_margin = 128,
	.upper_margin = 4,
	.lower_margin = 10,
	.hsync_len = 20,
	.vsync_len = 4,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

static struct imx_epdc_fb_mode panel_modes[] = {
	{
		&ed060xh2c1mode,	/* struct fb_videomode *mode */
		4, 	/* vscan_holdoff */
		10, 	/* sdoed_width */
		20, 	/* sdoed_delay */
		10, 	/* sdoez_width */
		20, 	/* sdoez_delay */
		524, 	/* GDCLK_HP */
		327, 	/* GDSP_OFF */
		0, 	/* GDOE_OFF */
		19, 	/* gdclk_offs */
		1, 	/* num_ce */
	},
	{
		&e60_v110_mode,
		4,      /* vscan_holdoff */
		10,     /* sdoed_width */
		20,     /* sdoed_delay */
		10,     /* sdoez_width */
		20,     /* sdoez_delay */
		428,    /* gdclk_hp_offs */
		20,     /* gdsp_offs */
		0,      /* gdoe_offs */
		1,      /* gdclk_offs */
		1,      /* num_ce */
	},
	{
		&e60_v220_mode,
		4,      /* vscan_holdoff */
		10,     /* sdoed_width */
		20,     /* sdoed_delay */
		10,     /* sdoez_width */
		20,     /* sdoez_delay */
		465,    /* gdclk_hp_offs */
		20,     /* gdsp_offs */
		0,      /* gdoe_offs */
		9,      /* gdclk_offs */
		1,      /* num_ce */
	},
	{
		&e060scm_mode,
		4,      /* vscan_holdoff */
		10,     /* sdoed_width */
		20,     /* sdoed_delay */
		10,     /* sdoez_width */
		20,     /* sdoez_delay */
		419,    /* gdclk_hp_offs */
		263,     /* gdsp_offs */
		0,      /* gdoe_offs */
		5,      /* gdclk_offs */
		1,      /* num_ce */
	},
	{
		&e97_v110_mode,
		8,      /* vscan_holdoff */
		10,     /* sdoed_width */
		20,     /* sdoed_delay */
		10,     /* sdoez_width */
		20,     /* sdoez_delay */
		632,    /* gdclk_hp_offs */
		20,     /* gdsp_offs */
		0,      /* gdoe_offs */
		1,      /* gdclk_offs */
		3,      /* num_ce */
	}
};
#endif //]

static struct imx_epdc_fb_platform_data epdc_data = {
	.epdc_mode = panel_modes,
	.num_modes = ARRAY_SIZE(panel_modes),
};

void __iomem *epdc_v2_base;

static struct mxc_epdc_fb_data *g_fb_data;

/* forward declaration */
static int mxc_epdc_fb_get_temp_index(struct mxc_epdc_fb_data *fb_data,
						int temp);
static void mxc_epdc_fb_flush_updates(struct mxc_epdc_fb_data *fb_data);
static int mxc_epdc_fb_blank(int blank, struct fb_info *info);
static int mxc_epdc_fb_init_hw(struct fb_info *info);
static int pxp_legacy_process(struct mxc_epdc_fb_data *fb_data,
			      u32 src_width, u32 src_height,
			      struct mxcfb_rect *update_region);
static int pxp_process_dithering(struct mxc_epdc_fb_data *fb_data,
			      struct mxcfb_rect *update_region);
static int pxp_wfe_a_process(struct mxc_epdc_fb_data *fb_data,
			     struct mxcfb_rect *update_region,
			     struct update_data_list *upd_data_list);
static int pxp_wfe_b_process_update(struct mxc_epdc_fb_data *fb_data,
			      struct mxcfb_rect *update_region);
static int pxp_wfe_a_process_clear_workingbuffer(struct mxc_epdc_fb_data *fb_data,
			      u32 src_width, u32 src_height);
static int pxp_complete_update(struct mxc_epdc_fb_data *fb_data, u32 *hist_stat,
				u32 *pixel_nums);

static void draw_mode0(struct mxc_epdc_fb_data *fb_data);
static bool is_free_list_full(struct mxc_epdc_fb_data *fb_data);

static void do_dithering_processing_Y1_v1_0(
		unsigned char *update_region_virt_ptr,
		dma_addr_t update_region_phys_ptr,
		struct mxcfb_rect *update_region,
		unsigned long update_region_stride,
		int *err_dist);
static void do_dithering_processing_Y4_v1_0(
		unsigned char *update_region_virt_ptr,
		dma_addr_t update_region_phys_ptr,
		struct mxcfb_rect *update_region,
		unsigned long update_region_stride,
		int *err_dist);
static inline void epdc_set_used_lut(u64 used_bit);
static inline void epdc_reset_used_lut(void);
static int pxp_clear_wb_work_func(struct mxc_epdc_fb_data *fb_data);
static int epdc_working_buffer_update(struct mxc_epdc_fb_data *fb_data,
				      struct update_data_list *upd_data_list,
				      struct mxcfb_rect *update_region);
extern void pxp_get_collision_info(struct pxp_collision_info *info);

// FW_IN_RAM //[
static void mxc_epdc_fb_fw_handler(const struct firmware *fw,void *context);
// FW_IN_RAM //]

#include "mxc_epdc_fake_s1d13522.c"

void _epdc_qos_setup(void)
{
	int ret = 0;
#ifdef QOS_ENABLE //[
		if( QOS_SOC_MX6SLL() || QOS_SOC_MX6SL() ) 
		{
			unsigned char *reg_qos_base;
			unsigned long dwQoS_epdc_offset;
			int iQoS_RD,iQoS_WR;
			void __iomem *qosc_base;
			//unsigned char *reg_qos_lcdif,*reg_qos_pxp;

			if( QOS_SOC_MX6SLL() ) {
				printk("EPDC QoS mx6sll \n");
				reg_qos_base = 0x02094000;
				dwQoS_epdc_offset = 0x1800;
				//reg_qos_epdc = 0x02095800;
				//reg_qos_lcdif = 0x02096000;
				//reg_qos_pxp = 0x02095400;
				iQoS_RD = 0xe0;
				iQoS_WR = 0xd0;
			}
			else {
				printk("EPDC QoS mx6sl \n");
				reg_qos_base = 0x02094000;
				dwQoS_epdc_offset = 0x1400;
				//reg_qos_epdc = 0x02095400;
				//reg_qos_lcdif = 0x02095800;
				//reg_qos_pxp = 0x02095c00;
				iQoS_RD = 0xe0;
				iQoS_WR = 0xd0;
			}

			reg_qos_base = ioremap(reg_qos_base,SZ_16K);
			if(reg_qos_base == NULL) {
				ret = -ENOMEM;
				printk(KERN_ERR"request qos memory failed !\n");
				goto out;
			}
			else {

				printk ("[%s-%d] old masters=0x%08X,epdQos=0x%08X\n",__func__, __LINE__,__raw_readl(reg_qos_base+0x40),__raw_readl(reg_qos_base+dwQoS_epdc_offset));
				__raw_writel(0,reg_qos_base); // Disable clkgate & soft_reset .
				__raw_writel(0,reg_qos_base+0x40); // Enable all masters .

				printk ("[%s-%d] new masters=0x%08X,epdQos=0x%08X\n",__func__, __LINE__,__raw_readl(reg_qos_base+0x40),__raw_readl(reg_qos_base+dwQoS_epdc_offset));
				__raw_writel(0,reg_qos_base+dwQoS_epdc_offset); // Disable clkgate & soft_reset .
				printk ("[%s-%d] Old QoS_WR=%08X\n",__func__, __LINE__,__raw_readl(reg_qos_base+dwQoS_epdc_offset+iQoS_WR));
				printk ("[%s-%d] Old QoS_RD=%08X\n",__func__, __LINE__,__raw_readl(reg_qos_base+dwQoS_epdc_offset+iQoS_RD));
				__raw_writel(0x0f020722,reg_qos_base+dwQoS_epdc_offset+iQoS_WR); // Write QOS,init = 7 with red flag .
				__raw_writel(0x0f020722,reg_qos_base+dwQoS_epdc_offset+iQoS_RD); // Read QOS, init = 7 with red flag .
				printk ("[%s-%d] New QoS_WR=%08X\n",__func__, __LINE__,__raw_readl(reg_qos_base+dwQoS_epdc_offset+iQoS_WR));
				printk ("[%s-%d] New QoS_RD=%08X\n",__func__, __LINE__,__raw_readl(reg_qos_base+dwQoS_epdc_offset+iQoS_RD));

				__raw_readl(reg_qos_base+dwQoS_epdc_offset); // check the QOS value .

				iounmap(reg_qos_base);
			}
		}
		else 
		if( QOS_SOC_MX6ULL() ) {
			/* enable the QoS feature to make sure the EPDC has the highest priority. */
			#define	MX6ULL_GPV0_BASE_ADDR	0x00B00000
			#define	MX6ULL_PXP_OFFSET_ADDR	69*0x1000
			#define	MX6ULL_CSI_OFFSET_ADDR	70*0x1000
			#define	MX6ULL_LCD_OFFSET_ADDR	71*0x1000
			#define	MX6ULL_EPD_OFFSET_ADDR	72*0x1000

			#define	MX6ULL_QOS_RD	0x100
			#define	MX6ULL_QOS_WR	0x104
			void __iomem *nic301_base;
			nic301_base = ioremap(MX6ULL_GPV0_BASE_ADDR, SZ_1M);
			if (nic301_base == NULL) {
				ret = -ENOMEM;
				goto out;
			} else {
				__raw_writel(0x7, nic301_base + MX6ULL_EPD_OFFSET_ADDR  + MX6ULL_QOS_WR); //default 2
				__raw_writel(0x7, nic301_base + MX6ULL_EPD_OFFSET_ADDR  + MX6ULL_QOS_RD); //default 2

				printk("EPD QOS_WR 0x%x, QOS_RD 0x%x\n",
				__raw_readl( nic301_base + MX6ULL_EPD_OFFSET_ADDR  + MX6ULL_QOS_WR),
				__raw_readl( nic301_base + MX6ULL_EPD_OFFSET_ADDR  + MX6ULL_QOS_RD));
			}
			
		}
		else {
			printk(KERN_ERR"EPDC_QoS not supported on this platform !\n");
		}
	
#endif //]QOS_ENABLE
out:
		return ;
}
/* include advance waveform buffer processing routines */
#include "mxc_epdc_v2_adwbf.c"

#ifdef DEBUG
static void dump_pxp_config(struct mxc_epdc_fb_data *fb_data,
			    struct pxp_config_data *pxp_conf)
{
	dev_info(fb_data->dev, "S0 fmt 0x%x",
		pxp_conf->s0_param.pixel_fmt);
	dev_info(fb_data->dev, "S0 width 0x%x",
		pxp_conf->s0_param.width);
	dev_info(fb_data->dev, "S0 height 0x%x",
		pxp_conf->s0_param.height);
	dev_info(fb_data->dev, "S0 ckey 0x%x",
		pxp_conf->s0_param.color_key);
	dev_info(fb_data->dev, "S0 ckey en 0x%x",
		pxp_conf->s0_param.color_key_enable);

	dev_info(fb_data->dev, "OL0 combine en 0x%x",
		pxp_conf->ol_param[0].combine_enable);
	dev_info(fb_data->dev, "OL0 fmt 0x%x",
		pxp_conf->ol_param[0].pixel_fmt);
	dev_info(fb_data->dev, "OL0 width 0x%x",
		pxp_conf->ol_param[0].width);
	dev_info(fb_data->dev, "OL0 height 0x%x",
		pxp_conf->ol_param[0].height);
	dev_info(fb_data->dev, "OL0 ckey 0x%x",
		pxp_conf->ol_param[0].color_key);
	dev_info(fb_data->dev, "OL0 ckey en 0x%x",
		pxp_conf->ol_param[0].color_key_enable);
	dev_info(fb_data->dev, "OL0 alpha 0x%x",
		pxp_conf->ol_param[0].global_alpha);
	dev_info(fb_data->dev, "OL0 alpha en 0x%x",
		pxp_conf->ol_param[0].global_alpha_enable);
	dev_info(fb_data->dev, "OL0 local alpha en 0x%x",
		pxp_conf->ol_param[0].local_alpha_enable);

	dev_info(fb_data->dev, "Out fmt 0x%x",
		pxp_conf->out_param.pixel_fmt);
	dev_info(fb_data->dev, "Out width 0x%x",
		pxp_conf->out_param.width);
	dev_info(fb_data->dev, "Out height 0x%x",
		pxp_conf->out_param.height);

	dev_info(fb_data->dev,
		"drect left 0x%x right 0x%x width 0x%x height 0x%x",
		pxp_conf->proc_data.drect.left, pxp_conf->proc_data.drect.top,
		pxp_conf->proc_data.drect.width,
		pxp_conf->proc_data.drect.height);
	dev_info(fb_data->dev,
		"srect left 0x%x right 0x%x width 0x%x height 0x%x",
		pxp_conf->proc_data.srect.left, pxp_conf->proc_data.srect.top,
		pxp_conf->proc_data.srect.width,
		pxp_conf->proc_data.srect.height);
	dev_info(fb_data->dev, "Scaling en 0x%x", pxp_conf->proc_data.scaling);
	dev_info(fb_data->dev, "HFlip en 0x%x", pxp_conf->proc_data.hflip);
	dev_info(fb_data->dev, "VFlip en 0x%x", pxp_conf->proc_data.vflip);
	dev_info(fb_data->dev, "Rotation 0x%x", pxp_conf->proc_data.rotate);
	dev_info(fb_data->dev, "BG Color 0x%x", pxp_conf->proc_data.bgcolor);
}

static void dump_epdc_reg(void)
{
	printk(KERN_DEBUG "\n\n");
	printk(KERN_DEBUG "EPDC_CTRL 0x%x\n", __raw_readl(EPDC_CTRL));
	printk(KERN_DEBUG "EPDC_WVADDR 0x%x\n", __raw_readl(EPDC_WVADDR));
	printk(KERN_DEBUG "EPDC_WB_ADDR 0x%x\n", __raw_readl(EPDC_WB_ADDR));
	printk(KERN_DEBUG "EPDC_RES 0x%x\n", __raw_readl(EPDC_RES));
	printk(KERN_DEBUG "EPDC_FORMAT 0x%x\n", __raw_readl(EPDC_FORMAT));
	printk(KERN_DEBUG "EPDC_FIFOCTRL 0x%x\n", __raw_readl(EPDC_FIFOCTRL));
	printk(KERN_DEBUG "EPDC_UPD_ADDR 0x%x\n", __raw_readl(EPDC_UPD_ADDR));
	printk(KERN_DEBUG "EPDC_UPD_STRIDE 0x%x\n", __raw_readl(EPDC_UPD_STRIDE));
	printk(KERN_DEBUG "EPDC_UPD_FIXED 0x%x\n", __raw_readl(EPDC_UPD_FIXED));
	printk(KERN_DEBUG "EPDC_UPD_CORD 0x%x\n", __raw_readl(EPDC_UPD_CORD));
	printk(KERN_DEBUG "EPDC_UPD_SIZE 0x%x\n", __raw_readl(EPDC_UPD_SIZE));
	printk(KERN_DEBUG "EPDC_UPD_CTRL 0x%x\n", __raw_readl(EPDC_UPD_CTRL));
	printk(KERN_DEBUG "EPDC_TEMP 0x%x\n", __raw_readl(EPDC_TEMP));
	printk(KERN_DEBUG "EPDC_AUTOWV_LUT 0x%x\n", __raw_readl(EPDC_AUTOWV_LUT));
	printk(KERN_DEBUG "EPDC_TCE_CTRL 0x%x\n", __raw_readl(EPDC_TCE_CTRL));
	printk(KERN_DEBUG "EPDC_TCE_SDCFG 0x%x\n", __raw_readl(EPDC_TCE_SDCFG));
	printk(KERN_DEBUG "EPDC_TCE_GDCFG 0x%x\n", __raw_readl(EPDC_TCE_GDCFG));
	printk(KERN_DEBUG "EPDC_TCE_HSCAN1 0x%x\n", __raw_readl(EPDC_TCE_HSCAN1));
	printk(KERN_DEBUG "EPDC_TCE_HSCAN2 0x%x\n", __raw_readl(EPDC_TCE_HSCAN2));
	printk(KERN_DEBUG "EPDC_TCE_VSCAN 0x%x\n", __raw_readl(EPDC_TCE_VSCAN));
	printk(KERN_DEBUG "EPDC_TCE_OE 0x%x\n", __raw_readl(EPDC_TCE_OE));
	printk(KERN_DEBUG "EPDC_TCE_POLARITY 0x%x\n", __raw_readl(EPDC_TCE_POLARITY));
	printk(KERN_DEBUG "EPDC_TCE_TIMING1 0x%x\n", __raw_readl(EPDC_TCE_TIMING1));
	printk(KERN_DEBUG "EPDC_TCE_TIMING2 0x%x\n", __raw_readl(EPDC_TCE_TIMING2));
	printk(KERN_DEBUG "EPDC_TCE_TIMING3 0x%x\n", __raw_readl(EPDC_TCE_TIMING3));
	printk(KERN_DEBUG "EPDC_PIGEON_CTRL0 0x%x\n", __raw_readl(EPDC_PIGEON_CTRL0));
	printk(KERN_DEBUG "EPDC_PIGEON_CTRL1 0x%x\n", __raw_readl(EPDC_PIGEON_CTRL1));
	printk(KERN_DEBUG "EPDC_IRQ_MASK1 0x%x\n", __raw_readl(EPDC_IRQ_MASK1));
	printk(KERN_DEBUG "EPDC_IRQ_MASK2 0x%x\n", __raw_readl(EPDC_IRQ_MASK2));
	printk(KERN_DEBUG "EPDC_IRQ1 0x%x\n", __raw_readl(EPDC_IRQ1));
	printk(KERN_DEBUG "EPDC_IRQ2 0x%x\n", __raw_readl(EPDC_IRQ2));
	printk(KERN_DEBUG "EPDC_IRQ_MASK 0x%x\n", __raw_readl(EPDC_IRQ_MASK));
	printk(KERN_DEBUG "EPDC_IRQ 0x%x\n", __raw_readl(EPDC_IRQ));
	printk(KERN_DEBUG "EPDC_STATUS_LUTS 0x%x\n", __raw_readl(EPDC_STATUS_LUTS));
	printk(KERN_DEBUG "EPDC_STATUS_LUTS2 0x%x\n", __raw_readl(EPDC_STATUS_LUTS2));
	printk(KERN_DEBUG "EPDC_STATUS_NEXTLUT 0x%x\n", __raw_readl(EPDC_STATUS_NEXTLUT));
	printk(KERN_DEBUG "EPDC_STATUS_COL1 0x%x\n", __raw_readl(EPDC_STATUS_COL));
	printk(KERN_DEBUG "EPDC_STATUS_COL2 0x%x\n", __raw_readl(EPDC_STATUS_COL2));
	printk(KERN_DEBUG "EPDC_STATUS 0x%x\n", __raw_readl(EPDC_STATUS));
	printk(KERN_DEBUG "EPDC_UPD_COL_CORD 0x%x\n", __raw_readl(EPDC_UPD_COL_CORD));
	printk(KERN_DEBUG "EPDC_UPD_COL_SIZE 0x%x\n", __raw_readl(EPDC_UPD_COL_SIZE));
	printk(KERN_DEBUG "EPDC_DEBUG 0x%x\n", __raw_readl(EPDC_DEBUG));
	printk(KERN_DEBUG "EPDC_DEBUG_LUT 0x%x\n", __raw_readl(EPDC_DEBUG_LUT));
	printk(KERN_DEBUG "EPDC_HIST1_PARAM 0x%x\n", __raw_readl(EPDC_HIST1_PARAM));
	printk(KERN_DEBUG "EPDC_HIST2_PARAM 0x%x\n", __raw_readl(EPDC_HIST2_PARAM));
	printk(KERN_DEBUG "EPDC_HIST4_PARAM 0x%x\n", __raw_readl(EPDC_HIST4_PARAM));
	printk(KERN_DEBUG "EPDC_HIST8_PARAM0 0x%x\n", __raw_readl(EPDC_HIST8_PARAM0));
	printk(KERN_DEBUG "EPDC_HIST8_PARAM1 0x%x\n", __raw_readl(EPDC_HIST8_PARAM1));
	printk(KERN_DEBUG "EPDC_HIST16_PARAM0 0x%x\n", __raw_readl(EPDC_HIST16_PARAM0));
	printk(KERN_DEBUG "EPDC_HIST16_PARAM1 0x%x\n", __raw_readl(EPDC_HIST16_PARAM1));
	printk(KERN_DEBUG "EPDC_HIST16_PARAM2 0x%x\n", __raw_readl(EPDC_HIST16_PARAM2));
	printk(KERN_DEBUG "EPDC_HIST16_PARAM3 0x%x\n", __raw_readl(EPDC_HIST16_PARAM3));
	printk(KERN_DEBUG "EPDC_GPIO 0x%x\n", __raw_readl(EPDC_GPIO));
	printk(KERN_DEBUG "EPDC_VERSION 0x%x\n", __raw_readl(EPDC_VERSION));
	printk(KERN_DEBUG "\n\n");
}

static void dump_update_data(struct device *dev,
			     struct update_data_list *upd_data_list)
{
	dev_info(dev,
		"X = %d, Y = %d, Width = %d, Height = %d, WaveMode = %d, "
		"LUT = %d, Coll Mask = 0x%llx, order = %d\n",
		upd_data_list->update_desc->upd_data.update_region.left,
		upd_data_list->update_desc->upd_data.update_region.top,
		upd_data_list->update_desc->upd_data.update_region.width,
		upd_data_list->update_desc->upd_data.update_region.height,
		upd_data_list->update_desc->upd_data.waveform_mode,
		upd_data_list->lut_num,
		upd_data_list->collision_mask,
		upd_data_list->update_desc->update_order);
}

static void dump_collision_list(struct mxc_epdc_fb_data *fb_data)
{
	struct update_data_list *plist;

	dev_info(fb_data->dev, "Collision List:\n");
	if (list_empty(&fb_data->upd_buf_collision_list))
		dev_info(fb_data->dev, "Empty");
	list_for_each_entry(plist, &fb_data->upd_buf_collision_list, list) {
		dev_info(fb_data->dev, "Virt Addr = 0x%x, Phys Addr = 0x%x ",
			(u32)plist->virt_addr, plist->phys_addr);
		dump_update_data(fb_data->dev, plist);
	}
}

static void dump_free_list(struct mxc_epdc_fb_data *fb_data)
{
	struct update_data_list *plist;

	dev_info(fb_data->dev, "Free List:\n");
	if (list_empty(&fb_data->upd_buf_free_list))
		dev_info(fb_data->dev, "Empty");
	list_for_each_entry(plist, &fb_data->upd_buf_free_list, list)
		dev_info(fb_data->dev, "Virt Addr = 0x%x, Phys Addr = 0x%x ",
			(u32)plist->virt_addr, plist->phys_addr);
}

static void dump_queue(struct mxc_epdc_fb_data *fb_data)
{
	struct update_data_list *plist;

	dev_info(fb_data->dev, "Queue:\n");
	if (list_empty(&fb_data->upd_buf_queue))
		dev_info(fb_data->dev, "Empty");
	list_for_each_entry(plist, &fb_data->upd_buf_queue, list) {
		dev_info(fb_data->dev, "Virt Addr = 0x%x, Phys Addr = 0x%x ",
			(u32)plist->virt_addr, plist->phys_addr);
		dump_update_data(fb_data->dev, plist);
	}
}

static void dump_desc_data(struct device *dev,
			     struct update_desc_list *upd_desc_list)
{
	dev_info(dev,
		"X = %d, Y = %d, Width = %d, Height = %d, WaveMode = %d, "
		"order = %d\n",
		upd_desc_list->upd_data.update_region.left,
		upd_desc_list->upd_data.update_region.top,
		upd_desc_list->upd_data.update_region.width,
		upd_desc_list->upd_data.update_region.height,
		upd_desc_list->upd_data.waveform_mode,
		upd_desc_list->update_order);
}

static void dump_pending_list(struct mxc_epdc_fb_data *fb_data)
{
	struct update_desc_list *plist;

	dev_info(fb_data->dev, "Queue:\n");
	if (list_empty(&fb_data->upd_pending_list))
		dev_info(fb_data->dev, "Empty");
	list_for_each_entry(plist, &fb_data->upd_pending_list, list)
		dump_desc_data(fb_data->dev, plist);
}

static void dump_all_updates(struct mxc_epdc_fb_data *fb_data)
{
	dump_free_list(fb_data);
	dump_queue(fb_data);
	dump_collision_list(fb_data);
	dev_info(fb_data->dev, "Current update being processed:\n");
	if (fb_data->cur_update == NULL)
		dev_info(fb_data->dev, "No current update\n");
	else
		dump_update_data(fb_data->dev, fb_data->cur_update);
}

static void dump_fw_header(struct device *dev,
			   struct mxcfb_waveform_data_file *fw)
{
	dev_dbg(dev, "Firmware Header:\n");
	dev_dbg(dev, "wi0         0x%08x\n", fw->wdh.wi0);
	dev_dbg(dev, "wi1         0x%08x\n", fw->wdh.wi1);
	dev_dbg(dev, "wi2         0x%08x\n", fw->wdh.wi2);
	dev_dbg(dev, "wi3         0x%08x\n", fw->wdh.wi3);
	dev_dbg(dev, "wi4         0x%08x\n", fw->wdh.wi4);
	dev_dbg(dev, "wi5         0x%08x\n", fw->wdh.wi5);
	dev_dbg(dev, "wi6         0x%08x\n", fw->wdh.wi6);
	dev_dbg(dev, "xwia:24     0x%06x\n", fw->wdh.xwia);
	dev_dbg(dev, "cs1:8       0x%02x\n", fw->wdh.cs1);
	dev_dbg(dev, "wmta:24     0x%06x\n", fw->wdh.wmta);
	dev_dbg(dev, "fvsn:8      0x%02x\n", fw->wdh.fvsn);
	dev_dbg(dev, "luts:8      0x%02x\n", fw->wdh.luts);
	dev_dbg(dev, "mc:8        0x%02x\n", fw->wdh.mc);
	dev_dbg(dev, "trc:8       0x%02x\n", fw->wdh.trc);
	dev_dbg(dev, "awf         0x%02x\n", fw->wdh.awf);
	dev_dbg(dev, "eb:8        0x%02x\n", fw->wdh.eb);
	dev_dbg(dev, "sb:8        0x%02x\n", fw->wdh.sb);
	dev_dbg(dev, "reserved0_1 0x%02x\n", fw->wdh.reserved0_1);
	dev_dbg(dev, "reserved0_2 0x%02x\n", fw->wdh.reserved0_2);
	dev_dbg(dev, "reserved0_3 0x%02x\n", fw->wdh.reserved0_3);
	dev_dbg(dev, "reserved0_4 0x%02x\n", fw->wdh.reserved0_4);
	dev_dbg(dev, "reserved0_5 0x%02x\n", fw->wdh.reserved0_5);
	dev_dbg(dev, "cs2:8       0x%02x\n", fw->wdh.cs2);
}

#else
static inline void dump_pxp_config(struct mxc_epdc_fb_data *fb_data,
				   struct pxp_config_data *pxp_conf) {}
static inline void dump_epdc_reg(void) {}
static inline void dump_update_data(struct device *dev,
			     struct update_data_list *upd_data_list) {}
static inline void dump_collision_list(struct mxc_epdc_fb_data *fb_data) {}
static inline void dump_free_list(struct mxc_epdc_fb_data *fb_data) {}
static inline void dump_queue(struct mxc_epdc_fb_data *fb_data) {}
static inline void dump_all_updates(struct mxc_epdc_fb_data *fb_data) {}
static void dump_fw_header(struct device *dev,
			   struct mxcfb_waveform_data_file *fw) {}

#endif

/* 
 * EPDC Voltage Control data handler
 */
struct epd_vc_data {
		unsigned version:16;
		unsigned v1:16;
		unsigned v2:16;
		unsigned v3:16;
		unsigned v4:16;
		unsigned v5:16;
		unsigned v6:16;
		unsigned v7:8;
		u8	cs:8;
	};
void fetch_Epdc_Pmic_Voltages( struct epd_vc_data *vcd, struct mxc_epdc_fb_data *fb_data,
	u32 waveform_mode,
	u32 waveform_tempRange)
{
	/* fetch and display the voltage control data  */
	if (fb_data->waveform_vcd_buffer) {

		/* fetch the voltage control data */
		if (mxc_epdc_fb_fetch_vc_data( fb_data->waveform_vcd_buffer, waveform_mode, waveform_tempRange, fb_data->waveform_mc, fb_data->waveform_trc, (unsigned char *) vcd) < 0)
			dev_err(fb_data->dev, " *** Extra Waveform Data checksum error ***\n");
		else
			dev_dbg(fb_data->dev, " -- VC Data version 0x%04x : v1 = 0x%04x, v2 = 0x%04x, v3 = 0x%04x, v4 = 0x%04x, v5 = 0x%04x, v6 = 0x%04x, v7 = 0x%02x --\n",
				vcd->version, vcd->v1, vcd->v2, vcd->v3, vcd->v4, vcd->v5, vcd->v6, vcd->v7 );

	}
}

/********************************************************
 * Start Low-Level EPDC Functions
 ********************************************************/

static inline void epdc_lut_complete_intr(int rev, u32 lut_num, bool enable)
{
	if (rev < 20) {
		if (enable)
			__raw_writel(1 << lut_num, EPDC_IRQ_MASK_SET);
		else
			__raw_writel(1 << lut_num, EPDC_IRQ_MASK_CLEAR);
	} else {
		if (enable) {
			if (lut_num < 32)
				__raw_writel(1 << lut_num, EPDC_IRQ_MASK1_SET);
			else
				__raw_writel(1 << (lut_num - 32),
					EPDC_IRQ_MASK2_SET);
		} else {
			if (lut_num < 32)
				__raw_writel(1 << lut_num,
					EPDC_IRQ_MASK1_CLEAR);
			else
				__raw_writel(1 << (lut_num - 32),
					EPDC_IRQ_MASK2_CLEAR);
		}
	}
}

static inline void epdc_working_buf_intr(bool enable)
{
	if (enable)
		__raw_writel(EPDC_IRQ_WB_CMPLT_IRQ, EPDC_IRQ_MASK_SET);
	else
		__raw_writel(EPDC_IRQ_WB_CMPLT_IRQ, EPDC_IRQ_MASK_CLEAR);
}

static inline void epdc_clear_working_buf_irq(void)
{
	__raw_writel(EPDC_IRQ_WB_CMPLT_IRQ | EPDC_IRQ_LUT_COL_IRQ,
		     EPDC_IRQ_CLEAR);
}

static inline void epdc_eof_intr(bool enable)
{
	if (enable)
		__raw_writel(EPDC_IRQ_FRAME_END_IRQ, EPDC_IRQ_MASK_SET);
	else
		__raw_writel(EPDC_IRQ_FRAME_END_IRQ, EPDC_IRQ_MASK_CLEAR);
}

static inline void epdc_clear_eof_irq(void)
{
	__raw_writel(EPDC_IRQ_FRAME_END_IRQ, EPDC_IRQ_CLEAR);
}

static inline bool epdc_signal_eof(void)
{
	return (__raw_readl(EPDC_IRQ_MASK) & __raw_readl(EPDC_IRQ)
		& EPDC_IRQ_FRAME_END_IRQ) ? true : false;
}

static inline void epdc_set_temp(u32 temp)
{
	int ret = 0;
	/* used to store external panel temperature value */
	unsigned int ext_temp, ext_temp_index = temp;

	if (temp == DEFAULT_TEMP_INDEX) {
#ifdef CONFIG_MFD_MAX17135 //[
		ret = max17135_reg_read(REG_MAX17135_EXT_TEMP, &ext_temp);
		if (ret == 0) {
			ext_temp = ext_temp >> 8;
			dev_dbg(g_fb_data->dev, "the current external temperature is %d\n",
				ext_temp);
			ext_temp_index = mxc_epdc_fb_get_temp_index(g_fb_data, ext_temp);
		}
#endif //] CONFIG_MFD_MAX17135
	}


	EPDC_VPRINT(g_fb_data,2,"%s(),temp_idx=%hd\n",__FUNCTION__,ext_temp_index);

	__raw_writel(ext_temp_index, EPDC_TEMP);
}

static inline void epdc_set_screen_res(u32 width, u32 height)
{
	u32 val = (height << EPDC_RES_VERTICAL_OFFSET) | width;
	__raw_writel(val, EPDC_RES);
}

static inline void epdc_set_update_addr(u32 addr)
{
#ifdef	EPDC_STANDARD_MODE
	__raw_writel(0, EPDC_UPD_ADDR);
#else
	__raw_writel(addr, EPDC_UPD_ADDR);
#endif
}

static inline void epdc_set_update_coord(u32 x, u32 y)
{
	EPDC_VPRINT(g_fb_data,1,"%s(),x=%hd,y=%hd\n",__FUNCTION__,x,y);
	u32 val = (y << EPDC_UPD_CORD_YCORD_OFFSET) | x;
	__raw_writel(val, EPDC_UPD_CORD);

	if(g_fb_data) {
		g_fb_data->latest_update_region.left = x;
		g_fb_data->latest_update_region.top = y;
	}
	else {
		WARNING_MSG("%s(): epdc not probed !\n",__FUNCTION__);
	}

	ntx_epdc_set_update_coord(x,y);
}

static inline void epdc_set_update_dimensions(u32 width, u32 height)
{

	EPDC_VPRINT(g_fb_data,1,"%s(),w=%hd,h=%hd\n",__FUNCTION__,width,height);
	u32 val = (height << EPDC_UPD_SIZE_HEIGHT_OFFSET) | width;
	__raw_writel(val, EPDC_UPD_SIZE);

	if(g_fb_data) {
		g_fb_data->latest_update_region.width = width;
		g_fb_data->latest_update_region.height = height;
	}
	else {
		WARNING_MSG("%s(): epdc not probed !\n",__FUNCTION__);
	}

	ntx_epdc_set_update_dimensions(width,height);
}

#ifdef MXCFB_WAVEFORM_MODES_NTX //[
static void epdc_set_update_waveform(struct mxcfb_waveform_modes_ntx *wv_modes)
#else //][!MXCFB_WAVEFORM_MODES_NTX
static void epdc_set_update_waveform(struct mxcfb_waveform_modes *wv_modes)
#endif //] MXCFB_WAVEFORM_MODES_NTX
{
	u32 val;

#ifdef EPDC_STANDARD_MODE
	return;
#endif

	/* Configure the auto-waveform look-up table based on waveform modes */

	/* Entry 1 = DU, 2 = GC4, 3 = GC8, etc. */
	val = (wv_modes->mode_du << EPDC_AUTOWV_LUT_DATA_OFFSET) |
		(0 << EPDC_AUTOWV_LUT_ADDR_OFFSET);
	__raw_writel(val, EPDC_AUTOWV_LUT);
	val = (wv_modes->mode_du << EPDC_AUTOWV_LUT_DATA_OFFSET) |
		(1 << EPDC_AUTOWV_LUT_ADDR_OFFSET);
	__raw_writel(val, EPDC_AUTOWV_LUT);
	val = (wv_modes->mode_gc4 << EPDC_AUTOWV_LUT_DATA_OFFSET) |
		(2 << EPDC_AUTOWV_LUT_ADDR_OFFSET);
	__raw_writel(val, EPDC_AUTOWV_LUT);
	val = (wv_modes->mode_gc8 << EPDC_AUTOWV_LUT_DATA_OFFSET) |
		(3 << EPDC_AUTOWV_LUT_ADDR_OFFSET);
	__raw_writel(val, EPDC_AUTOWV_LUT);
	val = (wv_modes->mode_gc16 << EPDC_AUTOWV_LUT_DATA_OFFSET) |
		(4 << EPDC_AUTOWV_LUT_ADDR_OFFSET);
	__raw_writel(val, EPDC_AUTOWV_LUT);
	val = (wv_modes->mode_gc32 << EPDC_AUTOWV_LUT_DATA_OFFSET) |
		(5 << EPDC_AUTOWV_LUT_ADDR_OFFSET);
	__raw_writel(val, EPDC_AUTOWV_LUT);
}

static void epdc_set_update_stride(u32 stride)
{
#ifdef EPDC_STANDARD_MODE
	__raw_writel(0, EPDC_UPD_STRIDE);
#else
	__raw_writel(stride, EPDC_UPD_STRIDE);
#endif
}

static void epdc_submit_update(u32 lut_num, u32 waveform_mode, u32 update_mode,
			       bool use_dry_run, bool use_test_mode, u32 np_val)
{
	u32 reg_val = 0;


	if(g_fb_data->vcom_off_with_data) {
	 if (!regulator_is_enabled(g_fb_data->vcom_regulator))
	  	regulator_enable(g_fb_data->vcom_regulator);
	}
	
	NTX_TimeStamp_printf("epdc_submite_update",lut_num,"wfm=%hd,updm=%hd;",
			waveform_mode,update_mode);

	if(giLast_waveform_mode!=waveform_mode) {
		if( g_fb_data->wv_modes.mode_a2==waveform_mode &&
				g_fb_data->wv_modes.mode_du!=giLast_waveform_mode) 
		{
			waveform_mode=g_fb_data->wv_modes.mode_du;
			EPDC_VPRINT(g_fb_data,1,"%s():waveform mode has been force chage to DU before A2\n",__FUNCTION__);
		}

	}
	#if 1 //[ 
	
	if( ( (waveform_mode==g_fb_data->wv_modes.mode_glkw16 && \
				(g_fb_data->wv_modes.mode_gl16!=g_fb_data->wv_modes.mode_glkw16)) ||\
			(waveform_mode==g_fb_data->wv_modes.mode_gck16 &&\
				(g_fb_data->wv_modes.mode_gc16!=g_fb_data->wv_modes.mode_gck16)) ) ) 
	{
		EPDC_VPRINT(g_fb_data,3,"nightmode wf mode=%d\n",waveform_mode);
		regulator_setflags(g_fb_data->display_regulator,EPD_PMIC_FLAGS_NIGHTMODE);
	}
	else 
	{
		regulator_setflags(g_fb_data->display_regulator,0);
	}
	#endif //]

	EPDC_VPRINT(g_fb_data,1,"%s(%d):%s(),lut=%d,wf_mode=%d,last_wf_mode=%d,upd_mode=%d,test=%d,np_val=%d\n",
		__FILE__,__LINE__,__FUNCTION__,lut_num,waveform_mode,giLast_waveform_mode,update_mode,use_test_mode,np_val);
	
	if(0!=waveform_mode) {
#if 0
		g_fb_data->lut_rect[lut_num].left = g_fb_data->cur_update->update_desc->upd_data.update_region.left;
		g_fb_data->lut_rect[lut_num].top = g_fb_data->cur_update->update_desc->upd_data.update_region.top;
		g_fb_data->lut_rect[lut_num].width = g_fb_data->cur_update->update_desc->upd_data.update_region.width;
		g_fb_data->lut_rect[lut_num].height = g_fb_data->cur_update->update_desc->upd_data.update_region.height;
#else 
		g_fb_data->lut_rect[lut_num].left = g_fb_data->latest_update_region.left;
		g_fb_data->lut_rect[lut_num].top = g_fb_data->latest_update_region.top;
		g_fb_data->lut_rect[lut_num].width = g_fb_data->latest_update_region.width;
		g_fb_data->lut_rect[lut_num].height = g_fb_data->latest_update_region.height;
#endif
		g_fb_data->active_updating_w += g_fb_data->latest_update_region.width;
		g_fb_data->active_updating_h += g_fb_data->latest_update_region.height;
	}
	EPDC_VPRINT(g_fb_data,2,"%s(),updating w=%hd,h=%hd\n",__FUNCTION__,g_fb_data->active_updating_w,g_fb_data->active_updating_h);


	if (use_test_mode) {
		reg_val |=
		    ((np_val << EPDC_UPD_FIXED_FIXNP_OFFSET) &
		     EPDC_UPD_FIXED_FIXNP_MASK) | EPDC_UPD_FIXED_FIXNP_EN;
		reg_val |=
		    ((np_val << EPDC_UPD_FIXED_FIXCP_OFFSET) &
		     EPDC_UPD_FIXED_FIXCP_MASK) | EPDC_UPD_FIXED_FIXCP_EN;

		__raw_writel(reg_val, EPDC_UPD_FIXED);

		reg_val = EPDC_UPD_CTRL_USE_FIXED;
	} else {
		__raw_writel(reg_val, EPDC_UPD_FIXED);
	}

	if (waveform_mode == WAVEFORM_MODE_AUTO)
		reg_val |= EPDC_UPD_CTRL_AUTOWV;
	else
		reg_val |= ((waveform_mode <<
				EPDC_UPD_CTRL_WAVEFORM_MODE_OFFSET) &
				EPDC_UPD_CTRL_WAVEFORM_MODE_MASK);

	reg_val |= (use_dry_run ? EPDC_UPD_CTRL_DRY_RUN : 0) |
	    ((lut_num << EPDC_UPD_CTRL_LUT_SEL_OFFSET) &
	     EPDC_UPD_CTRL_LUT_SEL_MASK) |
	    update_mode;

#ifdef EPDC_STANDARD_MODE
	reg_val |= 0x80000000;

	epdc_set_used_lut(lut_num);
#endif
	dump_epdc_reg();

	__raw_writel(reg_val, EPDC_UPD_CTRL);
	giLast_waveform_mode = waveform_mode;
	
#ifdef TCE_UNDERRUN_PREVENT_WORKFUNC //[

	if( g_fb_data&&
			g_fb_data->latest_update_region.width>=TCE_UNDERRUN_PREVENT_X_RES && 
			g_fb_data->latest_update_region.height>=TCE_UNDERRUN_PREVENT_Y_RES && 
			( (waveform_mode!=g_fb_data->wv_modes.mode_a2) &&
				(waveform_mode!=g_fb_data->wv_modes.mode_du) )
		)
	{
		if(g_fb_data->tce_safe_work_running) {
			g_fb_data->tce_safe_work_cancel = 1;
		}
		if(delayed_work_pending(&g_fb_data->tce_safe_work))
			cancel_delayed_work_sync(&g_fb_data->tce_safe_work);
		schedule_delayed_work(&g_fb_data->tce_safe_work,msecs_to_jiffies(80));
	}
#endif //] TCE_UNDERRUN_PREVENT_WORKFUNC


	if(g_fb_data) {
		g_fb_data->lastest_lut_num=lut_num;
		g_fb_data->luts_updating |= 1<<lut_num;
	}
	
}

static inline bool epdc_is_lut_complete(int rev, u32 lut_num)
{
	u32 val;
	bool is_compl;
	if (rev < 20) {
		val = __raw_readl(EPDC_IRQ);
		is_compl = val & (1 << lut_num) ? true : false;
	} else if (lut_num < 32) {
		val = __raw_readl(EPDC_IRQ1);
		is_compl = val & (1 << lut_num) ? true : false;
	} else {
		val = __raw_readl(EPDC_IRQ2);
		is_compl = val & (1 << (lut_num - 32)) ? true : false;
	}

	return is_compl;
}

static inline void epdc_clear_lut_complete_irq(int rev, u32 lut_num)
{
	if (rev < 20)
		__raw_writel(1 << lut_num, EPDC_IRQ_CLEAR);
	else if (lut_num < 32)
		__raw_writel(1 << lut_num, EPDC_IRQ1_CLEAR);
	else
		__raw_writel(1 << (lut_num - 32), EPDC_IRQ2_CLEAR);
}

static inline bool epdc_is_lut_active(u32 lut_num)
{
	u32 val;
	bool is_active;

	if (lut_num < 32) {
		val = __raw_readl(EPDC_STATUS_LUTS);
		is_active = val & (1 << lut_num) ? true : false;
	} else {
		val = __raw_readl(EPDC_STATUS_LUTS2);
		is_active = val & (1 << (lut_num - 32)) ? true : false;
	}

	return is_active;
}

static inline bool epdc_any_luts_active(int rev)
{
	bool any_active;

	if (rev < 20)
		any_active = __raw_readl(EPDC_STATUS_LUTS) ? true : false;
	else
		any_active = (__raw_readl(EPDC_STATUS_LUTS) |
			__raw_readl(EPDC_STATUS_LUTS2))	? true : false;

	return any_active;
}

static inline u64 epdc_luts_status(int rev)
{
	u64 luts_status;
	u32 format_p5n = ((__raw_readl(EPDC_FORMAT) & EPDC_FORMAT_BUF_PIXEL_FORMAT_MASK) ==
		EPDC_FORMAT_BUF_PIXEL_FORMAT_P5N);

	luts_status = __raw_readl(EPDC_STATUS_LUTS) ;
	if (rev < 20 || format_p5n)
		luts_status &= 0xFFFF;
	else
		luts_status |= ((u64)__raw_readl(EPDC_STATUS_LUTS2) << 32);

	return luts_status;
}

static inline bool epdc_any_luts_real_available(void)
{
	if ((__raw_readl(EPDC_STATUS_LUTS) != 0xfffffffe) ||
		(__raw_readl(EPDC_STATUS_LUTS2) != ~0UL))
		return true;
	else
		return false;
}

static inline bool epdc_any_luts_available(void)
{
#ifdef EPDC_STANDARD_MODE
	if (((u32)used_luts != ~0UL) || ((u32)(used_luts >> 32) != ~0UL))
		return 1;
	else
		return 0;
#else
	bool luts_available =
	    (__raw_readl(EPDC_STATUS_NEXTLUT) &
	     EPDC_STATUS_NEXTLUT_NEXT_LUT_VALID) ? true : false;
	return luts_available;
#endif
}

static inline int epdc_get_next_lut(void)
{
	u32 val =
	    __raw_readl(EPDC_STATUS_NEXTLUT) &
	    EPDC_STATUS_NEXTLUT_NEXT_LUT_MASK;
	return val;
}

static inline void epdc_set_used_lut(u64 used_bit)
{
	used_luts |= (u64)1 << used_bit;
}

static inline void epdc_reset_used_lut(void)
{
	used_luts = 0x1;
}

#ifdef EPDC_STANDARD_MODE
/*
 * in previous flow, when all LUTs are used, the LUT cleanup operation
 * need to wait for all the LUT to finish, it will not happen util last LUT
 * is done. while in new flow, the cleanup operation does not need to wait
 * for all LUTs to finish, instead it can start when there's LUT's done.
 * The saved time is multiple LUT operation time.
 */
static int epdc_choose_next_lut(struct mxc_epdc_fb_data *fb_data, int *next_lut)
{
	while (!epdc_any_luts_available()) {
		u64 luts_complete = fb_data->luts_complete;
		pxp_clear_wb_work_func(fb_data);
		used_luts &= ~luts_complete;
		fb_data->luts_complete &= ~luts_complete;
	}

	used_luts |= 0x1;

	if ((u32)used_luts != ~0UL)
		*next_lut = ffz((u32)used_luts);
	else if ((u32)(used_luts >> 32) != ~0UL)
		*next_lut = ffz((u32)(used_luts >> 32)) + 32;

	return 0;
}
#else
static int epdc_choose_next_lut(struct mxc_epdc_fb_data *fb_data, int *next_lut)
{
	u64 luts_status, unprocessed_luts, used_luts;
	/* Available LUTs are reduced to 16 in 5-bit waveform mode */
	bool format_p5n = ((__raw_readl(EPDC_FORMAT) &
	EPDC_FORMAT_BUF_PIXEL_FORMAT_MASK) ==
	EPDC_FORMAT_BUF_PIXEL_FORMAT_P5N);

	luts_status = __raw_readl(EPDC_STATUS_LUTS);
	if ((fb_data->rev < 20) || format_p5n)
		luts_status &= 0xFFFF;
	else
		luts_status |= ((u64)__raw_readl(EPDC_STATUS_LUTS2) << 32);

	if (fb_data->rev < 20) {
		unprocessed_luts = __raw_readl(EPDC_IRQ) & 0xFFFF;
	} else {
		unprocessed_luts = __raw_readl(EPDC_IRQ1) |
			((u64)__raw_readl(EPDC_IRQ2) << 32);
		if (format_p5n)
			unprocessed_luts &= 0xFFFF;
	}

	/*
	 * Note on unprocessed_luts: There is a race condition
	 * where a LUT completes, but has not been processed by
	 * IRQ handler workqueue, and then a new update request
	 * attempts to use that LUT.  We prevent that here by
	 * ensuring that the LUT we choose doesn't have its IRQ
	 * bit set (indicating it has completed but not yet been
	 * processed).
	 */
	used_luts = luts_status | unprocessed_luts;

	/*
	 * Selecting a LUT to minimize incidence of TCE Underrun Error
	 * --------------------------------------------------------
	 * We want to find the lowest order LUT that is of greater
	 * order than all other active LUTs.  If highest order LUT
	 * is active, then we want to choose the lowest order
	 * available LUT.
	 *
	 * NOTE: For EPDC version 2.0 and later, TCE Underrun error
	 *       bug is fixed, so it doesn't matter which LUT is used.
	 */

	if ((fb_data->rev < 20) || format_p5n) {
		*next_lut = fls64(used_luts);
		if (*next_lut > 15)
			*next_lut = ffz(used_luts);
	} else {
		if ((u32)used_luts != ~0UL)
			*next_lut = ffz((u32)used_luts);
		else if ((u32)(used_luts >> 32) != ~0UL)
			*next_lut = ffz((u32)(used_luts >> 32)) + 32;
		else
			*next_lut = INVALID_LUT;
	}

	if (used_luts & 0x8000)
		return 1;
	else
		return 0;
}
#endif

static inline bool epdc_is_working_buffer_busy(void)
{
	u32 val = __raw_readl(EPDC_STATUS);
	bool is_busy = (val & EPDC_STATUS_WB_BUSY) ? true : false;

	return is_busy;
}

static inline bool epdc_is_working_buffer_complete(void)
{
	u32 val = __raw_readl(EPDC_IRQ);
	bool is_compl = (val & EPDC_IRQ_WB_CMPLT_IRQ) ? true : false;

	return is_compl;
}

static inline bool epdc_is_lut_cancelled(void)
{
	u32 val = __raw_readl(EPDC_STATUS);
	bool is_void = (val & EPDC_STATUS_UPD_VOID) ? true : false;

	return is_void;
}

static inline bool epdc_is_collision(void)
{
	u32 val = __raw_readl(EPDC_IRQ);
	return (val & EPDC_IRQ_LUT_COL_IRQ) ? true : false;
}

static inline u64 epdc_get_colliding_luts(int rev)
{
	u32 val = __raw_readl(EPDC_STATUS_COL);
	if (rev >= 20)
		val |= (u64)__raw_readl(EPDC_STATUS_COL2) << 32;
	return val;
}

static void epdc_set_horizontal_timing(u32 horiz_start, u32 horiz_end,
				       u32 hsync_width, u32 hsync_line_length)
{
	u32 reg_val =
	    ((hsync_width << EPDC_TCE_HSCAN1_LINE_SYNC_WIDTH_OFFSET) &
	     EPDC_TCE_HSCAN1_LINE_SYNC_WIDTH_MASK)
	    | ((hsync_line_length << EPDC_TCE_HSCAN1_LINE_SYNC_OFFSET) &
	       EPDC_TCE_HSCAN1_LINE_SYNC_MASK);
	__raw_writel(reg_val, EPDC_TCE_HSCAN1);

	reg_val =
	    ((horiz_start << EPDC_TCE_HSCAN2_LINE_BEGIN_OFFSET) &
	     EPDC_TCE_HSCAN2_LINE_BEGIN_MASK)
	    | ((horiz_end << EPDC_TCE_HSCAN2_LINE_END_OFFSET) &
	       EPDC_TCE_HSCAN2_LINE_END_MASK);
	__raw_writel(reg_val, EPDC_TCE_HSCAN2);
}

static void epdc_set_vertical_timing(u32 vert_start, u32 vert_end,
				     u32 vsync_width)
{
	u32 reg_val =
	    ((vert_start << EPDC_TCE_VSCAN_FRAME_BEGIN_OFFSET) &
	     EPDC_TCE_VSCAN_FRAME_BEGIN_MASK)
	    | ((vert_end << EPDC_TCE_VSCAN_FRAME_END_OFFSET) &
	       EPDC_TCE_VSCAN_FRAME_END_MASK)
	    | ((vsync_width << EPDC_TCE_VSCAN_FRAME_SYNC_OFFSET) &
	       EPDC_TCE_VSCAN_FRAME_SYNC_MASK);
	__raw_writel(reg_val, EPDC_TCE_VSCAN);
}

#define EPDC_INIT_SETTING_PROC_TCE_RECOVERY 	1
static void epdc_init_settings_ex(struct mxc_epdc_fb_data *fb_data,int iProcType)
{
	struct imx_epdc_fb_mode *epdc_mode = fb_data->cur_mode;
	struct fb_var_screeninfo *screeninfo = &fb_data->epdc_fb_var;
	u32 reg_val;
	int num_ce;
#ifndef EPDC_STANDARD_MODE
	int i;
#endif
	int j;
	unsigned char *bb_p;

	int iBusType=-1;

#define EPDC_BUS_TYPE_16BITS					1
#define EPDC_BUS_TYPE_8BITS_MIRROR		2
#define EPDC_BUS_TYPE_16BITS_MIRROR		3

#if 1 
	if(gptHWCFG) {
		// NTX HWCFG .
		switch (gptHWCFG->m_val.bDisplayBusWidth) {
		case 1: // 16 bits .
			iBusType = EPDC_BUS_TYPE_16BITS;
			break;
		case 2: // 8 bits mirror 
			iBusType = EPDC_BUS_TYPE_8BITS_MIRROR;
			break;
		case 3:
			iBusType = EPDC_BUS_TYPE_16BITS_MIRROR;
			break;
		default:
		case 0:
			iBusType = 0;
			break;
		}
	}
#else
	if(0==strcmp("ED078KH1",epdc_mode->vmode->name)) {
		iBusType=EPDC_BUS_TYPE_16BITS;// test ED078KH1 .
	}
#endif

	printk("%s,%s():epdc_mode=\"%s\"\n",__FILE__,__FUNCTION__,
			epdc_mode->vmode->name);
	/* Enable clocks to access EPDC regs */
	clk_prepare_enable(fb_data->epdc_clk_axi);
	clk_prepare_enable(fb_data->epdc_clk_pix);

	/* Reset */
	__raw_writel(EPDC_CTRL_SFTRST, EPDC_CTRL_SET);
	while (!(__raw_readl(EPDC_CTRL) & EPDC_CTRL_CLKGATE))
		;
	__raw_writel(EPDC_CTRL_SFTRST, EPDC_CTRL_CLEAR);

	/* Enable clock gating (clear to enable) */
	__raw_writel(EPDC_CTRL_CLKGATE, EPDC_CTRL_CLEAR);
	while (__raw_readl(EPDC_CTRL) & (EPDC_CTRL_SFTRST | EPDC_CTRL_CLKGATE))
		;

	/* EPDC_CTRL */
	reg_val = __raw_readl(EPDC_CTRL);
	reg_val &= ~EPDC_CTRL_UPD_DATA_SWIZZLE_MASK;
#ifdef	EPDC_STANDARD_MODE
	reg_val |= EPDC_CTRL_UPD_DATA_SWIZZLE_ALL_BYTES_SWAP;
#else
	reg_val |= EPDC_CTRL_UPD_DATA_SWIZZLE_NO_SWAP;
#endif
	reg_val &= ~EPDC_CTRL_LUT_DATA_SWIZZLE_MASK;
	reg_val |= EPDC_CTRL_LUT_DATA_SWIZZLE_NO_SWAP;
	__raw_writel(reg_val, EPDC_CTRL_SET);

	/* EPDC_FORMAT - 2bit TFT and 4bit Buf pixel format */
	reg_val = EPDC_FORMAT_TFT_PIXEL_FORMAT_2BIT
#ifdef	EPDC_STANDARD_MODE
	    | EPDC_FORMAT_WB_TYPE_WB_EXTERNAL16
#endif
	    | EPDC_FORMAT_BUF_PIXEL_FORMAT_P4N
	    | ((0x0 << EPDC_FORMAT_DEFAULT_TFT_PIXEL_OFFSET) &
	       EPDC_FORMAT_DEFAULT_TFT_PIXEL_MASK);
	__raw_writel(reg_val, EPDC_FORMAT);

#ifdef	EPDC_STANDARD_MODE
	reg_val = 0;
	if (fb_data->waveform_is_advanced) {
		reg_val =
		    ((EPDC_WB_FIELD_USAGE_PTS << EPDC_WB_FIELD_USAGE_OFFSET) &
		      EPDC_WB_FIELD_USAGE_MASK)
		    | ((0x8 << EPDC_WB_FIELD_FROM_OFFSET) &
		      EPDC_WB_FIELD_FROM_MASK)
		    | ((0x8 << EPDC_WB_FIELD_TO_OFFSET) &
		      EPDC_WB_FIELD_TO_MASK)
		    | ((0x1 << EPDC_WB_FIELD_LEN_OFFSET) &
		      EPDC_WB_FIELD_LEN_MASK);
	}
	__raw_writel(reg_val, EPDC_WB_FIELD3);
#endif

	/* EPDC_FIFOCTRL */
	reg_val = EPDC_FIFOCTRL_ENABLE_PRIORITY |
	    ((0 << EPDC_FIFOCTRL_FIFO_INIT_LEVEL_OFFSET) &
	     EPDC_FIFOCTRL_FIFO_INIT_LEVEL_MASK)
	    | ((255 << EPDC_FIFOCTRL_FIFO_H_LEVEL_OFFSET) &
	       EPDC_FIFOCTRL_FIFO_H_LEVEL_MASK)
	    | ((254 << EPDC_FIFOCTRL_FIFO_L_LEVEL_OFFSET) &
	       EPDC_FIFOCTRL_FIFO_L_LEVEL_MASK);
	__raw_writel(reg_val, EPDC_FIFOCTRL);

	/* EPDC_TEMP - Use default temp to get index */
	epdc_set_temp(mxc_epdc_fb_get_temp_index(fb_data, DEFAULT_TEMP));

	/* EPDC_RES */
	epdc_set_screen_res(epdc_mode->vmode->xres, epdc_mode->vmode->yres);

#ifndef EPDC_STANDARD_MODE
	/* EPDC_AUTOWV_LUT */
	/* Initialize all auto-wavefrom look-up values to 2 - GC16 */
	for (i = 0; i < 8; i++)
		__raw_writel((2 << EPDC_AUTOWV_LUT_DATA_OFFSET) |
			(i << EPDC_AUTOWV_LUT_ADDR_OFFSET), EPDC_AUTOWV_LUT);
#endif

	switch (iBusType) {
	case EPDC_BUS_TYPE_16BITS:
		// 16 bits .
		//
		
		/* 
		* EPDC_TCE_CTRL
		* VSCAN_HOLDOFF = 4
		* VCOM_MODE = MANUAL
		* VCOM_VAL = 0
		* DDR_MODE = DISABLED
		* LVDS_MODE_CE = DISABLED
		* LVDS_MODE = DISABLED
		* DUAL_SCAN = DISABLED
		* SDDO_WIDTH = 16bit
		* PIXELS_PER_SDCLK = 8
		*/

		dev_info(fb_data->dev,"16bits display bus width \n");
		reg_val =
			((epdc_mode->vscan_holdoff << EPDC_TCE_CTRL_VSCAN_HOLDOFF_OFFSET) &
			EPDC_TCE_CTRL_VSCAN_HOLDOFF_MASK)
			| EPDC_TCE_CTRL_PIXELS_PER_SDCLK_8
			| EPDC_TCE_CTRL_SDDO_WIDTH_16BIT 
			;

		break;
	case EPDC_BUS_TYPE_8BITS_MIRROR:
		// 8 bits ,mirror .
		//
		
		dev_info(fb_data->dev,"8bits mirror display bus width \n");

		reg_val =
			((epdc_mode->vscan_holdoff << EPDC_TCE_CTRL_VSCAN_HOLDOFF_OFFSET) &
			EPDC_TCE_CTRL_VSCAN_HOLDOFF_MASK)
			| EPDC_TCE_CTRL_SCAN_DIR_0_UP
			| EPDC_TCE_CTRL_SCAN_DIR_1_UP
			| EPDC_TCE_CTRL_PIXELS_PER_SDCLK_4
			//| EPDC_TCE_CTRL_SDDO_WIDTH_8BIT 
			;
		break;
	case EPDC_BUS_TYPE_16BITS_MIRROR:
		// 16 bits ,mirror .
		//
		
		dev_info(fb_data->dev,"16bits mirror display bus width \n");

		reg_val =
			((epdc_mode->vscan_holdoff << EPDC_TCE_CTRL_VSCAN_HOLDOFF_OFFSET) &
			EPDC_TCE_CTRL_VSCAN_HOLDOFF_MASK)
			| EPDC_TCE_CTRL_SCAN_DIR_0_UP
		//	| EPDC_TCE_CTRL_SCAN_DIR_1_UP //@Sam 20140114 Marked for E60QC2
			| EPDC_TCE_CTRL_PIXELS_PER_SDCLK_8
			| EPDC_TCE_CTRL_SDDO_WIDTH_16BIT 
			;
		break;

	default:
		/*
		 * EPDC_TCE_CTRL
		 * VSCAN_HOLDOFF = 4
		 * VCOM_MODE = MANUAL
		 * VCOM_VAL = 0
		 * DDR_MODE = DISABLED
		 * LVDS_MODE_CE = DISABLED
		 * LVDS_MODE = DISABLED
		 * DUAL_SCAN = DISABLED
		 * SDDO_WIDTH = 8bit
		 * PIXELS_PER_SDCLK = 4
		 */
		reg_val =
				((epdc_mode->vscan_holdoff << EPDC_TCE_CTRL_VSCAN_HOLDOFF_OFFSET) &
				 EPDC_TCE_CTRL_VSCAN_HOLDOFF_MASK)
				| EPDC_TCE_CTRL_PIXELS_PER_SDCLK_4;
		break;
	}
	__raw_writel(reg_val, EPDC_TCE_CTRL);

	/* EPDC_TCE_HSCAN */
	epdc_set_horizontal_timing(screeninfo->left_margin,
				   screeninfo->right_margin,
				   screeninfo->hsync_len,
				   screeninfo->hsync_len);

	/* EPDC_TCE_VSCAN */
	epdc_set_vertical_timing(screeninfo->upper_margin,
				 screeninfo->lower_margin,
				 screeninfo->vsync_len);

	/* EPDC_TCE_OE */
	reg_val =
	    ((epdc_mode->sdoed_width << EPDC_TCE_OE_SDOED_WIDTH_OFFSET) &
	     EPDC_TCE_OE_SDOED_WIDTH_MASK)
	    | ((epdc_mode->sdoed_delay << EPDC_TCE_OE_SDOED_DLY_OFFSET) &
	       EPDC_TCE_OE_SDOED_DLY_MASK)
	    | ((epdc_mode->sdoez_width << EPDC_TCE_OE_SDOEZ_WIDTH_OFFSET) &
	       EPDC_TCE_OE_SDOEZ_WIDTH_MASK)
	    | ((epdc_mode->sdoez_delay << EPDC_TCE_OE_SDOEZ_DLY_OFFSET) &
	       EPDC_TCE_OE_SDOEZ_DLY_MASK);
	__raw_writel(reg_val, EPDC_TCE_OE);

	/* EPDC_TCE_TIMING1 */
	__raw_writel(0x0, EPDC_TCE_TIMING1);

	/* EPDC_TCE_TIMING2 */
	reg_val =
	    ((epdc_mode->gdclk_hp_offs << EPDC_TCE_TIMING2_GDCLK_HP_OFFSET) &
	     EPDC_TCE_TIMING2_GDCLK_HP_MASK)
	    | ((epdc_mode->gdsp_offs << EPDC_TCE_TIMING2_GDSP_OFFSET_OFFSET) &
	       EPDC_TCE_TIMING2_GDSP_OFFSET_MASK);
	__raw_writel(reg_val, EPDC_TCE_TIMING2);

	/* EPDC_TCE_TIMING3 */
	reg_val =
	    ((epdc_mode->gdoe_offs << EPDC_TCE_TIMING3_GDOE_OFFSET_OFFSET) &
	     EPDC_TCE_TIMING3_GDOE_OFFSET_MASK)
	    | ((epdc_mode->gdclk_offs << EPDC_TCE_TIMING3_GDCLK_OFFSET_OFFSET) &
	       EPDC_TCE_TIMING3_GDCLK_OFFSET_MASK);
	__raw_writel(reg_val, EPDC_TCE_TIMING3);

	/*
	 * EPDC_TCE_SDCFG
	 * SDCLK_HOLD = 1
	 * SDSHR = 1
	 * NUM_CE = 1
	 * SDDO_REFORMAT = FLIP_PIXELS
	 * SDDO_INVERT = DISABLED
	 * PIXELS_PER_CE = display horizontal resolution
	 */
	num_ce = epdc_mode->num_ce;
	if (num_ce == 0)
		num_ce = 1;
	reg_val = EPDC_TCE_SDCFG_SDCLK_HOLD | EPDC_TCE_SDCFG_SDSHR
	    | ((num_ce << EPDC_TCE_SDCFG_NUM_CE_OFFSET) &
	       EPDC_TCE_SDCFG_NUM_CE_MASK)
	    | EPDC_TCE_SDCFG_SDDO_REFORMAT_FLIP_PIXELS
	    | ((epdc_mode->vmode->xres/num_ce << EPDC_TCE_SDCFG_PIXELS_PER_CE_OFFSET) &
	       EPDC_TCE_SDCFG_PIXELS_PER_CE_MASK);
	__raw_writel(reg_val, EPDC_TCE_SDCFG);

	/*
	 * EPDC_TCE_GDCFG
	 * GDRL = 1
	 * GDOE_MODE = 0;
	 * GDSP_MODE = 0;
	 */
	reg_val = EPDC_TCE_SDCFG_GDRL;
	__raw_writel(reg_val, EPDC_TCE_GDCFG);

	/*
	 * EPDC_TCE_POLARITY
	 * SDCE_POL = ACTIVE LOW
	 * SDLE_POL = ACTIVE HIGH
	 * SDOE_POL = ACTIVE HIGH
	 * GDOE_POL = ACTIVE HIGH
	 * GDSP_POL = ACTIVE LOW
	 */
	reg_val = EPDC_TCE_POLARITY_SDLE_POL_ACTIVE_HIGH
	    | EPDC_TCE_POLARITY_SDOE_POL_ACTIVE_HIGH
	    | EPDC_TCE_POLARITY_GDOE_POL_ACTIVE_HIGH;
	__raw_writel(reg_val, EPDC_TCE_POLARITY);

	if(0==iProcType) {
		/* EPDC_IRQ_MASK */
		__raw_writel(EPDC_IRQ_TCE_UNDERRUN_IRQ, EPDC_IRQ_MASK);
	}

	/*
	 * EPDC_GPIO
	 * PWRCOM = ?
	 * PWRCTRL = ?
	 * BDR = ?
	 */
	reg_val = ((0 << EPDC_GPIO_PWRCTRL_OFFSET) & EPDC_GPIO_PWRCTRL_MASK)
	    | ((0 << EPDC_GPIO_BDR_OFFSET) & EPDC_GPIO_BDR_MASK);
	__raw_writel(reg_val, EPDC_GPIO);

	__raw_writel(fb_data->waveform_buffer_phys, EPDC_WVADDR);
	__raw_writel(fb_data->working_buffer_phys, EPDC_WB_ADDR);
	__raw_writel(fb_data->working_buffer_phys, EPDC_WB_ADDR_TCE);

	bb_p = (unsigned char *)fb_data->virt_addr_black;
	for (j = 0; j < fb_data->cur_mode->vmode->xres * fb_data->cur_mode->vmode->yres; j++) {
		*bb_p = 0x0;
		bb_p++;
	}

	/* clear LUT status */
	__raw_writel(0xFFFFFFFF, EPDC_STATUS_LUTS_CLEAR);
	__raw_writel(0xFFFFFFFF, EPDC_STATUS_LUTS2_CLEAR);

	if(0==iProcType) {
		/* Disable clock */
		clk_disable_unprepare(fb_data->epdc_clk_axi);
		clk_disable_unprepare(fb_data->epdc_clk_pix);
	}
}
static void epdc_init_settings(struct mxc_epdc_fb_data *fb_data)
{
	epdc_init_settings_ex(fb_data,0);
}

#ifdef NIGHT_MODE_XON_TIMING//[
static enum hrtimer_restart _hrtint_xon_on_ctrl(struct hrtimer *timer)
{
	struct mxc_epdc_fb_data *fb_data = 
		container_of(timer, struct mxc_epdc_fb_data, hrt_xon_on_ctrl);

	//EPDC_VPRINT(fb_data,0, "XON pulling high...\n");
	gpiod_set_value(fb_data->gpio_xon_desc,1);
	EPDC_VPRINT(fb_data,3, "XON pulled high\n");

	return HRTIMER_NORESTART;
}
static enum hrtimer_restart _hrtint_xon_off_ctrl(struct hrtimer *timer)
{
	struct mxc_epdc_fb_data *fb_data = 
		container_of(timer, struct mxc_epdc_fb_data, hrt_xon_off_ctrl);

	gpiod_set_value(fb_data->gpio_xon_desc,0);
	EPDC_VPRINT(fb_data,3, "XON pulled down\n");

	return HRTIMER_NORESTART;
}

#endif //] NIGHT_MODE_XON_TIMING

static void epdc_powerup(struct mxc_epdc_fb_data *fb_data)
{
	struct epd_vc_data vcd;
	int ret = 0;

	mutex_lock(&fb_data->power_mutex);

	

	/*
	 * If power down request is pending, clear
	 * powering_down to cancel the request.
	 */
	if (fb_data->powering_down) {
		cancel_delayed_work_sync(&fb_data->epdc_done_work);
		fb_data->powering_down = false;
	}

	if (fb_data->power_state == POWER_STATE_ON) {
		EPDC_VPRINT(fb_data,5, "EPD skipped pwrup, state=%d (ON)\n",fb_data->power_state);
		mutex_unlock(&fb_data->power_mutex);
		return;
	}

	EPDC_VPRINT(fb_data,3,"EPDC Powerup\n");
	NTX_TimeStamp_In("epdc_pwrup",0);

	fb_data->updates_active = true;

	/* Enable the v3p3 regulator */
	ret = regulator_enable(fb_data->v3p3_regulator);
	if (IS_ERR((void *)ret)) {
		dev_err(fb_data->dev, "Unable to enable V3P3 regulator."
			"err = 0x%x\n", ret);
		mutex_unlock(&fb_data->power_mutex);
		return;
	}
	if(!regulator_is_enabled(fb_data->v3p3_regulator)) {
		fb_data->v3p3_fixed = 1;
	}
#ifdef NIGHT_MODE_XON_TIMING//[
	if(-2!=fb_data->night_mode_test) {
		if (fb_data->gpio_xon_desc) {
			hrtimer_cancel(&fb_data->hrt_xon_off_ctrl);
			hrtimer_cancel(&fb_data->hrt_xon_on_ctrl);
			if(0==fb_data->night_mode_test) {
				gpiod_set_value(fb_data->gpio_xon_desc,0);
			}
			else 
			if(-1==fb_data->night_mode_test) {
				gpiod_set_value(fb_data->gpio_xon_desc,1);
			}
			else {
				gpiod_set_value(fb_data->gpio_xon_desc,1);
			}
		}
	}
#endif //] NIGHT_MODE_XON_TIMING

	msleep(1);

	pm_runtime_get_sync(fb_data->dev);


	/* Enable clocks to EPDC */
	clk_prepare_enable(fb_data->epdc_clk_axi);
	clk_prepare_enable(fb_data->epdc_clk_pix);

	__raw_writel(EPDC_CTRL_CLKGATE, EPDC_CTRL_CLEAR);

	if (fb_data->wfm < 256 && fb_data->waveform_vcd_buffer) {
		/* fetch and display the voltage control data for waveform mode 0, temp range 0 */
		fetch_Epdc_Pmic_Voltages(&vcd, fb_data, fb_data->wfm, fb_data->temp_index);


	}
	else
		vcd.v5 = 0;


	if (fb_data->wfm < 256) {
		int vcom_uV, new_vcom_uV;
		static volatile int last_vcom_uV = 0;
		int v5_sign = 1;
		int v5_offset = vcd.v5 & 0x7fff;
		
		/* get vcom offset value */
		if (vcd.v5 & 0x8000) {
			v5_sign = -1;
		}
		if( 0 == last_vcom_uV ) {
			last_vcom_uV = vcom_uV = regulator_get_voltage(fb_data->vcom_regulator);
		}
		else {
			vcom_uV = last_vcom_uV;
		}
		dev_dbg(fb_data->dev,"current VCOM %duV\n",vcom_uV);

		fb_data->iVCOM_offset_uV = v5_offset * 3125 * v5_sign;
		new_vcom_uV = vcom_nominal + fb_data->iVCOM_offset_uV;
		if (new_vcom_uV!=vcom_uV) 
		{
			dev_info(fb_data->dev,"AWV change VCOM %d->%d uV\n",vcom_uV,new_vcom_uV);
			if ( (new_vcom_uV >= -3050000) && (new_vcom_uV<=-500000) ) 
			{
				regulator_set_voltage(fb_data->vcom_regulator, new_vcom_uV, new_vcom_uV);
				last_vcom_uV = new_vcom_uV;
			}
			else {
				printk(KERN_ERR" adjusted VCOM is out of range, %d uV (offset:0x%04x)\n", new_vcom_uV, vcd.v5);
			}
		}
	}

	
	#if 0 //[ 
	
	EPDC_VPRINT(fb_data,3,"nightmode wf mode=%d\n",waveform_mode);
	if( ( (waveform_mode==fb_data->wv_modes.mode_glkw16 && \
				(fb_data->wv_modes.mode_gl16!=fb_data->wv_modes.mode_glkw16)) ||\
			(waveform_mode==fb_data->wv_modes.mode_gck16 &&\
				(fb_data->wv_modes.mode_gc16!=fb_data->wv_modes.mode_gck16)) ) ) 
	{
		ret = regulator_setflags(fb_data->display_regulator,EPD_PMIC_FLAGS_NIGHTMODE);
	}
	else 
	{
		ret = regulator_setflags(fb_data->display_regulator,0);
	}
	#endif //]

	/* Enable power to the EPD panel */
	dev_dbg(fb_data->dev," *** enabling the EPDC PMIC (%d,%d) ***\n",fb_data->wfm, fb_data->temp_index);
	ret = regulator_enable(fb_data->display_regulator);
	if (IS_ERR((void *)ret)) {
		dev_err(fb_data->dev, "Unable to enable DISPLAY regulator."
			"err = 0x%x\n", ret);
		mutex_unlock(&fb_data->power_mutex);
		return;
	}


	ret = regulator_enable(fb_data->vcom_regulator);
	if (IS_ERR((void *)ret)) {
		dev_err(fb_data->dev, "Unable to enable VCOM regulator."
			"err = 0x%x\n", ret);
		mutex_unlock(&fb_data->power_mutex);
		return;
	}

	k_set_temperature(&fb_data->info);

	fb_data->power_state = POWER_STATE_ON;


	udelay(1000); // for EPD power on sequence 

	mutex_unlock(&fb_data->power_mutex);
}

static void epdc_powerdown(struct mxc_epdc_fb_data *fb_data)
{
	mutex_lock(&fb_data->power_mutex);


	/* If powering_down has been cleared, a powerup
	 * request is pre-empting this powerdown request.
	 */
	if (!fb_data->powering_down
		|| (fb_data->power_state == POWER_STATE_OFF)) {
		mutex_unlock(&fb_data->power_mutex);
		EPDC_VPRINT(fb_data,5, "EPDC skipped pwrdwn,state=%d,powering_down=%d \n",
			fb_data->power_state,fb_data->powering_down);
		return;
	}

	NTX_TimeStamp_In("epdc_pwrdown",0);
	EPDC_VPRINT(fb_data,3, "EPDC Powerdown\n");

	/* Disable power to the EPD panel */
	if(!fb_data->vcom_off_with_data) {
		regulator_disable(fb_data->vcom_regulator);
	}

#ifdef NIGHT_MODE_XON_TIMING//[
	if(-2!=fb_data->night_mode_test) {
		if (fb_data->gpio_xon_desc) {

			if( ( (giLast_waveform_mode==g_fb_data->wv_modes.mode_glkw16 && \
				(g_fb_data->wv_modes.mode_gl16!=g_fb_data->wv_modes.mode_glkw16)) ||\
				(giLast_waveform_mode==g_fb_data->wv_modes.mode_gck16 &&\
				(g_fb_data->wv_modes.mode_gc16!=g_fb_data->wv_modes.mode_gck16)) ) ) 
			{
				//int xon_on_us=350000;
				int xon_on_us=fb_data->xon_on_delay_us;
				int xon_off_us=fb_data->xon_off_delay_us;

				hrtimer_cancel(&fb_data->hrt_xon_off_ctrl);
				EPDC_VPRINT(fb_data,2, "XON off after %d us\n",xon_off_us);
				//fb_data->hrt_xon_off_ctrl.function = _hrtint_xon_off_ctrl;
				hrtimer_start(&fb_data->hrt_xon_off_ctrl,
					ktime_set(xon_off_us/999999,(xon_off_us%1000000) * 1000),
					HRTIMER_MODE_REL);

				hrtimer_cancel(&fb_data->hrt_xon_on_ctrl);
				EPDC_VPRINT(fb_data,2, "XON on after %d us\n",xon_on_us);
				//fb_data->hrt_xon_on_ctrl.function = _hrtint_xon_on_ctrl;
				hrtimer_start(&fb_data->hrt_xon_on_ctrl,
					ktime_set(xon_on_us/999999,(xon_on_us%1000000) * 1000),
					HRTIMER_MODE_REL);
			}
			else 
			{
				int xon_off_us=fb_data->xon_off_day_delay_us;

				hrtimer_cancel(&fb_data->hrt_xon_off_ctrl);
				EPDC_VPRINT(fb_data,2, "XON off after %d us\n",xon_off_us);
				//fb_data->hrt_xon_off_ctrl.function = _hrtint_xon_off_ctrl;
				hrtimer_start(&fb_data->hrt_xon_off_ctrl,
					ktime_set(xon_off_us/999999,(xon_off_us%1000000) * 1000),
					HRTIMER_MODE_REL);
			}
			
		}
	}
#endif //] NIGHT_MODE_XON_TIMING
	regulator_disable(fb_data->display_regulator);

	/* Disable clocks to EPDC */
	__raw_writel(EPDC_CTRL_CLKGATE, EPDC_CTRL_SET);
	clk_disable_unprepare(fb_data->epdc_clk_pix);
	clk_disable_unprepare(fb_data->epdc_clk_axi);

	pm_runtime_put_sync_suspend(fb_data->dev);

	/* turn off the V3p3 */
	regulator_disable(fb_data->v3p3_regulator);
	if(regulator_is_enabled(fb_data->v3p3_regulator)) {
		fb_data->v3p3_fixed = 1;
	}

	fb_data->power_state = POWER_STATE_OFF;
	fb_data->powering_down = false;

	if (fb_data->wait_for_powerdown) {
		fb_data->wait_for_powerdown = false;
		complete(&fb_data->powerdown_compl);
	}
	fb_data->dwJiffies_To_TurnOFF_EP3V3 = jiffies + fb_data->dwSafeTicksEP3V3;
	mutex_unlock(&fb_data->power_mutex);
}


#ifdef TCE_UNDERRUN_PREVENT_WORKFUNC//[
static void tce_safe_work_func(struct work_struct *work)
{
	struct mxc_epdc_fb_data *fb_data =
		container_of(work, struct mxc_epdc_fb_data,
			tce_safe_work.work);
	int i;
	unsigned long flags;

	fb_data->tce_safe_work_running = 1;
	EPDC_VPRINT(fb_data,0,"%s() start\n",__FUNCTION__);
	for(i=0;i<fb_data->tce_safe_loops;i++)
	{
		if(fb_data->tce_safe_work_cancel) {
			fb_data->tce_safe_work_cancel = 0;
			EPDC_VPRINT(fb_data,0,"%s() cancel\n",__FUNCTION__);
			break;
		}
		EPDC_VPRINT(fb_data,0,"*",__FUNCTION__);
		//spin_lock_irqsave(&fb_data->tce_safe_lock,flags);
		spin_lock(&fb_data->tce_safe_lock);
		mdelay(fb_data->tce_safe_ms);
		spin_unlock(&fb_data->tce_safe_lock);
		//spin_unlock_irqrestore(&fb_data->tce_safe_lock,flags);
		msleep(fb_data->tce_safe_freems);
	}
	EPDC_VPRINT(fb_data,0,"%s() end\n",__FUNCTION__);

	fb_data->tce_safe_work_running = 0;
	return ;
}
#endif //]TCE_UNDERRUN_PREVENT_WORKFUNC

static void epdc_init_sequence(struct mxc_epdc_fb_data *fb_data)
{
	/* Initialize EPDC, passing pointer to EPDC registers */
	epdc_init_settings(fb_data);

	fb_data->in_init = true;
	epdc_powerup(fb_data);
	draw_mode0(fb_data);
	/* Force power down event */
	fb_data->powering_down = true;
	if(fb_data->vcom_off_with_data) {
		udelay(VCOM_OFF_DELAY_US);
		regulator_disable(fb_data->vcom_regulator);
	}
	epdc_powerdown(fb_data);
	fb_data->updates_active = false;
}

static int mxc_epdc_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	u32 len;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	u32 mem_start, mem_len ;

	fake_s1d13522_progress_stop();

	if(0==gptHWCFG->m_val.bUIStyle) {
		mem_start = info->fix.smem_start+info->fix.smem_len;
		mem_len = info->fix.smem_len;
	}
	else {
		mem_start = info->fix.smem_start;
		mem_len = info->fix.smem_len;
	}

	if(offset<mem_len)
	{
		/* mapping framebuffer memory */
		len = mem_len - offset;
		vma->vm_pgoff = (mem_start + offset) >> PAGE_SHIFT;
	} else
		return -EINVAL;

	len = PAGE_ALIGN(len);
	if (vma->vm_end - vma->vm_start > len)
		return -EINVAL;

	/* make buffers bufferable */
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		dev_dbg(info->device, "mmap remap_pfn_range failed\n");
		return -ENOBUFS;
	}

	return 0;
}

static inline u_int _chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int mxc_epdc_fb_setcolreg(u_int regno, u_int red, u_int green,
				 u_int blue, u_int transp, struct fb_info *info)
{
	unsigned int val;
	int ret = 1;

	/*
	 * If greyscale is true, then we convert the RGB value
	 * to greyscale no matter what visual we are using.
	 */
	if (info->var.grayscale)
		red = green = blue = (19595 * red + 38470 * green +
				      7471 * blue) >> 16;
	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/*
		 * 16-bit True Colour.  We encode the RGB value
		 * according to the RGB bitfield information.
		 */
		if (regno < 16) {
			u32 *pal = info->pseudo_palette;

			val = _chan_to_field(red, &info->var.red);
			val |= _chan_to_field(green, &info->var.green);
			val |= _chan_to_field(blue, &info->var.blue);

			pal[regno] = val;
			ret = 0;
		}
		break;

	case FB_VISUAL_STATIC_PSEUDOCOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		break;
	}

	return ret;
}

static int mxc_epdc_fb_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	int count, index, r;
	u16 *red, *green, *blue, *transp;
	u16 trans = 0xffff;
	struct mxc_epdc_fb_data *fb_data = (struct mxc_epdc_fb_data *)info;
	int i;

	dev_dbg(fb_data->dev, "setcmap\n");

	if (info->fix.visual == FB_VISUAL_STATIC_PSEUDOCOLOR) {
		/* Only support an 8-bit, 256 entry lookup */
		if (cmap->len != 256)
			return 1;

		mxc_epdc_fb_flush_updates(fb_data);

		mutex_lock(&fb_data->pxp_mutex);
		/*
		 * Store colormap in pxp_conf structure for later transmit
		 * to PxP during update process to convert gray pixels.
		 *
		 * Since red=blue=green for pseudocolor visuals, we can
		 * just use red values.
		 */
		for (i = 0; i < 256; i++)
			fb_data->pxp_conf.proc_data.lut_map[i] = cmap->red[i] & 0xFF;

		fb_data->pxp_conf.proc_data.lut_map_updated = true;

		mutex_unlock(&fb_data->pxp_mutex);
	} else {
		red     = cmap->red;
		green   = cmap->green;
		blue    = cmap->blue;
		transp  = cmap->transp;
		index   = cmap->start;

		for (count = 0; count < cmap->len; count++) {
			if (transp)
				trans = *transp++;
			r = mxc_epdc_fb_setcolreg(index++, *red++, *green++, *blue++,
						trans, info);
			if (r != 0)
				return r;
		}
	}

	return 0;
}

static void adjust_coordinates(u32 xres, u32 yres, u32 rotation,
	struct mxcfb_rect *update_region, struct mxcfb_rect *adj_update_region)
{
	u32 temp;

	/* If adj_update_region == NULL, pass result back in update_region */
	/* If adj_update_region == valid, use it to pass back result */
	if (adj_update_region)
		switch (rotation) {
		case FB_ROTATE_UR:
			adj_update_region->top = update_region->top;
			adj_update_region->left = update_region->left;
			adj_update_region->width = update_region->width;
			adj_update_region->height = update_region->height;
			break;
		case FB_ROTATE_CW:
			adj_update_region->top = update_region->left;
			adj_update_region->left = yres -
				(update_region->top + update_region->height);
			adj_update_region->width = update_region->height;
			adj_update_region->height = update_region->width;
			break;
		case FB_ROTATE_UD:
			adj_update_region->width = update_region->width;
			adj_update_region->height = update_region->height;
			adj_update_region->top = yres -
				(update_region->top + update_region->height);
			adj_update_region->left = xres -
				(update_region->left + update_region->width);
			break;
		case FB_ROTATE_CCW:
			adj_update_region->left = update_region->top;
			adj_update_region->top = xres -
				(update_region->left + update_region->width);
			adj_update_region->width = update_region->height;
			adj_update_region->height = update_region->width;
			break;
		}
	else
		switch (rotation) {
		case FB_ROTATE_UR:
			/* No adjustment needed */
			break;
		case FB_ROTATE_CW:
			temp = update_region->top;
			update_region->top = update_region->left;
			update_region->left = yres -
				(temp + update_region->height);
			temp = update_region->width;
			update_region->width = update_region->height;
			update_region->height = temp;
			break;
		case FB_ROTATE_UD:
			update_region->top = yres -
				(update_region->top + update_region->height);
			update_region->left = xres -
				(update_region->left + update_region->width);
			break;
		case FB_ROTATE_CCW:
			temp = update_region->left;
			update_region->left = update_region->top;
			update_region->top = xres -
				(temp + update_region->width);
			temp = update_region->width;
			update_region->width = update_region->height;
			update_region->height = temp;
			break;
		}
}

/*
 * Set fixed framebuffer parameters based on variable settings.
 *
 * @param       info     framebuffer information pointer
 */
static int mxc_epdc_fb_set_fix(struct fb_info *info)
{
	struct fb_fix_screeninfo *fix = &info->fix;
	struct fb_var_screeninfo *var = &info->var;

	fix->line_length = var->xres_virtual * var->bits_per_pixel / 8;

	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->accel = FB_ACCEL_NONE;
	if (var->grayscale)
		fix->visual = FB_VISUAL_STATIC_PSEUDOCOLOR;
	else
		fix->visual = FB_VISUAL_TRUECOLOR;
	fix->xpanstep = 1;
	fix->ypanstep = 1;

	return 0;
}

/*
 * This routine actually sets the video mode. It's in here where we
 * the hardware state info->par and fix which can be affected by the
 * change in par. For this driver it doesn't do much.
 *
 */
static int mxc_epdc_fb_set_par(struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = (struct mxc_epdc_fb_data *)info;
	struct pxp_config_data *pxp_conf = &fb_data->pxp_conf;
	struct pxp_proc_data *proc_data = &pxp_conf->proc_data;
	struct fb_var_screeninfo *screeninfo = &fb_data->info.var;
	struct imx_epdc_fb_mode *epdc_modes = fb_data->pdata->epdc_mode;
	int i;
	int ret;
	__u32 xoffset_old, yoffset_old;

	/*
	 * Can't change the FB parameters until current updates have completed.
	 * This function returns when all active updates are done.
	 */
	mxc_epdc_fb_flush_updates(fb_data);

	mutex_lock(&fb_data->queue_mutex);
	/*
	 * Set all screeninfo except for xoffset/yoffset
	 * Subsequent call to pan_display will handle those.
	 */
	xoffset_old = fb_data->epdc_fb_var.xoffset;
	yoffset_old = fb_data->epdc_fb_var.yoffset;
	fb_data->epdc_fb_var = *screeninfo;
	fb_data->epdc_fb_var.xoffset = xoffset_old;
	fb_data->epdc_fb_var.yoffset = yoffset_old;
	mutex_unlock(&fb_data->queue_mutex);

	mutex_lock(&fb_data->pxp_mutex);

	/*
	 * Update PxP config data (used to process FB regions for updates)
	 * based on FB info and processing tasks required
	 */

	/* Initialize non-channel-specific PxP parameters */
	proc_data->drect.left = proc_data->srect.left = 0;
	proc_data->drect.top = proc_data->srect.top = 0;
	proc_data->drect.width = proc_data->srect.width = screeninfo->xres;
	proc_data->drect.height = proc_data->srect.height = screeninfo->yres;
	proc_data->scaling = 0;
	proc_data->hflip = 0;
	proc_data->vflip = 0;
	proc_data->rotate = screeninfo->rotate;
	proc_data->bgcolor = 0;
	proc_data->overlay_state = 0;
	proc_data->lut_transform = PXP_LUT_NONE;

	/*
	 * configure S0 channel parameters
	 * Parameters should match FB format/width/height
	 */
	if (screeninfo->grayscale)
		pxp_conf->s0_param.pixel_fmt = PXP_PIX_FMT_GREY;
	else {
		switch (screeninfo->bits_per_pixel) {
		case 16:
			pxp_conf->s0_param.pixel_fmt = PXP_PIX_FMT_RGB565;
			break;
		case 24:
			pxp_conf->s0_param.pixel_fmt = PXP_PIX_FMT_RGB24;
			break;
		case 32:
			pxp_conf->s0_param.pixel_fmt = PXP_PIX_FMT_RGB32;
			break;
		default:
			pxp_conf->s0_param.pixel_fmt = PXP_PIX_FMT_RGB565;
			break;
		}
	}
	pxp_conf->s0_param.width = screeninfo->xres_virtual;
	pxp_conf->s0_param.height = screeninfo->yres;
	pxp_conf->s0_param.color_key = -1;
	pxp_conf->s0_param.color_key_enable = false;

	/*
	 * Initialize Output channel parameters
	 * Output is Y-only greyscale
	 * Output width/height will vary based on update region size
	 */
	pxp_conf->out_param.width = screeninfo->xres;
	pxp_conf->out_param.height = screeninfo->yres;
	pxp_conf->out_param.pixel_fmt = PXP_PIX_FMT_GREY;

	mutex_unlock(&fb_data->pxp_mutex);

	/*
	 * If HW not yet initialized, check to see if we are being sent
	 * an initialization request.
	 */
	if (!fb_data->hw_ready) {
		struct fb_videomode mode;
		u32 xres_temp;

		fb_var_to_videomode(&mode, screeninfo);

		/* When comparing requested fb mode,
		   we need to use unrotated dimensions */
		if ((screeninfo->rotate == FB_ROTATE_CW) ||
			(screeninfo->rotate == FB_ROTATE_CCW)) {
			xres_temp = mode.xres;
			mode.xres = mode.yres;
			mode.yres = xres_temp;
		}

		/*
		* If requested video mode does not match current video
		* mode, search for a matching panel.
		*/
		if (fb_data->cur_mode &&
			!fb_mode_is_equal(fb_data->cur_mode->vmode,
			&mode)) {
			bool found_match = false;

			/* Match videomode against epdc modes */
			for (i = 0; i < fb_data->pdata->num_modes; i++) {
				if (!fb_mode_is_equal(epdc_modes[i].vmode,
					&mode))
					continue;
				fb_data->cur_mode = &epdc_modes[i];
				found_match = true;
				break;
			}

			if (!found_match) {
				dev_err(fb_data->dev,
					"Failed to match requested "
					"video mode\n");
				return EINVAL;
			}
		}

		/* Found a match - Grab timing params */
		screeninfo->left_margin = mode.left_margin;
		screeninfo->right_margin = mode.right_margin;
		screeninfo->upper_margin = mode.upper_margin;
		screeninfo->lower_margin = mode.lower_margin;
		screeninfo->hsync_len = mode.hsync_len;
		screeninfo->vsync_len = mode.vsync_len;

		fb_data->hw_initializing = true;

		/* Initialize EPDC settings and init panel */
		ret =
		    mxc_epdc_fb_init_hw((struct fb_info *)fb_data);
		if (ret) {
			dev_err(fb_data->dev,
				"Failed to load panel waveform data\n");
			return ret;
		}
	}

	/*
	 * EOF sync delay (in us) should be equal to the vscan holdoff time
	 * VSCAN_HOLDOFF time = (VSCAN_HOLDOFF value + 1) * Vertical lines
	 * Add 25us for additional margin
	 */
	fb_data->eof_sync_period = (fb_data->cur_mode->vscan_holdoff + 1) *
		1000000/(fb_data->cur_mode->vmode->refresh *
		(fb_data->cur_mode->vmode->upper_margin +
		fb_data->cur_mode->vmode->yres +
		fb_data->cur_mode->vmode->lower_margin +
		fb_data->cur_mode->vmode->vsync_len)) + 25;

	mxc_epdc_fb_set_fix(info);

	return 0;
}

static int mxc_epdc_fb_check_var(struct fb_var_screeninfo *var,
				 struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = (struct mxc_epdc_fb_data *)info;
	int iPageScale=1;

	if (!var->xres)
		var->xres = 1;
	if (!var->yres)
		var->yres = 1;

	if (var->xres_virtual < var->xoffset + var->xres)
		var->xres_virtual = var->xoffset + var->xres;
	if (var->yres_virtual < var->yoffset + var->yres)
		var->yres_virtual = var->yoffset + var->yres;

	if ((var->bits_per_pixel != 32) && (var->bits_per_pixel != 24) &&
	    (var->bits_per_pixel != 16) && (var->bits_per_pixel != 8))
		var->bits_per_pixel = default_bpp;

	switch (var->bits_per_pixel) {
	case 8:
		if (var->grayscale != 0) {
			/*
			 * For 8-bit grayscale, R, G, and B offset are equal.
			 *
			 */
			var->red.length = 8;
			var->red.offset = 0;
			var->red.msb_right = 0;

			var->green.length = 8;
			var->green.offset = 0;
			var->green.msb_right = 0;

			var->blue.length = 8;
			var->blue.offset = 0;
			var->blue.msb_right = 0;

			var->transp.length = 0;
			var->transp.offset = 0;
			var->transp.msb_right = 0;
		} else {
			var->red.length = 3;
			var->red.offset = 5;
			var->red.msb_right = 0;

			var->green.length = 3;
			var->green.offset = 2;
			var->green.msb_right = 0;

			var->blue.length = 2;
			var->blue.offset = 0;
			var->blue.msb_right = 0;

			var->transp.length = 0;
			var->transp.offset = 0;
			var->transp.msb_right = 0;
		}
		break;
	case 16:
		var->red.length = 5;
		var->red.offset = 11;
		var->red.msb_right = 0;

		var->green.length = 6;
		var->green.offset = 5;
		var->green.msb_right = 0;

		var->blue.length = 5;
		var->blue.offset = 0;
		var->blue.msb_right = 0;

		var->transp.length = 0;
		var->transp.offset = 0;
		var->transp.msb_right = 0;
		break;
	case 24:
		var->red.length = 8;
		var->red.offset = 16;
		var->red.msb_right = 0;

		var->green.length = 8;
		var->green.offset = 8;
		var->green.msb_right = 0;

		var->blue.length = 8;
		var->blue.offset = 0;
		var->blue.msb_right = 0;

		var->transp.length = 0;
		var->transp.offset = 0;
		var->transp.msb_right = 0;
		break;
	case 32:
		var->red.length = 8;
		var->red.offset = 16;
		var->red.msb_right = 0;

		var->green.length = 8;
		var->green.offset = 8;
		var->green.msb_right = 0;

		var->blue.length = 8;
		var->blue.offset = 0;
		var->blue.msb_right = 0;

		var->transp.length = 8;
		var->transp.offset = 24;
		var->transp.msb_right = 0;
		break;
	}

	if(gptHWCFG) {
		// NTX HWCFG .
		switch (gptHWCFG->m_val.bDisplayBusWidth) {
		case 1: // 16 bits .
			break;
		case 2: // 8 bits mirror 
			break;
		case 3: // 16 bits mirror .
			if(FB_ROTATE_CW==var->rotate) {
				var->rotate = FB_ROTATE_CCW;
			}
			else if(FB_ROTATE_CCW==var->rotate) {
				var->rotate = FB_ROTATE_CW;
			}
			break;
		default:
		case 0:
			break;
		}
	}

	

	switch (var->rotate) {
	case FB_ROTATE_UR:
	case FB_ROTATE_UD:
		var->xres = fb_data->native_width;
		var->yres = fb_data->native_height;
		break;
	case FB_ROTATE_CW:
	case FB_ROTATE_CCW:
		var->xres = fb_data->native_height;
		var->yres = fb_data->native_width;
		break;
	default:
		/* Invalid rotation value */
		var->rotate = 0;
		dev_dbg(fb_data->dev, "Invalid rotation request\n");
		return -EINVAL;
	}


	if(var->bits_per_pixel>fb_data->default_bpp) {
		iPageScale = var->bits_per_pixel/fb_data->default_bpp ;
		if(var->bits_per_pixel%fb_data->default_bpp) {
			iPageScale += 1;
		}
	}

	var->xres_virtual = ALIGN(var->xres, 32);
	var->yres_virtual = ALIGN(var->yres, 128) * fb_data->num_screens/iPageScale;

	var->height = -1;
	var->width = -1;

	gptDC->bPixelBits = var->bits_per_pixel;
	epdfbdc_set_width_height(gptDC,var->xres_virtual,var->yres_virtual,var->xres,var->yres);
	return 0;
}

#ifdef MXCFB_WAVEFORM_MODES_NTX //[
static void mxc_epdc_fb_set_waveform_modes(struct mxcfb_waveform_modes_ntx *modes,
	struct fb_info *info)
#else //][!MXCFB_WAVEFORM_MODES_NTX
static void mxc_epdc_fb_set_waveform_modes(struct mxcfb_waveform_modes *modes,
	struct fb_info *info)
#endif //]MXCFB_WAVEFORM_MODES_NTX
{
	struct mxc_epdc_fb_data *fb_data = info ?
		(struct mxc_epdc_fb_data *)info:g_fb_data;

	mutex_lock(&fb_data->queue_mutex);
#ifdef MXCFB_WAVEFORM_MODES_NTX //[
	memcpy(&fb_data->wv_modes, modes, sizeof(struct mxcfb_waveform_modes_ntx));
#else //][!MXCFB_WAVEFORM_MODES_NTX
	memcpy(&fb_data->wv_modes, modes, sizeof(struct mxcfb_waveform_modes));
#endif //] MXCFB_WAVEFORM_MODES_NTX

	/* Set flag to ensure that new waveform modes
	 * are programmed into EPDC before next update */
	fb_data->wv_modes_update = true;

	mutex_unlock(&fb_data->queue_mutex);
}

static int mxc_epdc_fb_get_temp_index(struct mxc_epdc_fb_data *fb_data, int temp)
{
	int i;
	int index = -1;

	if (fb_data->trt_entries == 0) {
		dev_err(fb_data->dev,
			"No TRT exists...using default temp index\n");
		return DEFAULT_TEMP_INDEX;
	}

	/* Search temperature ranges for a match */
	for (i = 0; i < fb_data->trt_entries - 1; i++) {
		if ((temp >= fb_data->temp_range_bounds[i])
			&& (temp < fb_data->temp_range_bounds[i+1])) {
			index = i;
			break;
		}
	}

	if (index < 0) {
		dev_err(fb_data->dev,
			"No TRT index match (%d)\n", temp);
		if (temp < fb_data->temp_range_bounds[0]) {
			dev_warn(fb_data->dev, "temperature < minimum range\n");
			return 0;
		}
		if (temp >= fb_data->temp_range_bounds[fb_data->trt_entries-1]) {
			dev_warn(fb_data->dev, "temperature >= maximum range\n");
			return (fb_data->trt_entries-1);
		}
		return DEFAULT_TEMP_INDEX;
	}

	dev_dbg(fb_data->dev, "Using temperature index %d\n", index);

	return index;
}

int mxc_epdc_fb_read_temperature(struct mxc_epdc_fb_data *fb_data)
{
	unsigned long now;
	int temperature;
	/* Check if we need to auto update the temperature in regulate basis */
	if (!IS_ERR(fb_data->tmst_regulator) &&
		fb_data->temp_auto_update_period != FB_TEMP_AUTO_UPDATE_DISABLE)
	{
		now = get_seconds();
		if ((now - fb_data->last_time_temp_auto_update) >
				fb_data->temp_auto_update_period) {
			temperature = regulator_get_voltage(fb_data->tmst_regulator);
			dev_dbg(fb_data->dev, "auto temperature reading = %d\n", temperature);

			if (temperature != 0xFF) {
				fb_data->last_time_temp_auto_update = now;
				fb_data->temp_index = mxc_epdc_fb_get_temp_index(fb_data, temperature);
			}
		}
	}
	return 0;
}

static int mxc_epdc_fb_set_temperature(int temperature, struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = info ?
		(struct mxc_epdc_fb_data *)info:g_fb_data;

	EPDC_VPRINT(fb_data,3,"%s()\n",__FUNCTION__);
	/* Store temp index. Used later when configuring updates. */
	mutex_lock(&fb_data->queue_mutex);
	fb_data->temp_index = mxc_epdc_fb_get_temp_index(fb_data, temperature);
	mutex_unlock(&fb_data->queue_mutex);

	return 0;
}

int mxc_epdc_fb_set_temp_auto_update_period(int period, struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = info ?
		(struct mxc_epdc_fb_data *)info:g_fb_data;

	fb_data->temp_auto_update_period = period;

	return 0;
}

static int mxc_epdc_fb_set_auto_update(u32 auto_mode, struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = info ?
		(struct mxc_epdc_fb_data *)info:g_fb_data;

	dev_dbg(fb_data->dev, "Setting auto update mode to %d\n", auto_mode);

	if ((auto_mode == AUTO_UPDATE_MODE_AUTOMATIC_MODE)
		|| (auto_mode == AUTO_UPDATE_MODE_REGION_MODE))
		fb_data->auto_mode = auto_mode;
	else {
		dev_err(fb_data->dev, "Invalid auto update mode parameter.\n");
		return -EINVAL;
	}

	return 0;
}

static int mxc_epdc_fb_set_upd_scheme(u32 upd_scheme, struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = info ?
		(struct mxc_epdc_fb_data *)info:g_fb_data;

	dev_dbg(fb_data->dev, "Setting optimization level to %d\n", upd_scheme);

	/*
	 * Can't change the scheme until current updates have completed.
	 * This function returns when all active updates are done.
	 */
	mxc_epdc_fb_flush_updates(fb_data);

	if ((upd_scheme == UPDATE_SCHEME_SNAPSHOT)
		|| (upd_scheme == UPDATE_SCHEME_QUEUE)
		|| (upd_scheme == UPDATE_SCHEME_QUEUE_AND_MERGE)) 
	{

		switch (upd_scheme) {
		case UPDATE_SCHEME_SNAPSHOT:
			printk(KERN_ERR"update scheme <= snapshot\n");
			break;
		case UPDATE_SCHEME_QUEUE:
			printk(KERN_ERR"update scheme <= queue\n");
			break;
		case UPDATE_SCHEME_QUEUE_AND_MERGE:
			printk(KERN_ERR"update scheme <= queue and merge\n");
			break;
		default :
			printk(KERN_ERR"update scheme <= unknow \n");
			break;
		}

		fb_data->upd_scheme = upd_scheme;
	}
	else {
		dev_err(fb_data->dev, "Invalid update scheme specified.\n");
		return -EINVAL;
	}

	return 0;
}

static void copy_before_process(struct mxc_epdc_fb_data *fb_data,
	struct update_data_list *upd_data_list)
{
	struct mxcfb_update_data *upd_data =
		&upd_data_list->update_desc->upd_data;
	int i;
	unsigned char *temp_buf_ptr = fb_data->virt_addr_copybuf;
	unsigned char *src_ptr;
	struct mxcfb_rect *src_upd_region;
	int temp_buf_stride;
	int src_stride;
	int bpp = fb_data->epdc_fb_var.bits_per_pixel;
	int left_offs, right_offs;
	int x_trailing_bytes, y_trailing_bytes;
	int alt_buf_offset;

	/* Set source buf pointer based on input source, panning, etc. */
	if (upd_data->flags & EPDC_FLAG_USE_ALT_BUFFER) {
		src_upd_region = &upd_data->alt_buffer_data.alt_update_region;
		src_stride =
			upd_data->alt_buffer_data.width * bpp/8;
		alt_buf_offset = upd_data->alt_buffer_data.phys_addr -
			fb_data->info.fix.smem_start;
		src_ptr = fb_data->info.screen_base + alt_buf_offset
			+ src_upd_region->top * src_stride;
	} else {
		src_upd_region = &upd_data->update_region;
		src_stride = fb_data->epdc_fb_var.xres_virtual * bpp/8;
		src_ptr = fb_data->info.screen_base + fb_data->fb_offset
			+ src_upd_region->top * src_stride;
	}

	temp_buf_stride = ALIGN(src_upd_region->width, 8) * bpp/8;
	left_offs = src_upd_region->left * bpp/8;
	right_offs = src_upd_region->width * bpp/8;
	x_trailing_bytes = (ALIGN(src_upd_region->width, 8)
		- src_upd_region->width) * bpp/8;

	for (i = 0; i < src_upd_region->height; i++) {
		/* Copy the full line */
		memcpy(temp_buf_ptr, src_ptr + left_offs,
			src_upd_region->width * bpp/8);

		/* Clear any unwanted pixels at the end of each line */
		if (src_upd_region->width & 0x7) {
			memset(temp_buf_ptr + right_offs, 0x0,
				x_trailing_bytes);
		}

		temp_buf_ptr += temp_buf_stride;
		src_ptr += src_stride;
	}

	/* Clear any unwanted pixels at the bottom of the end of each line */
	if (src_upd_region->height & 0x7) {
		y_trailing_bytes = (ALIGN(src_upd_region->height, 8)
			- src_upd_region->height) *
			ALIGN(src_upd_region->width, 8) * bpp/8;
		memset(temp_buf_ptr, 0x0, y_trailing_bytes);
	}
}

/* Before every update to panel, we should call this
 * function to update the working buffer first.
 */
static int epdc_working_buffer_update(struct mxc_epdc_fb_data *fb_data,
				      struct update_data_list *upd_data_list,
				      struct mxcfb_rect *update_region)
{
	struct pxp_proc_data *proc_data = &fb_data->pxp_conf.proc_data;
	u32 wv_mode = upd_data_list->update_desc->upd_data.waveform_mode;
	int ret = 0, acd_version = 0;
	u32 adth_addr = 0;
	u32 temp_index = 0;
	u32 hist_stat;
	u32 pixel_nums;
	struct update_desc_list *upd_desc_list;
	ktime_t pxp_a_proc_stime;
	ktime_t pxp_b_proc_stime;

	/* get PXP mutex before updating PXP params */
	mutex_lock(&fb_data->pxp_mutex);
	/* Check if we are using advance waveform with ACD data.*/
	if (fb_data->waveform_acd_buffer != NULL) {

		/* update temp index */
		if (upd_data_list->update_desc->upd_data.temp != TEMP_USE_AMBIENT) {
			temp_index = mxc_epdc_fb_get_temp_index(fb_data,
				upd_data_list->update_desc->upd_data.temp);
		}
		acd_version = epdc_get_adc_version(fb_data,
						wv_mode,
						temp_index,
						&adth_addr);
	}

	if ((acd_version == WFM_ACD_VER_REAGL)
		|| (acd_version == WFM_ACD_VER_REAGLD)
		|| (acd_version == WFM_ACD_VER_ECLIPSE)) {

		dev_info(fb_data->dev, "Algorithm processing needed %d\n", acd_version);
		proc_data->reagl_en = 1;
		proc_data->reagl_d_en = (acd_version == WFM_ACD_VER_REAGLD) ? 1 : 0;
	} else {
		proc_data->reagl_en = 0;
		proc_data->reagl_d_en = 0;
	}

	pxp_a_proc_stime = ktime_get();
	ret = pxp_wfe_a_process(fb_data, update_region, upd_data_list);
	if (ret) {
		dev_err(fb_data->dev, "Unable to submit PxP update task.\n");
		mutex_unlock(&fb_data->pxp_mutex);
		return ret;
	}
	mutex_unlock(&fb_data->pxp_mutex);

	/* If needed, enable EPDC HW while ePxP is processing */
	if ((fb_data->power_state == POWER_STATE_OFF)
		|| fb_data->powering_down) {
		epdc_powerup(fb_data);
	}

	/* This is a blocking call, so upon return PxP tx should be done */
	ret = pxp_complete_update(fb_data, &fb_data->hist_status,
					&fb_data->pixel_nums);
	if (ret) {
		dev_err(fb_data->dev, "Unable to complete PxP update task: main process\n");
		mutex_unlock(&fb_data->pxp_mutex);
		return ret;
	}

	EPDC_VPRINT(fb_data,1,"%s : pxp a proc takes %d usecs,reagl_en=%d,reagl_d_en=%d\n",
			__func__,ktime_get_diffus(pxp_a_proc_stime),
			proc_data->reagl_en,
			proc_data->reagl_d_en);
	

	upd_desc_list = upd_data_list->update_desc;
	if (fb_data->epdc_wb_mode &&
		(upd_desc_list->upd_data.waveform_mode == WAVEFORM_MODE_AUTO)) {
		hist_stat = fb_data->hist_status;

		if (hist_stat & 0x1)
#ifdef AUTO_NTX_MODES //[
			upd_desc_list->upd_data.waveform_mode =
				giNTX_waveform_modeA[NTX_WFM_MODE_DU];
#else //][! AUTO_NTX_MODES
			upd_desc_list->upd_data.waveform_mode =
				fb_data->wv_modes.mode_du;
#endif //] AUTO_NTX_MODES
		else if (hist_stat & 0x2)
#ifdef AUTO_NTX_MODES //[
			upd_desc_list->upd_data.waveform_mode =
				giNTX_waveform_modeA[NTX_WFM_MODE_GC4];
#else //][!AUTO_NTX_MODES
			upd_desc_list->upd_data.waveform_mode =
				fb_data->wv_modes.mode_gc4;
#endif //] AUTO_NTX_MODES
		else if (hist_stat & 0x4)
#if 0//AUTO_NTX_MODES //[
			upd_desc_list->upd_data.waveform_mode =
				giNTX_waveform_modeA[NTX_WFM_MODE_GL16];
#else //][!AUTO_NTX_MODES
			upd_desc_list->upd_data.waveform_mode =
				fb_data->wv_modes.mode_gc8;
#endif //] AUTO_NTX_MODES
		else if (hist_stat & 0x8)
#ifdef AUTO_NTX_MODES //[
			if(upd_desc_list->upd_data.update_mode==UPDATE_MODE_PARTIAL)
				upd_desc_list->upd_data.waveform_mode =
					giNTX_waveform_modeA[NTX_WFM_MODE_GL16];
			else 
				upd_desc_list->upd_data.waveform_mode =
					giNTX_waveform_modeA[NTX_WFM_MODE_GC16];
#else //][!AUTO_NTX_MODES
			upd_desc_list->upd_data.waveform_mode =
				fb_data->wv_modes.mode_gc16;
#endif //] AUTO_NTX_MODES
		else
			upd_desc_list->upd_data.waveform_mode =
				fb_data->wv_modes.mode_gc32;


		EPDC_VPRINT(fb_data,2,"hist_stat = 0x%x, new waveform = 0x%x\n",
			hist_stat, upd_desc_list->upd_data.waveform_mode);
	}

	if (proc_data->detection_only == 1) {
		dev_info(fb_data->dev, "collision detection only, no real update\n");
		mutex_unlock(&fb_data->pxp_mutex);
		return 0;
	}

	if (fb_data->col_info.pixel_cnt) {
		//dev_dbg(fb_data->dev, "collision detected, can not do REAGl/-D\n");
		EPDC_VPRINT(fb_data,3,"collision detected, can not do REAGl/-D\n");
		mutex_unlock(&fb_data->pxp_mutex);
		return 0;
	}

	/* for REAGL/-D Processing */
	if ((acd_version == WFM_ACD_VER_REAGL)
		|| (acd_version == WFM_ACD_VER_REAGLD)
		|| (acd_version == WFM_ACD_VER_ECLIPSE)) {
		/* get algo parameters SFT */
		ret = fetch_epdc_adc_data(fb_data, wv_mode, temp_index, adth_addr);
		if (ret) {
			dev_err(fb_data->dev, "Algorithm data parser error.\n");
			return ret;
		}
		proc_data->reagl_en = 1;
		proc_data->reagl_d_en = (acd_version == WFM_ACD_VER_REAGLD) ? 1 : 0;
		dev_info(fb_data->dev, "ACD data SFT:%d ASV:%d\n", proc_data->reagl_sft, proc_data->reagl_asv);
		/* This is a blocking call, so upon return PxP tx should be done */

		pxp_b_proc_stime = ktime_get();
		mutex_lock(&fb_data->pxp_mutex);
		ret = pxp_wfe_b_process_update(fb_data, update_region);
		if (ret) {
			dev_err(fb_data->dev, "Unable to submit PxP update task.\n");
			mutex_unlock(&fb_data->pxp_mutex);
			return ret;
		}
		mutex_unlock(&fb_data->pxp_mutex);

		/* If needed, enable EPDC HW while ePxP is processing */
		if ((fb_data->power_state == POWER_STATE_OFF)
			|| fb_data->powering_down) {
			epdc_powerup(fb_data);
		}

		/* This is a blocking call, so upon return PxP tx should be done */
		mutex_lock(&fb_data->pxp_mutex);
		ret = pxp_complete_update(fb_data, &hist_stat, &pixel_nums);
		if (ret) {
			dev_err(fb_data->dev, "Unable to complete PxP update task: reagl/-d process\n");
			mutex_unlock(&fb_data->pxp_mutex);
			return ret;
		}
		EPDC_VPRINT(fb_data,1,"%s : pxp b proc takes %d usecs\n",__func__,ktime_get_diffus(pxp_b_proc_stime));
		mutex_unlock(&fb_data->pxp_mutex);

	}

	mutex_unlock(&fb_data->pxp_mutex);

	return 0;
}

static int epdc_process_update(struct update_data_list *upd_data_list,
				   struct mxc_epdc_fb_data *fb_data)
{
	struct mxcfb_rect *src_upd_region; /* Region of src buffer for update */
	struct mxcfb_rect pxp_upd_region;
	u32 src_width, src_height;
	u32 offset_from_4, bytes_per_pixel;
	u32 post_rotation_xcoord, post_rotation_ycoord, width_pxp_blocks;
	u32 pxp_input_offs, pxp_output_offs, pxp_output_shift;
	u32 hist_stat = 0;
	u32 pixel_nums = 0;
	int width_unaligned, height_unaligned;
	bool input_unaligned = false;
	bool line_overflow = false;
	int pix_per_line_added;
	bool use_temp_buf = false;
	struct mxcfb_rect temp_buf_upd_region;
	struct update_desc_list *upd_desc_list = upd_data_list->update_desc;

	int ret;

	/*
	 * Gotta do a whole bunch of buffer ptr manipulation to
	 * work around HW restrictions for PxP & EPDC
	 * Note: Applies to pre-2.0 versions of EPDC/PxP
	 */

	/*
	 * Are we using FB or an alternate (overlay)
	 * buffer for source of update?
	 */
	if (upd_desc_list->upd_data.flags & EPDC_FLAG_USE_ALT_BUFFER) {
		src_width = upd_desc_list->upd_data.alt_buffer_data.width;
		src_height = upd_desc_list->upd_data.alt_buffer_data.height;
		src_upd_region = &upd_desc_list->upd_data.alt_buffer_data.alt_update_region;
	} else {
		src_width = fb_data->epdc_fb_var.xres_virtual;
		src_height = fb_data->epdc_fb_var.yres;
		src_upd_region = &upd_desc_list->upd_data.update_region;
	}

	bytes_per_pixel = fb_data->epdc_fb_var.bits_per_pixel/8;

	/*
	 * SW workaround for PxP limitation (for pre-v2.0 HW)
	 *
	 * There are 3 cases where we cannot process the update data
	 * directly from the input buffer:
	 *
	 * 1) PxP must process 8x8 pixel blocks, and all pixels in each block
	 * are considered for auto-waveform mode selection. If the
	 * update region is not 8x8 aligned, additional unwanted pixels
	 * will be considered in auto-waveform mode selection.
	 *
	 * 2) PxP input must be 32-bit aligned, so any update
	 * address not 32-bit aligned must be shifted to meet the
	 * 32-bit alignment.  The PxP will thus end up processing pixels
	 * outside of the update region to satisfy this alignment restriction,
	 * which can affect auto-waveform mode selection.
	 *
	 * 3) If input fails 32-bit alignment, and the resulting expansion
	 * of the processed region would add at least 8 pixels more per
	 * line than the original update line width, the EPDC would
	 * cause screen artifacts by incorrectly handling the 8+ pixels
	 * at the end of each line.
	 *
	 * Workaround is to copy from source buffer into a temporary
	 * buffer, which we pad with zeros to match the 8x8 alignment
	 * requirement. This temp buffer becomes the input to the PxP.
	 */
	width_unaligned = src_upd_region->width & 0x7;
	height_unaligned = src_upd_region->height & 0x7;

	offset_from_4 = src_upd_region->left & 0x3;
	input_unaligned = ((offset_from_4 * bytes_per_pixel % 4) != 0) ?
				true : false;

	pix_per_line_added = (offset_from_4 * bytes_per_pixel % 4)
					/ bytes_per_pixel;
	if ((((fb_data->epdc_fb_var.rotate == FB_ROTATE_UR) ||
		fb_data->epdc_fb_var.rotate == FB_ROTATE_UD)) &&
		(ALIGN(src_upd_region->width, 8) <
			ALIGN(src_upd_region->width + pix_per_line_added, 8)))
		line_overflow = true;

	/* Grab pxp_mutex here so that we protect access
	 * to copybuf in addition to the PxP structures */
	mutex_lock(&fb_data->pxp_mutex);

	if (((((width_unaligned || height_unaligned || input_unaligned) &&
		(upd_desc_list->upd_data.waveform_mode == WAVEFORM_MODE_AUTO))
		|| line_overflow) && (fb_data->rev < 20)) ||
		fb_data->restrict_width) {
		dev_dbg(fb_data->dev, "Copying update before processing.\n");

		/* Update to reflect what the new source buffer will be */
		src_width = ALIGN(src_upd_region->width, 8);
		src_height = ALIGN(src_upd_region->height, 8);

		copy_before_process(fb_data, upd_data_list);

		/*
		 * src_upd_region should now describe
		 * the new update buffer attributes.
		 */
		temp_buf_upd_region.left = 0;
		temp_buf_upd_region.top = 0;
		temp_buf_upd_region.width = src_upd_region->width;
		temp_buf_upd_region.height = src_upd_region->height;
		src_upd_region = &temp_buf_upd_region;

		use_temp_buf = true;
	}

	/*
	 * For pre-2.0 HW, input address must be 32-bit aligned
	 * Compute buffer offset to account for this PxP limitation
	 */
	offset_from_4 = src_upd_region->left & 0x3;
	input_unaligned = ((offset_from_4 * bytes_per_pixel % 4) != 0) ?
				true : false;
	if ((fb_data->rev < 20) && input_unaligned) {
		/* Leave a gap between PxP input addr and update region pixels */
		pxp_input_offs =
			(src_upd_region->top * src_width + src_upd_region->left)
			* bytes_per_pixel & 0xFFFFFFFC;
		/* Update region left changes to reflect relative position to input ptr */
		pxp_upd_region.left = (offset_from_4 * bytes_per_pixel % 4)
					/ bytes_per_pixel;
	} else {
		pxp_input_offs =
			(src_upd_region->top * src_width + src_upd_region->left)
			* bytes_per_pixel;
		pxp_upd_region.left = 0;
	}

	pxp_upd_region.top = 0;

	/*
	 * For version 2.0 and later of EPDC & PxP, if no rotation, we don't
	 * need to align width & height (rotation always requires 8-pixel
	 * width & height alignment, per PxP limitations)
	 */
	if ((fb_data->epdc_fb_var.rotate == 0) && (fb_data->rev >= 20)) {
		pxp_upd_region.width = src_upd_region->width;
		pxp_upd_region.height = src_upd_region->height;
	} else {
		/* Update region dimensions to meet 8x8 pixel requirement */
		pxp_upd_region.width = ALIGN(src_upd_region->width + pxp_upd_region.left, 8);
		pxp_upd_region.height = ALIGN(src_upd_region->height, 8);
	}

	switch (fb_data->epdc_fb_var.rotate) {
	case FB_ROTATE_UR:
	default:
		post_rotation_xcoord = pxp_upd_region.left;
		post_rotation_ycoord = pxp_upd_region.top;
		width_pxp_blocks = pxp_upd_region.width;
		break;
	case FB_ROTATE_CW:
		width_pxp_blocks = pxp_upd_region.height;
		post_rotation_xcoord = width_pxp_blocks - src_upd_region->height;
		post_rotation_ycoord = pxp_upd_region.left;
		break;
	case FB_ROTATE_UD:
		width_pxp_blocks = pxp_upd_region.width;
		post_rotation_xcoord = width_pxp_blocks - src_upd_region->width - pxp_upd_region.left;
		post_rotation_ycoord = pxp_upd_region.height - src_upd_region->height - pxp_upd_region.top;
		break;
	case FB_ROTATE_CCW:
		width_pxp_blocks = pxp_upd_region.height;
		post_rotation_xcoord = pxp_upd_region.top;
		post_rotation_ycoord = pxp_upd_region.width - src_upd_region->width - pxp_upd_region.left;
		break;
	}

	/* Update region start coord to force PxP to process full 8x8 regions */
	pxp_upd_region.top &= ~0x7;
	pxp_upd_region.left &= ~0x7;

	if (fb_data->rev < 20) {
		pxp_output_shift = ALIGN(post_rotation_xcoord, 8)
			- post_rotation_xcoord;

		pxp_output_offs = post_rotation_ycoord * width_pxp_blocks
			+ pxp_output_shift;

		upd_desc_list->epdc_offs = ALIGN(pxp_output_offs, 8);
	} else {
		pxp_output_shift = 0;
		pxp_output_offs = post_rotation_ycoord * width_pxp_blocks
			+ post_rotation_xcoord;

		upd_desc_list->epdc_offs = pxp_output_offs;
	}

	upd_desc_list->epdc_stride = width_pxp_blocks;

	/* Source address either comes from alternate buffer
	   provided in update data, or from the framebuffer. */
	if (use_temp_buf)
		sg_dma_address(&fb_data->sg[0]) =
			fb_data->phys_addr_copybuf;
	else if (upd_desc_list->upd_data.flags & EPDC_FLAG_USE_ALT_BUFFER)
		sg_dma_address(&fb_data->sg[0]) =
			upd_desc_list->upd_data.alt_buffer_data.phys_addr
				+ pxp_input_offs;
	else {
		sg_dma_address(&fb_data->sg[0]) =
			fb_data->info.fix.smem_start + fb_data->fb_offset
			+ pxp_input_offs;
		sg_set_page(&fb_data->sg[0],
			virt_to_page(fb_data->info.screen_base),
			fb_data->info.fix.smem_len,
			offset_in_page(fb_data->info.screen_base));
	}

	/* Update sg[1] to point to output of PxP proc task */
	sg_dma_address(&fb_data->sg[1]) = upd_data_list->phys_addr
						+ pxp_output_shift;
	sg_set_page(&fb_data->sg[1], virt_to_page(upd_data_list->virt_addr),
		    fb_data->max_pix_size,
		    offset_in_page(upd_data_list->virt_addr));

	/*
	 * Set PxP LUT transform type based on update flags.
	 */
	fb_data->pxp_conf.proc_data.lut_transform = 0;
	if (upd_desc_list->upd_data.flags & EPDC_FLAG_ENABLE_INVERSION)
		fb_data->pxp_conf.proc_data.lut_transform |= PXP_LUT_INVERT;
	if (upd_desc_list->upd_data.flags & EPDC_FLAG_FORCE_MONOCHROME)
		fb_data->pxp_conf.proc_data.lut_transform |=
			PXP_LUT_BLACK_WHITE;
	if (upd_desc_list->upd_data.flags & EPDC_FLAG_USE_CMAP)
		fb_data->pxp_conf.proc_data.lut_transform |=
			PXP_LUT_USE_CMAP;

	/*
	 * Toggle inversion processing if 8-bit
	 * inverted is the current pixel format.
	 */
	if (fb_data->epdc_fb_var.grayscale == GRAYSCALE_8BIT_INVERTED)
		fb_data->pxp_conf.proc_data.lut_transform ^= PXP_LUT_INVERT;

#ifdef	USE_PS_AS_OUTPUT
	/* This is a blocking call, so upon return PxP tx should be done */
	ret = pxp_legacy_process(fb_data, src_width, src_height,
		&pxp_upd_region);
	if (ret) {
		dev_err(fb_data->dev, "Unable to submit PxP update task.\n");
		mutex_unlock(&fb_data->pxp_mutex);
		return ret;
	}

	/* If needed, enable EPDC HW while ePxP is processing */
	if ((fb_data->power_state == POWER_STATE_OFF)
		|| fb_data->powering_down) {
		epdc_powerup(fb_data);
	}

	/* This is a blocking call, so upon return PxP tx should be done */
	ret = pxp_complete_update(fb_data, &hist_stat, &pixel_nums);
	if (ret) {
		dev_err(fb_data->dev, "Unable to complete PxP update task: pre_prcoess.\n");
		mutex_unlock(&fb_data->pxp_mutex);
		return ret;
	}
#endif
	pr_debug(" upd_data.dither_mode %d  \n", upd_desc_list->upd_data.dither_mode);
	fb_data->pxp_conf.proc_data.dither_mode = 0;

	/* Dithering */
	if ((EPDC_FLAG_USE_DITHERING_PASSTHROUGH < upd_desc_list->upd_data.dither_mode) &&
		(upd_desc_list->upd_data.dither_mode < EPDC_FLAG_USE_DITHERING_MAX)) {

		DBG_MSG(" upd_data.dither_mode %d  \n", upd_desc_list->upd_data.dither_mode);
		fb_data->pxp_conf.proc_data.dither_mode = upd_desc_list->upd_data.dither_mode;
		fb_data->pxp_conf.proc_data.quant_bit = upd_desc_list->upd_data.quant_bit;

		/* This is a blocking call, so upon return PxP tx should be done */
		ret = pxp_process_dithering(fb_data, &pxp_upd_region);
		if (ret) {
			dev_err(fb_data->dev, "Unable to submit PxP update task.\n");
			mutex_unlock(&fb_data->pxp_mutex);
			return ret;
		}

		/* If needed, enable EPDC HW while ePxP is processing */
		if ((fb_data->power_state == POWER_STATE_OFF)
			|| fb_data->powering_down) {
			epdc_powerup(fb_data);
		}

		/* This is a blocking call, so upon return PxP tx should be done */
		ret = pxp_complete_update(fb_data, &hist_stat, &pixel_nums);
		if (ret) {
			dev_err(fb_data->dev, "Unable to complete PxP update task: dithering process\n");
			mutex_unlock(&fb_data->pxp_mutex);
			return ret;
		}

	}

	mutex_unlock(&fb_data->pxp_mutex);

	/* Update waveform mode from PxP histogram results */
	if ((fb_data->rev <= 20) &&
		(upd_desc_list->upd_data.waveform_mode == WAVEFORM_MODE_AUTO)) {
		if (hist_stat & 0x1)
			upd_desc_list->upd_data.waveform_mode =
				fb_data->wv_modes.mode_du;
		else if (hist_stat & 0x2)
			upd_desc_list->upd_data.waveform_mode =
				fb_data->wv_modes.mode_gc4;
		else if (hist_stat & 0x4)
			upd_desc_list->upd_data.waveform_mode =
				fb_data->wv_modes.mode_gc8;
		else if (hist_stat & 0x8)
			upd_desc_list->upd_data.waveform_mode =
				fb_data->wv_modes.mode_gc16;
		else
			upd_desc_list->upd_data.waveform_mode =
				fb_data->wv_modes.mode_gc32;

		dev_dbg(fb_data->dev, "hist_stat = 0x%x, new waveform = 0x%x\n",
			hist_stat, upd_desc_list->upd_data.waveform_mode);
	}

	return 0;
}

static int epdc_submit_merge(struct update_desc_list *upd_desc_list,
				struct update_desc_list *update_to_merge,
				struct mxc_epdc_fb_data *fb_data)
{
	struct mxcfb_update_data *a, *b;
	struct mxcfb_rect *arect, *brect;
	struct mxcfb_rect combine;
	bool use_flags = false;

	a = &upd_desc_list->upd_data;
	b = &update_to_merge->upd_data;
	arect = &upd_desc_list->upd_data.update_region;
	brect = &update_to_merge->upd_data.update_region;

	/* Do not merge a dry-run collision test update */
	if ((a->flags & EPDC_FLAG_TEST_COLLISION) ||
		(b->flags & EPDC_FLAG_TEST_COLLISION))
		return MERGE_BLOCK;

	/*
	 * Updates with different flags must be executed sequentially.
	 * Halt the merge process to ensure this.
	 */
	if (a->flags != b->flags) {
		/*
		 * Special exception: if update regions are identical,
		 * we may be able to merge them.
		 */
		if ((arect->left != brect->left) ||
			(arect->top != brect->top) ||
			(arect->width != brect->width) ||
			(arect->height != brect->height))
			return MERGE_BLOCK;

		use_flags = true;
	}

	if (a->update_mode != b->update_mode)
		a->update_mode = UPDATE_MODE_FULL;

	if (a->waveform_mode != b->waveform_mode)
		a->waveform_mode = WAVEFORM_MODE_AUTO;

	if (arect->left > (brect->left + brect->width) ||
		brect->left > (arect->left + arect->width) ||
		arect->top > (brect->top + brect->height) ||
		brect->top > (arect->top + arect->height))
		return MERGE_FAIL;

	combine.left = arect->left < brect->left ? arect->left : brect->left;
	combine.top = arect->top < brect->top ? arect->top : brect->top;
	combine.width = (arect->left + arect->width) >
			(brect->left + brect->width) ?
			(arect->left + arect->width - combine.left) :
			(brect->left + brect->width - combine.left);
	combine.height = (arect->top + arect->height) >
			(brect->top + brect->height) ?
			(arect->top + arect->height - combine.top) :
			(brect->top + brect->height - combine.top);

	/* Don't merge if combined width exceeds max width */
	if (fb_data->restrict_width) {
		u32 max_width = EPDC_V2_MAX_UPDATE_WIDTH;
		u32 combined_width = combine.width;
		if (fb_data->epdc_fb_var.rotate != FB_ROTATE_UR)
			max_width -= EPDC_V2_ROTATION_ALIGNMENT;
		if ((fb_data->epdc_fb_var.rotate == FB_ROTATE_CW) ||
			(fb_data->epdc_fb_var.rotate == FB_ROTATE_CCW))
			combined_width = combine.height;
		if (combined_width > max_width)
			return MERGE_FAIL;
	}

	*arect = combine;

	/* Use flags of the later update */
	if (use_flags)
		a->flags = b->flags;

	/* Merge markers */
	list_splice_tail(&update_to_merge->upd_marker_list,
		&upd_desc_list->upd_marker_list);

	/* Merged update should take on the earliest order */
	upd_desc_list->update_order =
		(upd_desc_list->update_order > update_to_merge->update_order) ?
		upd_desc_list->update_order : update_to_merge->update_order;

	return MERGE_OK;
}
#ifdef FW_IN_RAM //[

static void epdc_firmware_work_func(struct work_struct *work) 
{
	struct mxc_epdc_fb_data *fb_data =
		container_of(work, struct mxc_epdc_fb_data, epdc_firmware_work);
	struct firmware fw;


	if(!delayed_work_pending(&fb_data->epdc_firmware_work)) {

		down(&fb_data->firmware_work_lock);
		if(0==gbModeVersion) {
			fw.size = gdwWF_size;
			fw.data = (u8*)gpbWF_vaddr;

			printk("[%s]:fw p=%p,size=%u,fb_data@%p,pdata=%p\n",__FUNCTION__,
				fw.data,fw.size,fb_data,fb_data->pdata);
			mxc_epdc_fb_fw_handler(&fw,fb_data);
		}
		else {
			printk(KERN_WARNING"%s skipped cause firmware bas been setup\n",__FUNCTION__);
		}
		up(&fb_data->firmware_work_lock);
	}
	else {
		printk(KERN_WARNING"%s skipped cause firmware work is pending \n",__FUNCTION__);
	}
}

#endif //] FW_IN_RAM

#define min(a,b)	(((a)>(b))?b:a)
#define max(a,b)	(((a)>(b))?a:b)
static inline int _is_region_overlap(struct mxcfb_rect *r1,struct mxcfb_rect *r2)
{
	int x_overlap = min(r1->left+r1->width,r2->left+r2->width) - max(r1->left,r2->left);
	int y_overlap = min(r1->top+r1->height,r2->top+r2->height) - max(r1->top,r2->top);
	//printk("x_overlap=%d,y_overlap=%d\n",x_overlap,y_overlap);

	x_overlap=(x_overlap<0)?0:x_overlap;
	y_overlap=(y_overlap<0)?0:y_overlap;
	//return (x_overlap*y_overlap);
	return (x_overlap&&y_overlap);
}

static void epdc_submit_work_func(struct work_struct *work)
{
	int temp_index;
	struct update_data_list *next_update, *temp_update;
	struct update_desc_list *next_desc, *temp_desc;
	struct update_marker_data *next_marker, *temp_marker;
	struct mxc_epdc_fb_data *fb_data =
		container_of(work, struct mxc_epdc_fb_data, epdc_submit_work);
	struct pxp_config_data *pxp_conf = &fb_data->pxp_conf;
	struct pxp_proc_data *proc_data = &pxp_conf->proc_data;
	struct update_data_list *upd_data_list = NULL;
	struct mxcfb_rect adj_update_region, *upd_region;
	bool end_merge = false;
	bool is_transform;
	u32 update_addr;
	int *err_dist;
	int ret;
#if DROP_OLD_COLLISION //[
	bool ignore_this_collision ;
	u32 lut;
#endif //]DROP_OLD_COLLISION

	EPDC_VPRINT(fb_data,6,"%s() begin-updating w=%d,h=%d\n",
			__FUNCTION__,fb_data->active_updating_w,fb_data->active_updating_h);



	/* Protect access to buffer queues and to update HW */
	mutex_lock(&fb_data->queue_mutex);

	/*
	 * Are any of our collision updates able to go now?
	 * Go through all updates in the collision list and check to see
	 * if the collision mask has been fully cleared
	 */
	list_for_each_entry_safe(next_update, temp_update,
				&fb_data->upd_buf_collision_list, list) {



		if (next_update->collision_mask != 0)
			continue;

#if DROP_OLD_COLLISION //[
		/*
		 * If we collide with newer updates, then
		 * we don't need to re-submit the update. The
		 * idea is that the newer updates should take
		 * precedence anyways, so we don't want to
		 * overwrite them.
		 */
		
		for (ignore_this_collision = false, lut = 0;
				lut<fb_data->num_luts;lut++) 
		{
			if(fb_data->lut_update_order[lut]) 
			{
				if ( (fb_data->lut_update_order[lut] >
					next_update->update_desc->update_order) ) 
				{
					
					struct mxcfb_rect adj_update_region;
					
					adjust_coordinates(fb_data->epdc_fb_var.xres,
						fb_data->epdc_fb_var.yres, fb_data->epdc_fb_var.rotate,
						&next_update->update_desc->upd_data.update_region,
						&adj_update_region);

					EPDC_VPRINT(fb_data,1,
						"checking collision (lut%d,order=%d<%d) with newer update.\n",
						next_update->lut_num,
						next_update->update_desc->update_order,
						fb_data->lut_update_order[lut] );

					EPDC_VPRINT(fb_data,2,"check r1(%d,%d,%d,%d),r2(%d,%d,%d,%d) overlapping\n",
						fb_data->lut_rect[lut].left,
						fb_data->lut_rect[lut].top,
						fb_data->lut_rect[lut].width,
						fb_data->lut_rect[lut].height,
						adj_update_region.left,
						adj_update_region.top,
						adj_update_region.width,
						adj_update_region.height);

					if(_is_region_overlap(&fb_data->lut_rect[lut],&adj_update_region) &&
					 fb_data->lut_rect[lut].left==adj_update_region.left &&
					 fb_data->lut_rect[lut].top==adj_update_region.top &&
					 fb_data->lut_rect[lut].width==adj_update_region.width &&
					 fb_data->lut_rect[lut].height==adj_update_region.height )
					{

						EPDC_VPRINT(fb_data,1,"ignore overlapping collision r1(%d,%d,%d,%d),r2(%d,%d,%d,%d)\n",
							fb_data->lut_rect[lut].left,
							fb_data->lut_rect[lut].top,
							fb_data->lut_rect[lut].width,
							fb_data->lut_rect[lut].height,
							adj_update_region.left,
							adj_update_region.top,
							adj_update_region.width,
							adj_update_region.height);
						ignore_this_collision = true;
					}
					break;
				}
			}
		}

		if(ignore_this_collision) {
			list_del_init(&next_update->update_desc->list);
			kfree(next_update->update_desc);
			list_del_init(&next_update->list);
			list_add_tail(&next_update->list,&fb_data->upd_buf_free_list);
			//list_del(&next_update->list);
			break;
		}

#endif //] DROP_OLD_COLLISION

		dev_dbg(fb_data->dev, "A collision update is ready to go!\n");
		EPDC_VPRINT(fb_data,3, "A collision update is ready to go!\n");

		/* Force waveform mode to auto for resubmitted collisions */
		next_update->update_desc->upd_data.waveform_mode =
			WAVEFORM_MODE_AUTO;

		/*
		 * We have a collision cleared, so select it for resubmission.
		 * If an update is already selected, attempt to merge.
		 */
		if (!upd_data_list) {
			upd_data_list = next_update;
			list_del_init(&next_update->list);
			if (fb_data->upd_scheme == UPDATE_SCHEME_QUEUE)
				/* If not merging, we have our update */
				break;
		} else {
			switch (epdc_submit_merge(upd_data_list->update_desc,
						next_update->update_desc,
						fb_data)) {
			case MERGE_OK:
				dev_dbg(fb_data->dev,
					"Update merged [collision]\n");
				EPDC_VPRINT(fb_data,3,
					"Update merged [collision]\n");
				list_del_init(&next_update->update_desc->list);
				kfree(next_update->update_desc);
				next_update->update_desc = NULL;
				list_del_init(&next_update->list);
				/* Add to free buffer list */
				list_add_tail(&next_update->list,
					 &fb_data->upd_buf_free_list);
				break;
			case MERGE_FAIL:
				dev_dbg(fb_data->dev,
					"Update not merged [collision]\n");
				EPDC_VPRINT(fb_data,3,
					"Update not merged [collision]\n");
				break;
			case MERGE_BLOCK:
				dev_dbg(fb_data->dev,
					"Merge blocked [collision]\n");
				EPDC_VPRINT(fb_data,3,
					"Merge blocked [collision]\n");
				end_merge = true;
				break;
			}

			if (end_merge) {
				end_merge = false;
				break;
			}
		}
	}

	/*
	 * Skip pending update list only if we found a collision
	 * update and we are not merging
	 */
	if (!((fb_data->upd_scheme == UPDATE_SCHEME_QUEUE) &&
		upd_data_list)) {
		/*
		 * If we didn't find a collision update ready to go, we
		 * need to get a free buffer and match it to a pending update.
		 */

		/*
		 * Can't proceed if there are no free buffers (and we don't
		 * already have a collision update selected)
		*/
		if (!upd_data_list &&
			list_empty(&fb_data->upd_buf_free_list)) {
			mutex_unlock(&fb_data->queue_mutex);
			return;
		}

		list_for_each_entry_safe(next_desc, temp_desc,
				&fb_data->upd_pending_list, list) {

			dev_dbg(fb_data->dev, "Found a pending update!\n");
			EPDC_VPRINT(fb_data,3, "Found a pending update!\n");

			if (!upd_data_list) {
				if (list_empty(&fb_data->upd_buf_free_list))
					break;
				upd_data_list =
					list_entry(fb_data->upd_buf_free_list.next,
						struct update_data_list, list);
				list_del_init(&upd_data_list->list);
				upd_data_list->update_desc = next_desc;
				list_del_init(&next_desc->list);
				if (fb_data->upd_scheme == UPDATE_SCHEME_QUEUE)
					/* If not merging, we have an update */
					break;
			} else {
				switch (epdc_submit_merge(upd_data_list->update_desc,
						next_desc, fb_data)) {
				case MERGE_OK:
					dev_dbg(fb_data->dev,
						"Update merged [queue]\n");
					EPDC_VPRINT(fb_data,3,
						"Update merged [queue]\n");
					list_del_init(&next_desc->list);
					kfree(next_desc);
					break;
				case MERGE_FAIL:
					dev_dbg(fb_data->dev,
						"Update not merged [queue]\n");
					EPDC_VPRINT(fb_data,3,
						"Update not merged [queue]\n");
					break;
				case MERGE_BLOCK:
					dev_dbg(fb_data->dev,
						"Merge blocked [collision]\n");
					EPDC_VPRINT(fb_data,3,
						"Merge blocked [collision]\n");
					end_merge = true;
					break;
				}

				if (end_merge)
					break;
			}
		}
	}

	/* Is update list empty? */
	if (!upd_data_list) {
		mutex_unlock(&fb_data->queue_mutex);
		return;
	}

	/*
	 * If no processing required, skip update processing
	 * No processing means:
	 *   - FB unrotated
	 *   - FB pixel format = 8-bit grayscale
	 *   - No look-up transformations (inversion, posterization, etc.)
	 *   - No scaling/flip
	 */
	is_transform = ((upd_data_list->update_desc->upd_data.flags &
		(EPDC_FLAG_ENABLE_INVERSION | EPDC_FLAG_USE_DITHERING_Y1 |
		EPDC_FLAG_USE_DITHERING_Y4 | EPDC_FLAG_FORCE_MONOCHROME |
		EPDC_FLAG_USE_CMAP)) && (proc_data->scaling == 0) &&
		(proc_data->hflip == 0) && (proc_data->vflip == 0)) ?
		true : false;

	/*XXX if we use external mode, we should first use pxp
	 * to update upd buffer data to working buffer first.
	 */
	if ((fb_data->epdc_fb_var.rotate == FB_ROTATE_UR) &&
		(fb_data->epdc_fb_var.grayscale == GRAYSCALE_8BIT) &&
		!is_transform && (proc_data->dither_mode == 0) &&
		!fb_data->restrict_width) {

		/* If needed, enable EPDC HW while ePxP is processing */
		if ((fb_data->power_state == POWER_STATE_OFF)
			|| fb_data->powering_down)
			epdc_powerup(fb_data);

		/*
		 * Set update buffer pointer to the start of
		 * the update region in the frame buffer.
		 */
		upd_region = &upd_data_list->update_desc->upd_data.update_region;
		update_addr = fb_data->info.fix.smem_start +
			((upd_region->top * fb_data->info.var.xres_virtual) +
			upd_region->left) * fb_data->info.var.bits_per_pixel/8;
		upd_data_list->update_desc->epdc_stride =
					fb_data->info.var.xres_virtual *
					fb_data->info.var.bits_per_pixel/8;
	} else {
		/* Select from PxP output buffers */
		upd_data_list->phys_addr =
			fb_data->phys_addr_updbuf[fb_data->upd_buffer_num];
		upd_data_list->virt_addr =
			fb_data->virt_addr_updbuf[fb_data->upd_buffer_num];
		fb_data->upd_buffer_num++;
		if (fb_data->upd_buffer_num > fb_data->max_num_buffers-1)
			fb_data->upd_buffer_num = 0;

		/* Release buffer queues */
		mutex_unlock(&fb_data->queue_mutex);


#if 1
		// waiting for tce underrun recovery process .
		if(fb_data->tce_underrun_proc_stat>=TCEUNDERRUN_PROC_STAT_INT &&
			 TCE_UNDERRUN_RECOVER_MARKERNO!=upd_data_list->update_desc->upd_data.update_marker )
		{
			int i,iMaxWaitCnt=(TCE_UNDERRUN_RECOVERY_UPDATE_DELAYMS/10)+60;
			giTCE_State = TCE_STATE_CRITICAL;
			printk(KERN_ERR"%s(%d) skipped when tce underrun recovering \n",__FUNCTION__,__LINE__);
			for(i=0;fb_data->tce_underrun_proc_stat>TCEUNDERRUN_PROC_STAT_INT;i++) {
				
				printk(KERN_ERR".",__FUNCTION__,__LINE__);
				msleep(10);
				if(i>=iMaxWaitCnt) {
					printk(KERN_ERR"tce underrun wait recovering timeout\n");
					break;
				}
			}
			giTCE_State = TCE_STATE_NORMAL;

			if(i<iMaxWaitCnt) {
				printk(KERN_ERR"tce underrun recovering done\n");
			}
		}
#endif

#if (TCE_UNDERRUN_PREVENT_PATCH==2) // .
		// postpone submit update to avoid TCE underrun
		if(	fb_data->cur_mode->vmode->pixclock >= TCE_UNDERRUN_PREVENT_PIXCLK && 
				fb_data->cur_mode->vmode->xres>=TCE_UNDERRUN_PREVENT_X_RES && 
				fb_data->cur_mode->vmode->yres>=TCE_UNDERRUN_PREVENT_Y_RES )
		{
			int i,iMaxWaitCnt=50;
			//int iUPD_RES = fb_data->active_updating_w*fb_data->active_updating_h;
			int iUPD_RES = fb_data->cur_mode->vmode->xres*fb_data->active_updating_h;
			u64 lut_status = fb_data->lut_status;
			int iActLuts;

			for(iActLuts=1,i=0;i<64;i++) {
				if(lut_status&(u64)(1ULL<<i))
					iActLuts++;
			}

			if( iUPD_RES >= ((fb_data->cur_mode->vmode->xres*fb_data->cur_mode->vmode->yres)/iActLuts)
					&& ( (giLast_waveform_mode!=fb_data->wv_modes.mode_a2) &&
						   (giLast_waveform_mode!=fb_data->wv_modes.mode_du) )
#if 0
					&& ( ((fb_data->wv_modes.mode_aa!=fb_data->wv_modes.mode_gl16)&&(giLast_waveform_mode == fb_data->wv_modes.mode_aa)) ||
					((fb_data->wv_modes.mode_aad!=fb_data->wv_modes.mode_gc16)&&(giLast_waveform_mode == fb_data->wv_modes.mode_aad)) )
#endif
				)
			{
				giTCE_State = TCE_STATE_CRITICAL;
				EPDC_VPRINT(fb_data,1,"%s():waiting for lut%d/%d complete to submit update ...\n",
					__FUNCTION__,fb_data->lastest_lut_num,iActLuts);


				for(i=0;i<iMaxWaitCnt;i++) {
					EPDC_VPRINT(fb_data,1,".");
#if 0
					// system will halt .
					if(epdc_is_lut_complete(fb_data->rev,fb_data->lastest_lut_num))
#else
					if( !(fb_data->luts_updating & (1<<fb_data->lastest_lut_num)) )
#endif
					{
						EPDC_VPRINT(fb_data,1,"%s() : lut%d complete !\n",
							__FUNCTION__,fb_data->lastest_lut_num);
						break;
					}
					msleep(10);
				}
				giTCE_State = TCE_STATE_NORMAL;
				if(i>=iMaxWaitCnt) {
					EPDC_VPRINT(fb_data,1,"\n%s(): waiting for lut%d complete %dms timeout !!\n",__FUNCTION__,fb_data->lastest_lut_num,10*i);
				}
				else {
					EPDC_VPRINT(fb_data,1,"\n%s(): lut%d wait %dms done, submitting update !!\n",__FUNCTION__,fb_data->lastest_lut_num,10*i);
				}

			}
		}
#endif//] (TCE_UNDERRUN_PREVENT_PATCH==2)


		/* Perform PXP processing - EPDC power will also be enabled */
		if (epdc_process_update(upd_data_list, fb_data)) {
			dev_err(fb_data->dev, "PXP processing error.\n");
			/* Protect access to buffer queues and to update HW */
			mutex_lock(&fb_data->queue_mutex);
			list_del_init(&upd_data_list->update_desc->list);
			kfree(upd_data_list->update_desc);
			upd_data_list->update_desc = NULL;
			/* Add to free buffer list */
			list_add_tail(&upd_data_list->list,
				&fb_data->upd_buf_free_list);
			/* Release buffer queues */
			mutex_unlock(&fb_data->queue_mutex);
			return;
		}

		/* Protect access to buffer queues and to update HW */
		mutex_lock(&fb_data->queue_mutex);

		update_addr = upd_data_list->phys_addr
				+ upd_data_list->update_desc->epdc_offs;
	}

	/* Check if auto temperature update is needed */
	mxc_epdc_fb_read_temperature(fb_data);

	/* Get rotation-adjusted coordinates */
	adjust_coordinates(fb_data->epdc_fb_var.xres,
		fb_data->epdc_fb_var.yres, fb_data->epdc_fb_var.rotate,
		&upd_data_list->update_desc->upd_data.update_region,
		&adj_update_region);

	/*
	 * Is the working buffer idle?
	 * If the working buffer is busy, we must wait for the resource
	 * to become free. The IST will signal this event.
	 */
	if (fb_data->cur_update != NULL) {
		dev_warn(fb_data->dev, "working buf busy!\n");

		/* Initialize event signalling an update resource is free */
		init_completion(&fb_data->update_res_free);

		fb_data->waiting_for_wb = true;

		/* Leave spinlock while waiting for WB to complete */
		mutex_unlock(&fb_data->queue_mutex);
		wait_for_completion(&fb_data->update_res_free);
		mutex_lock(&fb_data->queue_mutex);
	}

	/*
	 * Dithering Processing (Version 1.0 - for i.MX508 and i.MX6SL)
	 */
	if (upd_data_list->update_desc->upd_data.flags &
	    EPDC_FLAG_USE_DITHERING_Y1) {

		err_dist = kzalloc((fb_data->info.var.xres_virtual + 3) * 3
				* sizeof(int), GFP_KERNEL);

		/* Dithering Y8 -> Y1 */
		do_dithering_processing_Y1_v1_0(
				(uint8_t *)(upd_data_list->virt_addr +
				upd_data_list->update_desc->epdc_offs),
				upd_data_list->phys_addr +
				upd_data_list->update_desc->epdc_offs,
				&adj_update_region,
				(fb_data->rev < 20) ?
				ALIGN(adj_update_region.width, 8) :
				adj_update_region.width,
				err_dist);

		kfree(err_dist);
	} else if (upd_data_list->update_desc->upd_data.flags &
		EPDC_FLAG_USE_DITHERING_Y4) {

		err_dist = kzalloc((fb_data->info.var.xres_virtual + 3) * 3
				* sizeof(int), GFP_KERNEL);

		/* Dithering Y8 -> Y1 */
		do_dithering_processing_Y4_v1_0(
				(uint8_t *)(upd_data_list->virt_addr +
				upd_data_list->update_desc->epdc_offs),
				upd_data_list->phys_addr +
				upd_data_list->update_desc->epdc_offs,
				&adj_update_region,
				(fb_data->rev < 20) ?
				ALIGN(adj_update_region.width, 8) :
				adj_update_region.width,
				err_dist);

		kfree(err_dist);
	}

	/*
	 * If there are no LUTs available,
	 * then we must wait for the resource to become free.
	 * The IST will signal this event.
	 */
	{
		bool luts_available;

		luts_available = fb_data->epdc_wb_mode ? epdc_any_luts_real_available() :
							 epdc_any_luts_available();
		if (!luts_available) {
			dev_warn(fb_data->dev, "no luts available!\n");

			/* Initialize event signalling an update resource is free */
			init_completion(&fb_data->update_res_free);

			fb_data->waiting_for_lut = true;

			/* Leave spinlock while waiting for LUT to free up */
			mutex_unlock(&fb_data->queue_mutex);
			wait_for_completion(&fb_data->update_res_free);
			mutex_lock(&fb_data->queue_mutex);
		}
	}

	ret = epdc_choose_next_lut(fb_data, &upd_data_list->lut_num);
	/*
	 * If LUT15 is in use (for pre-EPDC v2.0 hardware):
	 *   - Wait for LUT15 to complete is if TCE underrun prevent is enabled
	 *   - If we go ahead with update, sync update submission with EOF
	 */
	if (ret && fb_data->tce_prevent && (fb_data->rev < 20)) {
		dev_dbg(fb_data->dev, "Waiting for LUT15\n");

		/* Initialize event signalling that lut15 is free */
		init_completion(&fb_data->lut15_free);

		fb_data->waiting_for_lut15 = true;

		/* Leave spinlock while waiting for LUT to free up */
		mutex_unlock(&fb_data->queue_mutex);
		wait_for_completion(&fb_data->lut15_free);
		mutex_lock(&fb_data->queue_mutex);

		epdc_choose_next_lut(fb_data, &upd_data_list->lut_num);
	} else if (ret && (fb_data->rev < 20)) {
		/* Synchronize update submission time to reduce
		   chances of TCE underrun */
		init_completion(&fb_data->eof_event);

		epdc_eof_intr(true);

		/* Leave spinlock while waiting for EOF event */
		mutex_unlock(&fb_data->queue_mutex);
		ret = wait_for_completion_timeout(&fb_data->eof_event,
			msecs_to_jiffies(1000));
		if (!ret) {
			dev_err(fb_data->dev, "Missed EOF event!\n");
			epdc_eof_intr(false);
		}
		udelay(fb_data->eof_sync_period);
		mutex_lock(&fb_data->queue_mutex);

	}

	/* LUTs are available, so we get one here */
	fb_data->cur_update = upd_data_list;

	/* Reset mask for LUTS that have completed during WB processing */
	fb_data->luts_complete_wb = 0;

	/* If we are just testing for collision, we don't assign a LUT,
	 * so we don't need to update LUT-related resources. */
	if (!(upd_data_list->update_desc->upd_data.flags
		& EPDC_FLAG_TEST_COLLISION)) {
		/* Associate LUT with update marker */
		list_for_each_entry_safe(next_marker, temp_marker,
			&upd_data_list->update_desc->upd_marker_list, upd_list)
			next_marker->lut_num = fb_data->cur_update->lut_num;

		/* Mark LUT with order */
		fb_data->lut_update_order[upd_data_list->lut_num] =
			upd_data_list->update_desc->update_order;

		EPDC_VPRINT(fb_data,1,"%s,%d,%s() : lut%d's order=%d,flags=0x%x\n",
				__FILE__,__LINE__,__FUNCTION__,
				upd_data_list->lut_num,upd_data_list->update_desc->update_order,
				upd_data_list->update_desc->upd_data.flags);

		epdc_lut_complete_intr(fb_data->rev, upd_data_list->lut_num,
					true);
	}

	/* Enable Collision and WB complete IRQs */
	epdc_working_buf_intr(true);

	/* add working buffer update here for external mode */
	if (fb_data->epdc_wb_mode)
		ret = epdc_working_buffer_update(fb_data, upd_data_list,
				&adj_update_region);

	/* Program EPDC update to process buffer */
	if (upd_data_list->update_desc->upd_data.temp != TEMP_USE_AMBIENT) {
		temp_index = mxc_epdc_fb_get_temp_index(fb_data,
			upd_data_list->update_desc->upd_data.temp);
		epdc_set_temp(temp_index);
	} else
		epdc_set_temp(fb_data->temp_index);

	epdc_set_update_addr(update_addr);
	epdc_set_update_coord(adj_update_region.left, adj_update_region.top);
	epdc_set_update_dimensions(adj_update_region.width,
				   adj_update_region.height);
	if (fb_data->rev > 20)
		epdc_set_update_stride(upd_data_list->update_desc->epdc_stride);
	if (fb_data->wv_modes_update &&
		(upd_data_list->update_desc->upd_data.waveform_mode
			== WAVEFORM_MODE_AUTO)) {
		epdc_set_update_waveform(&fb_data->wv_modes);
		fb_data->wv_modes_update = false;
	}

	epdc_submit_update(upd_data_list->lut_num,
			   upd_data_list->update_desc->upd_data.waveform_mode,
			   upd_data_list->update_desc->upd_data.update_mode,
			   (upd_data_list->update_desc->upd_data.flags
				& EPDC_FLAG_TEST_COLLISION) ? true : false,
			   false, 0);

	/* Release buffer queues */
	mutex_unlock(&fb_data->queue_mutex);
}



static int mxc_epdc_fb_send_single_update(struct mxcfb_update_data *upd_data,
				   struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = info ?
		(struct mxc_epdc_fb_data *)info:g_fb_data;
	struct update_data_list *upd_data_list = NULL;
	struct mxcfb_rect *screen_upd_region; /* Region on screen to update */
	int temp_index;
	int ret;
	struct update_desc_list *upd_desc;
	struct update_marker_data *marker_data, *next_marker, *temp_marker;

	EPDC_VPRINT(fb_data,1,"%s():upd_scheme=%d,upd_mode=%s,wf mode=%d,left=%d,top=%d,w=%d,h=%d,flags=0x%x,marker=%d,dither_mode=%d,quant_bit=%d\n",
			__FUNCTION__,fb_data->upd_scheme,\
			upd_data->update_mode==UPDATE_MODE_PARTIAL?"partial":"full",\
			(int)upd_data->waveform_mode,(int)upd_data->update_region.left,\
			(int)upd_data->update_region.top,(int)upd_data->update_region.width,\
			(int)upd_data->update_region.height,upd_data->flags,upd_data->update_marker,\
			upd_data->dither_mode,upd_data->quant_bit);


	/* Has EPDC HW been initialized? */
	if (!fb_data->hw_ready) {
		/* Throw message if we are not mid-initialization */
		if (!fb_data->hw_initializing)
			dev_err(fb_data->dev, "Display HW not properly"
				"initialized. Aborting update.\n");
		return -EPERM;
	}

	/* Check validity of update params */
	if ((upd_data->update_mode != UPDATE_MODE_PARTIAL) &&
		(upd_data->update_mode != UPDATE_MODE_FULL)) {
		dev_err(fb_data->dev,
			"Update mode 0x%x is invalid.  Aborting update.\n",
			upd_data->update_mode);
		return -EINVAL;
	}
	if ((upd_data->waveform_mode > 255) &&
		(upd_data->waveform_mode != WAVEFORM_MODE_AUTO)) {
		dev_err(fb_data->dev,
			"Update waveform mode 0x%x is invalid."
			"  Aborting update.\n",
			upd_data->waveform_mode);
		return -EINVAL;
	}
	if ((upd_data->update_region.left >= fb_data->epdc_fb_var.xres) ||
		(upd_data->update_region.top >= fb_data->epdc_fb_var.yres) ||
		(upd_data->update_region.width > fb_data->epdc_fb_var.xres) ||
		(upd_data->update_region.height > fb_data->epdc_fb_var.yres) ||
		(upd_data->update_region.left + upd_data->update_region.width > fb_data->epdc_fb_var.xres) ||
		(upd_data->update_region.top + upd_data->update_region.height > fb_data->epdc_fb_var.yres)) {
		dev_err(fb_data->dev,
			"Update region is outside bounds of framebuffer."
			"Aborting update.\n");
		return -EINVAL;
	}
	if (upd_data->flags & EPDC_FLAG_USE_ALT_BUFFER) {
		if ((upd_data->update_region.width !=
			upd_data->alt_buffer_data.alt_update_region.width) ||
			(upd_data->update_region.height !=
			upd_data->alt_buffer_data.alt_update_region.height)) {
			dev_err(fb_data->dev,
				"Alternate update region dimensions must "
				"match screen update region dimensions.\n");
			return -EINVAL;
		}

		/* Validate physical address parameter */
		if ((upd_data->alt_buffer_data.phys_addr <
			fb_data->info.fix.smem_start) ||
			(upd_data->alt_buffer_data.phys_addr >
			fb_data->info.fix.smem_start + fb_data->map_size)) {
			dev_err(fb_data->dev,
				"Invalid physical address for alternate "
				"buffer.  Aborting update...\n");
			printk("smem@0x%p,sz=%ld,alt buf phy addr@%p\n",
				fb_data->info.fix.smem_start,
				fb_data->map_size,
				upd_data->alt_buffer_data.phys_addr);

			return -EINVAL;
		}
	}

	if(fb_data->force_invert) {
		upd_data->flags |= EPDC_FLAG_ENABLE_INVERSION;
	}

	mutex_lock(&fb_data->queue_mutex);

	/*
	 * If we are waiting to go into suspend, or the FB is blanked,
	 * we do not accept new updates
	 */
	if ((fb_data->waiting_for_idle) ||
		(fb_data->blank != FB_BLANK_UNBLANK)) {
		dev_dbg(fb_data->dev, "EPDC not active."
			"Update request abort.\n");
		mutex_unlock(&fb_data->queue_mutex);
		return -EPERM;
	}

	if (fb_data->upd_scheme == UPDATE_SCHEME_SNAPSHOT) {
		int count = 0;
		struct update_data_list *plist;

		/*
		 * If next update is a FULL mode update, then we must
		 * ensure that all pending & active updates are complete
		 * before submitting the update.  Otherwise, the FULL
		 * mode update may cause an endless collision loop with
		 * other updates.  Block here until updates are flushed.
		 */
		if (upd_data->update_mode == UPDATE_MODE_FULL) {
			mutex_unlock(&fb_data->queue_mutex);
			mxc_epdc_fb_flush_updates(fb_data);
			mutex_lock(&fb_data->queue_mutex);
		}

		/* Count buffers in free buffer list */
		list_for_each_entry(plist, &fb_data->upd_buf_free_list, list)
			count++;

		/* Use count to determine if we have enough
		 * free buffers to handle this update request */
		if (count + fb_data->max_num_buffers
			<= fb_data->max_num_updates) {
			dev_err(fb_data->dev,
				"No free intermediate buffers available.\n");
			mutex_unlock(&fb_data->queue_mutex);
			return -ENOMEM;
		}

		/* Grab first available buffer and delete from the free list */
		upd_data_list =
		    list_entry(fb_data->upd_buf_free_list.next,
			       struct update_data_list, list);

		list_del_init(&upd_data_list->list);
	}

	/*
	 * Create new update data structure, fill it with new update
	 * data and add it to the list of pending updates
	 */
	upd_desc = kzalloc(sizeof(struct update_desc_list), GFP_KERNEL);
	if (!upd_desc) {
		dev_err(fb_data->dev,
			"Insufficient system memory for update! Aborting.\n");
		if (fb_data->upd_scheme == UPDATE_SCHEME_SNAPSHOT) {
			list_add(&upd_data_list->list,
				&fb_data->upd_buf_free_list);
		}
		mutex_unlock(&fb_data->queue_mutex);
		return -EPERM;
	}
	/* Initialize per-update marker list */
	INIT_LIST_HEAD(&upd_desc->upd_marker_list);
	upd_desc->upd_data = *upd_data;
	upd_desc->update_order = fb_data->order_cnt++;


	NTX_TimeStamp_printf("epdc_new_update_queued",upd_desc->upd_data.update_marker,
		"x=%d,y=%d,%dx%d",upd_data->update_region.left,upd_data->update_region.top,
			upd_data->update_region.width,upd_data->update_region.height);
#if 1 //[ auto select mode 
	{ 


		if(WAVEFORM_MODE_AUTO!=upd_desc->upd_data.waveform_mode) {

#ifdef NTX_WFM_MODE_OPTIMIZED //[
			//if(0==gptHWCFG->m_val.bUIStyle) 
			{

				if(NTX_WFM_MODE_GC16==upd_desc->upd_data.waveform_mode) {
					if(upd_desc->upd_data.update_mode == UPDATE_MODE_FULL) {
						#ifdef NTX_WFM_MODE_OPTIMIZED_REAGL//[
						if(giNTX_waveform_modeA[NTX_WFM_MODE_GLD16]!=giNTX_waveform_modeA[NTX_WFM_MODE_GC16]) {
							// has GLD16 mode .
							DBG_MSG("WF Mode version=0x%02x,chg W.F Mode GC16(%d)->GLD16(%d) @ full\n",
								gbModeVersion,NTX_WFM_MODE_GC16,NTX_WFM_MODE_GLD16);
							upd_desc->upd_data.waveform_mode = NTX_WFM_MODE_GLD16;
						}
						#endif //]NTX_WFM_MODE_OPTIMIZED_REAGL
					}
					else if(upd_desc->upd_data.update_mode == UPDATE_MODE_PARTIAL){
						#ifdef NTX_WFM_MODE_OPTIMIZED_REAGL//[
						if(giNTX_waveform_modeA[NTX_WFM_MODE_GLR16]!=giNTX_waveform_modeA[NTX_WFM_MODE_GC16]) {
							DBG_MSG("WF Mode version=0x%02x,chg W.F Mode GC16(%d)->GLR16(%d) @ partial\n",
								gbModeVersion,NTX_WFM_MODE_GC16,NTX_WFM_MODE_GLR16);
							upd_desc->upd_data.waveform_mode = NTX_WFM_MODE_GLR16;
						}
						else
						#endif //]NTX_WFM_MODE_OPTIMIZED_REAGL
						if (giNTX_waveform_modeA[NTX_WFM_MODE_GL16]!=giNTX_waveform_modeA[NTX_WFM_MODE_GC16]) {
							DBG_MSG("chg W.F Mode GC16(%d)->GL16(%d) @ partial\n",
								NTX_WFM_MODE_GC16,NTX_WFM_MODE_GL16);
							upd_desc->upd_data.waveform_mode = NTX_WFM_MODE_GL16;
						}
					}	
				}

			}
#endif //] NTX_WFM_MODE_OPTIMIZED
#ifdef WFM_ENABLE_AAD //[
			if( NTX_WFM_MODE_GLD16==upd_desc->upd_data.waveform_mode && 
					giNTX_waveform_modeA[NTX_WFM_MODE_GLD16]!=giNTX_waveform_modeA[NTX_WFM_MODE_GC16]) 
			{
				//upd_desc->upd_data.flags |= EPDC_FLAG_USE_AAD;
			}
			/*
			else {
				upd_desc->upd_data.flags &= ~EPDC_FLAG_USE_AAD;
			}
			*/
#endif //]WFM_ENABLE_AAD

			DBG_MSG("ntx wfm mode %d->eink wfm mode %d x\n",
				upd_desc->upd_data.waveform_mode,giNTX_waveform_modeA[upd_desc->upd_data.waveform_mode]);
			upd_desc->upd_data.waveform_mode = giNTX_waveform_modeA[upd_desc->upd_data.waveform_mode];

#if defined( WFM_ENABLE_GCK16 ) && defined (WFM_ENABLE_GLKW16) //[
			
#if 1
			if( ( (giLast_waveform_mode==fb_data->wv_modes.mode_glkw16 && \
					(fb_data->wv_modes.mode_gl16!=fb_data->wv_modes.mode_glkw16)) ||\
				(giLast_waveform_mode==fb_data->wv_modes.mode_gck16 &&\
					(fb_data->wv_modes.mode_gc16!=fb_data->wv_modes.mode_gck16)) ) ) 
			{
				// if last waveform is nightmode , we should wait that update finished .
				EPDC_VPRINT(fb_data,1,"waiting for update marker=%d\n",fb_data->last_nightmode_upd_marker.update_marker);
				mutex_unlock(&fb_data->queue_mutex);
				mxc_epdc_fb_wait_update_complete(&fb_data->last_nightmode_upd_marker,
						&fb_data->info);
				mutex_lock(&fb_data->queue_mutex);

			}
#endif

			if( ( (upd_desc->upd_data.waveform_mode==fb_data->wv_modes.mode_glkw16 && \
					(fb_data->wv_modes.mode_gl16!=fb_data->wv_modes.mode_glkw16)) ||\
				(upd_desc->upd_data.waveform_mode==fb_data->wv_modes.mode_gck16 &&\
					(fb_data->wv_modes.mode_gc16!=fb_data->wv_modes.mode_gck16)) ) ) 
			{
				// store marker to wait night mode refresh . 
				//upd_desc->upd_data.flags |= EPDC_FLAG_ENABLE_INVERSION;
				fb_data->last_nightmode_upd_marker.collision_test = 0;
				fb_data->last_nightmode_upd_marker.update_marker = upd_desc->upd_data.update_marker;

			}
#endif //] WFM_ENABLE_GCK16


		}
		else 
		{
			/*
			if(2==gptHWCFG->m_val.bUIStyle) {
				// android .
				if(giNTX_waveform_modeA[NTX_WFM_MODE_A2]!=giNTX_waveform_modeA[NTX_WFM_MODE_DU]) {
					extern int g_touch_pressed_cnt;
					// waveform has A2 !!
					if( g_touch_pressed_cnt>5 && 
						upd_desc->upd_data.update_mode == UPDATE_MODE_PARTIAL) 
					{
						DBG_MSG("WF Mode version=0x%02x,chg W.F Mode %d->A2(%d) @ partial\n",
								bModeVersion,upd_desc->upd_data.waveform_mode,NTX_WFM_MODE_A2);
						upd_desc->upd_data.waveform_mode = NTX_WFM_MODE_A2;
					}

				}
				
			}
			*/
		}
	}
#endif //]

#ifdef OUTPUT_SNAPSHOT_IMGFILE //[
	{
		char cExtraInfoA[32] = {0,};

		
		if(0!=giFB_snapshot_enable) {
			sprintf(cExtraInfoA,"dither%d-q%d",
				upd_data->dither_mode,upd_data->quant_bit);
		}

		if(2==giFB_snapshot_enable) {
			int iChk;	
			// capture partial update retangle of framebuffer .
			// 

			iChk = fb_capture_ex(gptDC,\
				(int)upd_data->update_region.left,\
				(int)upd_data->update_region.top,\
				(int)upd_data->update_region.width,\
				(int)upd_data->update_region.height,\
				8,EPDFB_R_0,gcFB_snapshot_pathA,\
				(unsigned long) giFB_snapshot_total,\
				upd_desc->upd_data.waveform_mode,\
				(upd_desc->upd_data.update_mode==UPDATE_MODE_FULL)?1:0,
				cExtraInfoA);

			if( giFB_snapshot_repeat>0 && 1==iChk) {
				if(0==--giFB_snapshot_repeat) {
					giFB_snapshot_enable = 0;
				}
			}
		}
		else if(1==giFB_snapshot_enable){
			int iChk;

			iChk = fb_capture_ex(gptDC,0,0,gptDC->dwWidth,gptDC->dwHeight,
				8,EPDFB_R_0,gcFB_snapshot_pathA,\
				(unsigned long) giFB_snapshot_total,\
				upd_desc->upd_data.waveform_mode,\
				(upd_desc->upd_data.update_mode==UPDATE_MODE_FULL)?1:0,
				cExtraInfoA);

			if( giFB_snapshot_repeat>0 && 1==iChk) {
				if(0==--giFB_snapshot_repeat) {
					giFB_snapshot_enable = 0;
				}
			}
		}
	}
#endif //]OUTPUT_SNAPSHOT_IMGFILE


	list_add_tail(&upd_desc->list, &fb_data->upd_pending_list);

	/* If marker specified, associate it with a completion */
	if (upd_data->update_marker != 0) {
		/* Allocate new update marker and set it up */
		marker_data = kzalloc(sizeof(struct update_marker_data),
				GFP_KERNEL);
		if (!marker_data) {
			dev_err(fb_data->dev, "No memory for marker!\n");
			mutex_unlock(&fb_data->queue_mutex);
			return -ENOMEM;
		}
		list_add_tail(&marker_data->upd_list,
			&upd_desc->upd_marker_list);
		marker_data->update_marker = upd_data->update_marker;
		if (upd_desc->upd_data.flags & EPDC_FLAG_TEST_COLLISION)
			marker_data->lut_num = DRY_RUN_NO_LUT;
		else
			marker_data->lut_num = INVALID_LUT;
		init_completion(&marker_data->update_completion);
		/* Add marker to master marker list */
		list_add_tail(&marker_data->full_list,
			&fb_data->full_marker_list);
	}

	if (fb_data->upd_scheme != UPDATE_SCHEME_SNAPSHOT) {
		/* Queued update scheme processing */

		mutex_unlock(&fb_data->queue_mutex);

		/* Signal workqueue to handle new update */
		queue_work(fb_data->epdc_submit_workqueue,
			&fb_data->epdc_submit_work);

		return 0;
	}

	/* Snapshot update scheme processing */

	/* Set descriptor for current update, delete from pending list */
	upd_data_list->update_desc = upd_desc;
	list_del_init(&upd_desc->list);

	mutex_unlock(&fb_data->queue_mutex);

	/*
	 * Hold on to original screen update region, which we
	 * will ultimately use when telling EPDC where to update on panel
	 */
	screen_upd_region = &upd_desc->upd_data.update_region;

	/* Select from PxP output buffers */
	upd_data_list->phys_addr =
		fb_data->phys_addr_updbuf[fb_data->upd_buffer_num];
	upd_data_list->virt_addr =
		fb_data->virt_addr_updbuf[fb_data->upd_buffer_num];
	fb_data->upd_buffer_num++;
	if (fb_data->upd_buffer_num > fb_data->max_num_buffers-1)
		fb_data->upd_buffer_num = 0;

	ret = epdc_process_update(upd_data_list, fb_data);
	if (ret) {
		return ret;
	}


	/* Check if auto temperature update is needed */
	mxc_epdc_fb_read_temperature(fb_data);

	/* Pass selected waveform mode back to user */
	upd_data->waveform_mode = upd_desc->upd_data.waveform_mode;

	/* Get rotation-adjusted coordinates */
	adjust_coordinates(fb_data->epdc_fb_var.xres,
		fb_data->epdc_fb_var.yres, fb_data->epdc_fb_var.rotate,
		&upd_desc->upd_data.update_region, NULL);

	/* Grab lock for queue manipulation and update submission */
	mutex_lock(&fb_data->queue_mutex);

	/*
	 * Is the working buffer idle?
	 * If either the working buffer is busy, or there are no LUTs available,
	 * then we return and let the ISR handle the update later
	 */
	{
		bool luts_available;

		luts_available = fb_data->epdc_wb_mode ? epdc_any_luts_real_available() :
							 epdc_any_luts_available();
		if ((fb_data->cur_update != NULL) || !luts_available) {
			/* Add processed Y buffer to update list */
			list_add_tail(&upd_data_list->list, &fb_data->upd_buf_queue);

			/* Return and allow the update to be submitted by the ISR. */
			mutex_unlock(&fb_data->queue_mutex);
			return 0;
		}
	}

	/* LUTs are available, so we get one here */
	ret = epdc_choose_next_lut(fb_data, &upd_data_list->lut_num);
	if (ret && fb_data->tce_prevent && (fb_data->rev < 20)) {
		dev_dbg(fb_data->dev, "Must wait for LUT15\n");
		/* Add processed Y buffer to update list */
		list_add_tail(&upd_data_list->list, &fb_data->upd_buf_queue);

		/* Return and allow the update to be submitted by the ISR. */
		mutex_unlock(&fb_data->queue_mutex);
		return 0;
	}

	if (!(upd_data_list->update_desc->upd_data.flags
		& EPDC_FLAG_TEST_COLLISION)) {

		/* Save current update */
		fb_data->cur_update = upd_data_list;

		/* Reset mask for LUTS that have completed during WB processing */
		fb_data->luts_complete_wb = 0;

		/* Associate LUT with update marker */
		list_for_each_entry_safe(next_marker, temp_marker,
			&upd_data_list->update_desc->upd_marker_list, upd_list)
			next_marker->lut_num = upd_data_list->lut_num;

		/* Mark LUT as containing new update */
		fb_data->lut_update_order[upd_data_list->lut_num] =
			upd_desc->update_order;

		EPDC_VPRINT(fb_data,1,"%s,%d,%s() : lut%d's order=%d\n",
				__FILE__,__LINE__,__FUNCTION__,
				upd_data_list->lut_num,upd_desc->update_order);

		epdc_lut_complete_intr(fb_data->rev, upd_data_list->lut_num,
					true);
	}

	/* Clear status and Enable LUT complete and WB complete IRQs */
	epdc_working_buf_intr(true);

	/* add working buffer update before display for external mode */
	if (fb_data->epdc_wb_mode)
		ret = epdc_working_buffer_update(fb_data, upd_data_list,
				screen_upd_region);

	/* Program EPDC update to process buffer */
	epdc_set_update_addr(upd_data_list->phys_addr + upd_desc->epdc_offs);
	epdc_set_update_coord(screen_upd_region->left, screen_upd_region->top);
	epdc_set_update_dimensions(screen_upd_region->width,
		screen_upd_region->height);
	if (fb_data->rev > 20)
		epdc_set_update_stride(upd_desc->epdc_stride);
	if (upd_desc->upd_data.temp != TEMP_USE_AMBIENT) {
		temp_index = mxc_epdc_fb_get_temp_index(fb_data,
			upd_desc->upd_data.temp);
		epdc_set_temp(temp_index);
	} else
		epdc_set_temp(fb_data->temp_index);
	if (fb_data->wv_modes_update &&
		(upd_desc->upd_data.waveform_mode == WAVEFORM_MODE_AUTO)) {
		epdc_set_update_waveform(&fb_data->wv_modes);
		fb_data->wv_modes_update = false;
	}

	epdc_submit_update(upd_data_list->lut_num,
			   upd_desc->upd_data.waveform_mode,
			   upd_desc->upd_data.update_mode,
			   (upd_desc->upd_data.flags
				& EPDC_FLAG_TEST_COLLISION) ? true : false,
			   false, 0);

	mutex_unlock(&fb_data->queue_mutex);

	if(EPD_PMIC_EXCEPTION_STATE_REUPDATE_INIT==ntx_epdc_pmic_get_exception_state()) {
#ifdef VDROP_PROC_IN_KERNEL //[
#else //][!VDROP_PROC_IN_KERNEL
		dev_err(fb_data->dev, "send update failed : EPD PMIC get exceptions !!!\n");
		GALLEN_DBGLOCAL_ESC();
#endif // ]VDROP_PROC_IN_KERNEL
		ntx_epdc_pmic_exception_state_clear();
	}


	return 0;
}

static int mxc_epdc_fb_send_update(struct mxcfb_update_data *upd_data,
				   struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = info ?
		(struct mxc_epdc_fb_data *)info:g_fb_data;


	if (!fb_data->restrict_width) {
		/* No width restriction, send entire update region */
		return mxc_epdc_fb_send_single_update(upd_data, info);
	} else {
		int ret;
		__u32 width, left;
		__u32 marker;
		__u32 *region_width, *region_left;
		u32 max_upd_width = EPDC_V2_MAX_UPDATE_WIDTH;

		/* Further restrict max width due to pxp rotation
		  * alignment requirement
		  */
		if (fb_data->epdc_fb_var.rotate != FB_ROTATE_UR)
			max_upd_width -= EPDC_V2_ROTATION_ALIGNMENT;

		/* Select split of width or height based on rotation */
		if ((fb_data->epdc_fb_var.rotate == FB_ROTATE_UR) ||
			(fb_data->epdc_fb_var.rotate == FB_ROTATE_UD)) {
			region_width = &upd_data->update_region.width;
			region_left = &upd_data->update_region.left;
		} else {
			region_width = &upd_data->update_region.height;
			region_left = &upd_data->update_region.top;
		}

		if (*region_width <= max_upd_width)
			return mxc_epdc_fb_send_single_update(upd_data,	info);

		width = *region_width;
		left = *region_left;
		marker = upd_data->update_marker;
		upd_data->update_marker = 0;

		do {
			*region_width = max_upd_width;
			*region_left = left;
			ret = mxc_epdc_fb_send_single_update(upd_data, info);
			if (ret)
				return ret;
			width -= max_upd_width;
			left += max_upd_width;
		} while (width > max_upd_width);

		*region_width = width;
		*region_left = left;
		upd_data->update_marker = marker;
		return mxc_epdc_fb_send_single_update(upd_data, info);
	}
}

static int mxc_epdc_fb_wait_update_complete(struct mxcfb_update_marker_data *marker_data,
						struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = info ?
		(struct mxc_epdc_fb_data *)info:g_fb_data;
	struct update_marker_data *next_marker;
	struct update_marker_data *temp;
	bool marker_found = false;
	int ret = 0;

	/* 0 is an invalid update_marker value */
	if (marker_data->update_marker == 0)
		return -EINVAL;

	/*
	 * Find completion associated with update_marker requested.
	 * Note: If update completed already, marker will have been
	 * cleared, it won't be found, and function will just return.
	 */

	/* Grab queue lock to protect access to marker list */
	mutex_lock(&fb_data->queue_mutex);

	list_for_each_entry_safe(next_marker, temp,
		&fb_data->full_marker_list, full_list) {
		if (next_marker->update_marker == marker_data->update_marker) {
			dev_dbg(fb_data->dev, "Waiting for marker %d\n",
				marker_data->update_marker);
			next_marker->waiting = true;
			marker_found = true;
			break;
		}
	}

	mutex_unlock(&fb_data->queue_mutex);

	/*
	 * If marker not found, it has either been signalled already
	 * or the update request failed.  In either case, just return.
	 */
	if (!marker_found)
		return ret;

	EPDC_VPRINT(fb_data,2,"%s(): waiting marker %d for completion\n",
			__FUNCTION__,next_marker->update_marker);
	ret = wait_for_completion_timeout(&next_marker->update_completion,
						msecs_to_jiffies(5000));
	if (!ret) {
		dev_err(fb_data->dev,
			"Timed out waiting for update completion\n");
		return -ETIMEDOUT;
	}

	marker_data->collision_test = next_marker->collision_test;

	/* Free update marker object */
	kfree(next_marker);

	if(EPD_PMIC_EXCEPTION_STATE_REUPDATE_INIT==ntx_epdc_pmic_get_exception_state()) {
#ifdef VDROP_PROC_IN_KERNEL //[
#else //][!VDROP_PROC_IN_KERNEL
		ret = -EMEDIUMTYPE;
		dev_err(fb_data->dev, "wait compelete failed : EPD PMIC get exceptions !!!\n");
#endif //] VDROP_PROC_IN_KERNEL
		ntx_epdc_pmic_exception_state_clear();
	}


	return ret;
}

static int mxc_epdc_fb_set_pwrdown_delay(u32 pwrdown_delay,
					    struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = info ?
		(struct mxc_epdc_fb_data *)info:g_fb_data;

	dev_info(fb_data->dev,
			"%s(),pwrdwn_delay=%d,info=%p,g_fb_data=%p,info->par=%p \n",__FUNCTION__,pwrdown_delay,info,g_fb_data,info->par);

	fb_data->pwrdown_delay = pwrdown_delay;

	return 0;
}

static int mxc_epdc_get_pwrdown_delay(struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = info ?
		(struct mxc_epdc_fb_data *)info:g_fb_data;

	return fb_data->pwrdown_delay;
}

static int mxc_epdc_fb_ioctl(struct fb_info *info, unsigned int cmd,
			     unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = -EINVAL;
	fake_s1d13522_progress_stop();


	switch (cmd) {
#ifdef MXCFB_WAVEFORM_MODES_NTX //[
	case MXCFB_SET_WAVEFORM_MODES_NTX:
		{
			struct mxcfb_waveform_modes_ntx modes;
			if (!copy_from_user(&modes, argp, sizeof(modes))) {
				mxc_epdc_fb_set_waveform_modes(&modes, info);
				ret = 0;
			}
			break;
		}
	case MXCFB_SET_WAVEFORM_MODES:
		{
			struct mxc_epdc_fb_data *fb_data = info ?
				(struct mxc_epdc_fb_data *)info:g_fb_data;
			struct mxcfb_waveform_modes modes;
			struct mxcfb_waveform_modes modes_ntx;
			if (!copy_from_user(&modes, argp, sizeof(modes))) {
				memcpy(&modes_ntx,&fb_data->wv_modes,sizeof(struct mxcfb_waveform_modes_ntx));
				modes_ntx.mode_init = modes.mode_init;
				modes_ntx.mode_du = modes.mode_du;
				modes_ntx.mode_gc4 = modes.mode_gc4;
				modes_ntx.mode_gc8 = modes.mode_gc8;
				modes_ntx.mode_gc16 = modes.mode_gc16;
				modes_ntx.mode_gc32 = modes.mode_gc32;

				mxc_epdc_fb_set_waveform_modes(&modes_ntx, info);
				ret = 0;
			}
			break;
		}
#else //][!MXCFB_WAVEFORM_MODES_NTX
	case MXCFB_SET_WAVEFORM_MODES:
		{
			struct mxcfb_waveform_modes modes;
			if (!copy_from_user(&modes, argp, sizeof(modes))) {
				mxc_epdc_fb_set_waveform_modes(&modes, info);
				ret = 0;
			}
			break;
		}
#endif //] MXCFB_WAVEFORM_MODES_NTX
	case MXCFB_SET_TEMPERATURE:
		{
			int temperature;
			if (!get_user(temperature, (int32_t __user *) arg))
				ret = mxc_epdc_fb_set_temperature(temperature,
					info);
			break;
		}
	case MXCFB_SET_TEMP_AUTO_UPDATE_PERIOD:
		{
			int period;
			if (!get_user(period, (int32_t __user *) arg))
				ret = mxc_epdc_fb_set_temp_auto_update_period(period,
					info);
			break;
		}
	case MXCFB_SET_AUTO_UPDATE_MODE:
		{
			u32 auto_mode = 0;
			if (!get_user(auto_mode, (__u32 __user *) arg))
				ret = mxc_epdc_fb_set_auto_update(auto_mode,
					info);
			break;
		}
	case MXCFB_SET_UPDATE_SCHEME:
		{
			u32 upd_scheme = 0;
			if (!get_user(upd_scheme, (__u32 __user *) arg))
				ret = mxc_epdc_fb_set_upd_scheme(upd_scheme,
					info);
			break;
		}

	case MXCFB_SEND_UPDATE_V1_NTX:
		{
			struct mxcfb_update_data upd_data;
			struct mxcfb_update_data_v1_ntx upd_data_v1_ntx;

			if (!copy_from_user(&upd_data_v1_ntx, argp,sizeof(upd_data_v1_ntx))) {
				upd_data.alt_buffer_data.phys_addr = upd_data_v1_ntx.alt_buffer_data.phys_addr;
				upd_data.alt_buffer_data.width = upd_data_v1_ntx.alt_buffer_data.width;
				upd_data.alt_buffer_data.height = upd_data_v1_ntx.alt_buffer_data.height;
				upd_data.alt_buffer_data.alt_update_region = upd_data_v1_ntx.alt_buffer_data.alt_update_region;
				upd_data.update_region = upd_data_v1_ntx.update_region;
				upd_data.waveform_mode = upd_data_v1_ntx.waveform_mode;
				upd_data.update_mode = upd_data_v1_ntx.update_mode;
				upd_data.update_marker = upd_data_v1_ntx.update_marker;
				upd_data.temp = upd_data_v1_ntx.temp;
				upd_data.flags = upd_data_v1_ntx.flags;
				upd_data.dither_mode = 0;
				upd_data.quant_bit = 0;
				
				ntx_epdc_upd_prepare(upd_data.update_region.left,
					upd_data.update_region.top,
					upd_data.update_region.width,
					upd_data.update_region.height);
				ret = mxc_epdc_fb_send_update(&upd_data, info);
				upd_data_v1_ntx.alt_buffer_data.phys_addr = upd_data.alt_buffer_data.phys_addr;
				upd_data_v1_ntx.alt_buffer_data.width = upd_data.alt_buffer_data.phys_addr;
				upd_data_v1_ntx.alt_buffer_data.height = upd_data.alt_buffer_data.phys_addr;
				upd_data_v1_ntx.alt_buffer_data.alt_update_region = upd_data.alt_buffer_data.alt_update_region;
				upd_data_v1_ntx.update_region = upd_data.update_region;
				upd_data_v1_ntx.waveform_mode = upd_data.waveform_mode;
				upd_data_v1_ntx.update_mode = upd_data.update_mode;
				upd_data_v1_ntx.update_marker = upd_data.update_marker;
				upd_data_v1_ntx.temp = upd_data.temp;
				upd_data_v1_ntx.flags = upd_data.flags;
			} else {
				ret = -EFAULT;
			}

			break;
		}

	case MXCFB_SEND_UPDATE_V1:
		{
			struct mxcfb_update_data upd_data;
			struct mxcfb_update_data_v1 upd_data_v1;

			if (!copy_from_user(&upd_data_v1, argp,sizeof(upd_data_v1))) {
				upd_data.alt_buffer_data.phys_addr = upd_data_v1.alt_buffer_data.phys_addr;
				upd_data.alt_buffer_data.width = upd_data_v1.alt_buffer_data.width;
				upd_data.alt_buffer_data.height = upd_data_v1.alt_buffer_data.height;
				upd_data.alt_buffer_data.alt_update_region = upd_data_v1.alt_buffer_data.alt_update_region;
				upd_data.update_region = upd_data_v1.update_region;
				upd_data.waveform_mode = upd_data_v1.waveform_mode;
				upd_data.update_mode = upd_data_v1.update_mode;
				upd_data.update_marker = upd_data_v1.update_marker;
				upd_data.temp = upd_data_v1.temp;
				upd_data.flags = upd_data_v1.flags;
				upd_data.dither_mode = 0;
				upd_data.quant_bit = 0;
				ntx_epdc_upd_prepare(upd_data.update_region.left,
					upd_data.update_region.top,
					upd_data.update_region.width,
					upd_data.update_region.height);
				ret = mxc_epdc_fb_send_update(&upd_data, info);
				upd_data_v1.alt_buffer_data.phys_addr = upd_data.alt_buffer_data.phys_addr;
				upd_data_v1.alt_buffer_data.width = upd_data.alt_buffer_data.phys_addr;
				upd_data_v1.alt_buffer_data.height = upd_data.alt_buffer_data.phys_addr;
				upd_data_v1.alt_buffer_data.alt_update_region = upd_data.alt_buffer_data.alt_update_region;
				upd_data_v1.update_region = upd_data.update_region;
				upd_data_v1.waveform_mode = upd_data.waveform_mode;
				upd_data_v1.update_mode = upd_data.update_mode;
				upd_data_v1.update_marker = upd_data.update_marker;
				upd_data_v1.temp = upd_data.temp;
				upd_data_v1.flags = upd_data.flags;
			} else {
				ret = -EFAULT;
			}

			break;
		}

	case MXCFB_SEND_UPDATE_V2:
		{
			struct mxcfb_update_data upd_data;
			if (!copy_from_user(&upd_data, argp,
				sizeof(upd_data))) {
				ntx_epdc_upd_prepare(upd_data.update_region.left,
					upd_data.update_region.top,
					upd_data.update_region.width,
					upd_data.update_region.height);
				ret = mxc_epdc_fb_send_update(&upd_data, info);
				if (ret == 0 && copy_to_user(argp, &upd_data,
					sizeof(upd_data)))
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}

			break;
		}

	case MXCFB_WAIT_FOR_UPDATE_COMPLETE_V2:// mx6sl BSP interface .
	case MXCFB_WAIT_FOR_UPDATE_COMPLETE_V3: // mx7d/mx6ul/mx6ull/mx6sll BSP interface .
		{
			struct mxcfb_update_marker_data upd_marker_data;
			if (!copy_from_user(&upd_marker_data, argp,
				sizeof(upd_marker_data))) {
				ret = mxc_epdc_fb_wait_update_complete(
					&upd_marker_data, info);
				if (copy_to_user(argp, &upd_marker_data,
					sizeof(upd_marker_data)))
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}

			break;
		}
	case MXCFB_WAIT_FOR_UPDATE_COMPLETE_V1: // mx7d/mx6ul/mx6ull/mx6sll BSP interface .
		{
			u32 update_marker;
			struct mxcfb_update_marker_data upd_marker_data;

			if (!get_user(update_marker, (__u32 __user *) arg)) 
			{
				upd_marker_data.update_marker = update_marker;
				upd_marker_data.collision_test = 0;
				ret = mxc_epdc_fb_wait_update_complete(&upd_marker_data, info);
			}
			else {
				dev_err(g_fb_data->dev,"copy marker number failed !\n");
				ret = -EFAULT;
			}
			break;
		}

	case MXCFB_SET_PWRDOWN_DELAY:
		{
			int delay = 0;

			if (!get_user(delay, (__u32 __user *) arg))
				ret =	mxc_epdc_fb_set_pwrdown_delay(delay, info);
			break;
		}

	case MXCFB_GET_PWRDOWN_DELAY:
		{
			int pwrdown_delay = mxc_epdc_get_pwrdown_delay(info);
			if (put_user(pwrdown_delay,
				(int __user *)argp))
				ret = -EFAULT;
			ret = 0;
			break;
		}

	case MXCFB_GET_WORK_BUFFER:
		{
			/* copy the epdc working buffer to the user space */
			struct mxc_epdc_fb_data *fb_data = info ?
				(struct mxc_epdc_fb_data *)info:g_fb_data;
			flush_cache_all();
			outer_flush_range(fb_data->working_buffer_phys,
				fb_data->working_buffer_phys +
				fb_data->working_buffer_size);
			if (copy_to_user((void __user *)arg,
				(const void *) fb_data->working_buffer_virt,
				fb_data->working_buffer_size))
				ret = -EFAULT;
			else
				ret = 0;
			flush_cache_all();
			outer_flush_range(fb_data->working_buffer_phys,
				fb_data->working_buffer_phys +
				fb_data->working_buffer_size);
			break;
		}

	default:
		ret = k_fake_s1d13522_ioctl(cmd,arg);
		break;
	}
	return ret;
}

static void mxc_epdc_fb_update_pages(struct mxc_epdc_fb_data *fb_data,
				     u16 y1, u16 y2)
{
	struct mxcfb_update_data update;

	/* Do partial screen update, Update full horizontal lines */
	update.update_region.left = 0;
	update.update_region.width = fb_data->epdc_fb_var.xres;
	update.update_region.top = y1;
	update.update_region.height = y2 - y1;
	update.waveform_mode = WAVEFORM_MODE_AUTO;
	update.update_mode = UPDATE_MODE_FULL;
	update.update_marker = 0;
	update.temp = TEMP_USE_AMBIENT;
	update.flags = 0;

	mxc_epdc_fb_send_update(&update, &fb_data->info);
}

/* this is called back from the deferred io workqueue */
static void mxc_epdc_fb_deferred_io(struct fb_info *info,
				    struct list_head *pagelist)
{
	struct mxc_epdc_fb_data *fb_data = (struct mxc_epdc_fb_data *)info;
	struct page *page;
	unsigned long beg, end;
	int y1, y2, miny, maxy;

	if (fb_data->auto_mode != AUTO_UPDATE_MODE_AUTOMATIC_MODE)
		return;

	miny = INT_MAX;
	maxy = 0;
	list_for_each_entry(page, pagelist, lru) {
		beg = page->index << PAGE_SHIFT;
		end = beg + PAGE_SIZE - 1;
		y1 = beg / info->fix.line_length;
		y2 = end / info->fix.line_length;
		if (y2 >= fb_data->epdc_fb_var.yres)
			y2 = fb_data->epdc_fb_var.yres - 1;
		if (miny > y1)
			miny = y1;
		if (maxy < y2)
			maxy = y2;
	}

	mxc_epdc_fb_update_pages(fb_data, miny, maxy);
}

void mxc_epdc_fb_flush_updates(struct mxc_epdc_fb_data *fb_data)
{
	int ret;

	if (fb_data->in_init)
		return;

	/* Grab queue lock to prevent any new updates from being submitted */
	mutex_lock(&fb_data->queue_mutex);

	/*
	 * 3 places to check for updates that are active or pending:
	 *   1) Updates in the pending list
	 *   2) Update buffers in use (e.g., PxP processing)
	 *   3) Active updates to panel - We can key off of EPDC
	 *      power state to know if we have active updates.
	 */
	if (!list_empty(&fb_data->upd_pending_list) ||
		!is_free_list_full(fb_data) ||
		(fb_data->updates_active == true)) {
		/* Initialize event signalling updates are done */
		init_completion(&fb_data->updates_done);
		fb_data->waiting_for_idle = true;

		mutex_unlock(&fb_data->queue_mutex);
		/* Wait for any currently active updates to complete */
		ret = wait_for_completion_timeout(&fb_data->updates_done,
						msecs_to_jiffies(8000));
		if (!ret)
			dev_err(fb_data->dev,
				"Flush updates timeout! ret = 0x%x\n", ret);

		mutex_lock(&fb_data->queue_mutex);
		fb_data->waiting_for_idle = false;
	}

	mutex_unlock(&fb_data->queue_mutex);
}

static int mxc_epdc_fb_blank(int blank, struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = (struct mxc_epdc_fb_data *)info;
	int ret;

	dev_info(fb_data->dev, "blank = %d\n", blank);

	if (fb_data->blank == blank)
		return 0;

	fb_data->blank = blank;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		mxc_epdc_fb_flush_updates(fb_data);
		/* Wait for powerdown */
		mutex_lock(&fb_data->power_mutex);
		if ((fb_data->power_state == POWER_STATE_ON) &&
			(fb_data->pwrdown_delay == FB_POWERDOWN_DISABLE)) {

			/* Powerdown disabled, so we disable EPDC manually */
			int count = 0;
			int sleep_ms = 10;

			mutex_unlock(&fb_data->power_mutex);

			/* If any active updates, wait for them to complete */
			while (fb_data->updates_active) {
				/* Timeout after 1 sec */
				if ((count * sleep_ms) > 1000)
					break;
				msleep(sleep_ms);
				count++;
			}

			fb_data->powering_down = true;
			if(fb_data->vcom_off_with_data) {
				if (regulator_is_enabled(fb_data->vcom_regulator)) {
					udelay(VCOM_OFF_DELAY_US);
					regulator_disable(fb_data->vcom_regulator);
				}
			}
			epdc_powerdown(fb_data);
		} else if (fb_data->power_state != POWER_STATE_OFF) {
			fb_data->wait_for_powerdown = true;
			init_completion(&fb_data->powerdown_compl);
			mutex_unlock(&fb_data->power_mutex);
			ret = wait_for_completion_timeout(&fb_data->powerdown_compl,
				msecs_to_jiffies(5000));
			if (!ret) {
				dev_err(fb_data->dev,
					"No powerdown received!\n");
				return -ETIMEDOUT;
			}
		} else
			mutex_unlock(&fb_data->power_mutex);
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_NORMAL:
		mxc_epdc_fb_flush_updates(fb_data);
		break;
	}
	return 0;
}

static int mxc_epdc_fb_pan_display(struct fb_var_screeninfo *var,
				   struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = (struct mxc_epdc_fb_data *)info;
	u_int y_bottom;

	dev_dbg(info->device, "%s: var->yoffset %d, info->var.yoffset %d\n",
		 __func__, var->yoffset, info->var.yoffset);
	/* check if var is valid; also, xpan is not supported */
	if (!var || (var->xoffset != info->var.xoffset) ||
	    (var->yoffset + var->yres > var->yres_virtual)) {
		dev_dbg(info->device, "x panning not supported\n");
		return -EINVAL;
	}

	if ((fb_data->epdc_fb_var.xoffset == var->xoffset) &&
		(fb_data->epdc_fb_var.yoffset == var->yoffset))
		return 0;	/* No change, do nothing */

	y_bottom = var->yoffset;

	if (!(var->vmode & FB_VMODE_YWRAP))
		y_bottom += var->yres;

	if (y_bottom > info->var.yres_virtual)
		return -EINVAL;

	mutex_lock(&fb_data->queue_mutex);

	fb_data->fb_offset = (var->yoffset * var->xres_virtual + var->xoffset)
		* (var->bits_per_pixel) / 8;

	fb_data->epdc_fb_var.xoffset = var->xoffset;
	fb_data->epdc_fb_var.yoffset = var->yoffset;

	k_fake_s1d13522_pan_display(var->xoffset,var->yoffset);
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;

	mutex_unlock(&fb_data->queue_mutex);

	return 0;
}

static struct fb_ops mxc_epdc_fb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = mxc_epdc_fb_check_var,
	.fb_set_par = mxc_epdc_fb_set_par,
	.fb_setcmap = mxc_epdc_fb_setcmap,
	.fb_setcolreg = mxc_epdc_fb_setcolreg,
	.fb_pan_display = mxc_epdc_fb_pan_display,
	.fb_ioctl = mxc_epdc_fb_ioctl,
	.fb_mmap = mxc_epdc_fb_mmap,
	.fb_blank = mxc_epdc_fb_blank,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
};

static struct fb_deferred_io mxc_epdc_fb_defio = {
	.delay = HZ,
	.deferred_io = mxc_epdc_fb_deferred_io,
};


static void epdc_reupdate_work_func(struct work_struct *work)
{
	struct mxc_epdc_fb_data *fb_data =
		container_of(work, struct mxc_epdc_fb_data,
			epdc_reupdate_work.work);
	struct mxcfb_update_data l_upd_data = {0,};
	struct mxcfb_update_marker_data l_mxc_upd_marker_data = {0,};

	int iChk;
	unsigned long update_marker_num = 0;
	__u32 w,h,x,y;

	int iTCE_Underrun_Proc = 
		(fb_data->tce_underrun_proc_stat>=TCEUNDERRUN_PROC_STAT_PS1)?1:0;


	l_upd_data.update_mode = UPDATE_MODE_FULL;
	l_upd_data.waveform_mode = fb_data->wv_modes.mode_gc16;
	l_upd_data.temp = TEMP_USE_AMBIENT;
	l_upd_data.flags = 0;
	l_upd_data.update_marker = update_marker_num;


	if(iTCE_Underrun_Proc) {
		update_marker_num = TCE_UNDERRUN_RECOVER_MARKERNO;
		x = 0;
		y = 0;
		w = fb_data->epdc_fb_var.xres;
		h = fb_data->epdc_fb_var.yres;
		printk(KERN_ERR"%s(),TCE underrun,x=%d,y=%d,w=%d,h=%d\n",__FUNCTION__,(int)x,(int)y,(int)w,(int)h);
	}
	else {
		x = fb_data->latest_update_region.left;
		y = fb_data->latest_update_region.top;
		w = fb_data->latest_update_region.width;
		h = fb_data->latest_update_region.height;
		printk(KERN_ERR"%s(),x=%d,y=%d,w=%d,h=%d\n",__FUNCTION__,(int)x,(int)y,(int)w,(int)h);
	}

	l_upd_data.update_region.width = w;
	l_upd_data.update_region.height = h;
	l_upd_data.update_region.left = x;
	l_upd_data.update_region.top = y;

	iChk = mxc_epdc_fb_send_update(&l_upd_data, &fb_data->info);

	l_mxc_upd_marker_data.collision_test = 0;
	l_mxc_upd_marker_data.update_marker = update_marker_num;
	iChk = mxc_epdc_fb_wait_update_complete(&l_mxc_upd_marker_data,&fb_data->info);

	if(iTCE_Underrun_Proc) {
		fb_data->tce_underrun_proc_stat = TCEUNDERRUN_PROC_STAT_OK;
	}
	printk(KERN_ERR"%s(),x=%d,y=%d,w=%d,h=%d reupdate done \n",__FUNCTION__,(int)x,(int)y,(int)w,(int)h);
}

static void epdc_done_work_func(struct work_struct *work)
{
	struct mxc_epdc_fb_data *fb_data =
		container_of(work, struct mxc_epdc_fb_data,
			epdc_done_work.work);
	epdc_powerdown(fb_data);
}

static bool is_free_list_full(struct mxc_epdc_fb_data *fb_data)
{
	int count = 0;
	struct update_data_list *plist;

	/* Count buffers in free buffer list */
	list_for_each_entry(plist, &fb_data->upd_buf_free_list, list)
		count++;

	/* Check to see if all buffers are in this list */
	if (count == fb_data->max_num_updates)
		return true;
	else
		return false;
}

static irqreturn_t mxc_epdc_irq_handler(int irq, void *dev_id)
{
	struct mxc_epdc_fb_data *fb_data = dev_id;
	u32 ints_fired, luts1_ints_fired, luts2_ints_fired;
	u32 epdc_irq_stat;

	/*
	 * If we just completed one-time panel init, bypass
	 * queue handling, clear interrupt and return
	 */
	if (fb_data->in_init) {
		if (epdc_is_working_buffer_complete()) {
			epdc_working_buf_intr(false);
			epdc_clear_working_buf_irq();
			dev_info(fb_data->dev, "Cleared WB for init update\n");
		}

		if (epdc_is_lut_complete(fb_data->rev, 0)) {
			epdc_lut_complete_intr(fb_data->rev, 0, false);
			epdc_clear_lut_complete_irq(fb_data->rev, 0);
			fb_data->in_init = false;
			dev_info(fb_data->dev, "Cleared LUT complete for init update\n");
		}

		return IRQ_HANDLED;
	}

	ints_fired = __raw_readl(EPDC_IRQ_MASK) & __raw_readl(EPDC_IRQ);
	if (fb_data->rev < 20) {
		luts1_ints_fired = 0;
		luts2_ints_fired = 0;
	} else {
		luts1_ints_fired = __raw_readl(EPDC_IRQ_MASK1) & __raw_readl(EPDC_IRQ1);
		luts2_ints_fired = __raw_readl(EPDC_IRQ_MASK2) & __raw_readl(EPDC_IRQ2);
	}

	if (!(ints_fired || luts1_ints_fired || luts2_ints_fired))
		return IRQ_HANDLED;

	epdc_irq_stat = __raw_readl(EPDC_IRQ);
	if (epdc_irq_stat & EPDC_IRQ_TCE_UNDERRUN_IRQ) {
		dev_err(fb_data->dev,
			"TCE underrun! Will continue to update panel,irq_stat=0x%x\n",epdc_irq_stat);
		if(TCEUNDERRUN_PROC_STAT_OK!=fb_data->tce_underrun_proc_stat)
		{
			dev_err(fb_data->dev,
				"TCE underrun is comming when procedure in progress .\n");
		}
#if (TCE_UNDERRUN_RECOVERY==1) //[
		// nop . 
		fb_data->tce_underrun_proc_stat = TCEUNDERRUN_PROC_STAT_INT;
#elif (TCE_UNDERRUN_RECOVERY==2) //[
		__raw_writel(EPDC_IRQ_TCE_UNDERRUN_IRQ, EPDC_IRQ_CLEAR);

		if(TCEUNDERRUN_PROC_STAT_OK!=fb_data->tce_underrun_proc_stat) {
			cancel_delayed_work_sync(&fb_data->epdc_reupdate_work);
		}
		else {
			fb_data->tce_underrun_proc_stat = TCEUNDERRUN_PROC_STAT_PS1;
		}
		schedule_delayed_work(&fb_data->epdc_reupdate_work,msecs_to_jiffies(TCE_UNDERRUN_RECOVERY_UPDATE_DELAYMS));
#else //][! (TCE_UNDERRUN_RECOVERY==1)
		/* Clear TCE underrun IRQ */
		__raw_writel(EPDC_IRQ_TCE_UNDERRUN_IRQ, EPDC_IRQ_CLEAR);
#endif //] (TCE_UNDERRUN_RECOVERY==1)
	}

	/* Check if we are waiting on EOF to sync a new update submission */
	if (epdc_signal_eof()) {
		epdc_eof_intr(false);
		epdc_clear_eof_irq();

#if (TCE_UNDERRUN_RECOVERY==1) //[
		if(TCEUNDERRUN_PROC_STAT_INT==fb_data->tce_underrun_proc_stat) {
			dev_err(fb_data->dev,"TCE underrun-> frame end \n");
			fb_data->tce_underrun_proc_stat = TCEUNDERRUN_PROC_STAT_PS1;
		}
#endif //]

		complete(&fb_data->eof_event);
	}

	/*
	 * Workaround for EPDC v2.0/v2.1 errata: Must read collision status
	 * before clearing IRQ, or else collision status for bits 16:63
	 * will be automatically cleared.  So we read it here, and there is
	 * no conflict with using it in epdc_intr_work_func since the
	 * working buffer processing flow is strictly sequential (i.e.,
	 * only one WB processing done at a time, so the data grabbed
	 * here should be up-to-date and accurate when the WB processing
	 * completes.  Also, note that there is no impact to other versions
	 * of EPDC by reading LUT status here.
	 */
	if (fb_data->cur_update != NULL)
		fb_data->epdc_colliding_luts = epdc_get_colliding_luts(fb_data->rev);

	/* Clear the interrupt mask for any interrupts signalled */
	__raw_writel(ints_fired, EPDC_IRQ_MASK_CLEAR);
	__raw_writel(luts1_ints_fired, EPDC_IRQ_MASK1_CLEAR);
	__raw_writel(luts2_ints_fired, EPDC_IRQ_MASK2_CLEAR);

	dev_dbg(fb_data->dev, "EPDC interrupts fired = 0x%x, "
		"LUTS1 fired = 0x%x, LUTS2 fired = 0x%x\n",
		ints_fired, luts1_ints_fired, luts2_ints_fired);

	queue_work(fb_data->epdc_intr_workqueue,
		&fb_data->epdc_intr_work);

	return IRQ_HANDLED;
}

static void epdc_recovery(struct mxc_epdc_fb_data *fb_data)
{
	struct mxcfb_update_data l_upd_data;
	int iQueueLockOrgState;

#ifdef TCE_UNDERRUN_PREVENT_WORKFUNC//[
	if(g_fb_data->tce_safe_work_running) {
		g_fb_data->tce_safe_work_cancel = 1;
	}
#endif //]TCE_UNDERRUN_PREVENT_WORKFUNC

	msleep(50);
	printk(KERN_WARNING"epdc reinit ...\n");

	iQueueLockOrgState = mutex_is_locked(&fb_data->queue_mutex); 
	if(!iQueueLockOrgState) {
		mutex_lock(&fb_data->queue_mutex);
	}

	//fb_data->in_init = true;
	//epdc_init_sequence(fb_data);
	epdc_init_settings_ex(fb_data,EPDC_INIT_SETTING_PROC_TCE_RECOVERY);
	//fb_data->tce_underrun_proc_stat=TCEUNDERRUN_PROC_STAT_PSF;
	fb_data->tce_underrun_proc_stat=TCEUNDERRUN_PROC_STAT_OK;
	fb_data->updates_active = false;

	msleep(100);

	//printk("epdc powerdown ...\n");
	//fb_data->powering_down = true;
	//epdc_powerdown(fb_data);
	

	fb_data->luts_updating &= ~(u64)(1ULL<<fb_data->lastest_lut_num);

	printk(KERN_WARNING"resending last rect update ...\n");
	l_upd_data.update_mode = UPDATE_MODE_FULL;
	l_upd_data.waveform_mode = fb_data->wv_modes.mode_gc16;
	l_upd_data.temp = TEMP_USE_AMBIENT;
	l_upd_data.flags = 0;
	l_upd_data.update_marker = 0;
	l_upd_data.dither_mode = 0;
	l_upd_data.quant_bit = 0;

	if(FB_ROTATE_UD==fb_data->epdc_fb_var.rotate || FB_ROTATE_UR==fb_data->epdc_fb_var.rotate) {
		l_upd_data.update_region.width = fb_data->latest_update_region.width;
		l_upd_data.update_region.height = fb_data->latest_update_region.height;
		l_upd_data.update_region.left = fb_data->latest_update_region.left;
		l_upd_data.update_region.top = fb_data->latest_update_region.top;
	}
	else {
		l_upd_data.update_region.height = fb_data->latest_update_region.width;
		l_upd_data.update_region.width = fb_data->latest_update_region.height;
		l_upd_data.update_region.left = fb_data->latest_update_region.left;
		l_upd_data.update_region.top = fb_data->latest_update_region.top;
	}


	mutex_unlock(&fb_data->queue_mutex);
	mxc_epdc_fb_send_single_update(&l_upd_data,&fb_data->info);
	if(iQueueLockOrgState) {
		mutex_lock(&fb_data->queue_mutex);
	}
	printk(KERN_WARNING"epdc recovery end\n");
}

static void epdc_intr_work_func(struct work_struct *work)
{
	struct mxc_epdc_fb_data *fb_data =
		container_of(work, struct mxc_epdc_fb_data, epdc_intr_work);
	struct update_data_list *collision_update;
	struct mxcfb_rect *next_upd_region;
	struct update_marker_data *next_marker;
	struct update_marker_data *temp;
	int temp_index;
	u64 temp_mask;
	u32 lut;
	bool ignore_collision = false;
	int i;
	bool wb_lut_done = false;
	bool free_update = true;
	int next_lut, epdc_next_lut_15;
	u32 epdc_luts_active, epdc_wb_busy, epdc_luts_avail, epdc_lut_cancelled;
	u32 epdc_collision;
	u64 epdc_irq_stat;
	bool epdc_waiting_on_wb;
	u32 coll_coord, coll_size;
	struct mxcfb_rect coll_region;
	int epd_power_down = 0;


	GALLEN_DBGLOCAL_BEGIN_EX(64);

	fb_data->lut_status = epdc_luts_status(fb_data->rev);

	/* Protect access to buffer queues and to update HW */
	mutex_lock(&fb_data->queue_mutex);

	/* Capture EPDC status one time to limit exposure to race conditions */
	epdc_luts_active = epdc_any_luts_active(fb_data->rev);
	epdc_wb_busy = epdc_is_working_buffer_busy();

	if (fb_data->epdc_wb_mode) {
		//epdc_lut_cancelled = fb_data->pixel_nums == 0 ? true : false;
		epdc_lut_cancelled = 0;
	}
	else
		epdc_lut_cancelled = epdc_is_lut_cancelled();

	if (fb_data->epdc_wb_mode)
		epdc_luts_avail = epdc_any_luts_real_available();
	else
		epdc_luts_avail = epdc_any_luts_available();

	if (fb_data->epdc_wb_mode)
		epdc_collision = fb_data->col_info.pixel_cnt ? 1 : 0;
	else
		epdc_collision = epdc_is_collision();
	if(epdc_collision) {
		NTX_TimeStamp_In("epdc_collision occured !",epdc_collision);
	}

	if (fb_data->rev < 20)
		epdc_irq_stat = __raw_readl(EPDC_IRQ);
	else
		epdc_irq_stat = (u64)__raw_readl(EPDC_IRQ1) |
			((u64)__raw_readl(EPDC_IRQ2) << 32);
	epdc_waiting_on_wb = (fb_data->cur_update != NULL) ? true : false;


	//if(fb_data->tce_underrun_proc_stat) 
	{
		EPDC_VPRINT(fb_data,10,"irq_stat=0x%llx\n",epdc_irq_stat);
		EPDC_VPRINT(fb_data,10,"wb_busy=%x\n",epdc_wb_busy);
		EPDC_VPRINT(fb_data,2,"luts_status=0x%x\n",epdc_luts_status(fb_data->rev));
		EPDC_VPRINT(fb_data,2,"luts_active=%x\n",epdc_luts_active);
		EPDC_VPRINT(fb_data,10,"luts_avail=%x\n",epdc_luts_avail);
		EPDC_VPRINT(fb_data,10,"luts_canelled=%x\n",epdc_lut_cancelled);
		EPDC_VPRINT(fb_data,10,"epdc_collision=%x\n",epdc_collision);
		EPDC_VPRINT(fb_data,2,"no pending=%d\n",list_empty(&fb_data->upd_pending_list));
		EPDC_VPRINT(fb_data,2,"free list full=%d\n",is_free_list_full(fb_data));
		EPDC_VPRINT(fb_data,2,"waiting on wb=%d\n",epdc_waiting_on_wb);
	}

#if (TCE_UNDERRUN_RECOVERY==1) //[
	if(TCEUNDERRUN_PROC_STAT_PS1==fb_data->tce_underrun_proc_stat) {
		epdc_recovery(fb_data);
		/* Clear TCE underrun IRQ */
		__raw_writel(EPDC_IRQ_TCE_UNDERRUN_IRQ, EPDC_IRQ_MASK);
		__raw_writel(EPDC_IRQ_TCE_UNDERRUN_IRQ, EPDC_IRQ_CLEAR);
		mutex_unlock(&fb_data->queue_mutex);
		return ;
	}
#endif ///]


	/* Free any LUTs that have completed */
	for (i = 0; i < fb_data->num_luts; i++) {
		if ((epdc_irq_stat & (1ULL << i)) == 0)
			continue;

		NTX_TimeStamp_In("epdc_lut_completed",i);
		EPDC_VPRINT(fb_data,4, "LUT %d completed\n", i);

		/* Disable IRQ for completed LUT */
		epdc_lut_complete_intr(fb_data->rev, i, false);

		/*
		 * Go through all updates in the collision list and
		 * unmask any updates that were colliding with
		 * the completed LUT.
		 */
		list_for_each_entry(collision_update,
				    &fb_data->upd_buf_collision_list, list) {
			collision_update->collision_mask =
			    collision_update->collision_mask & ~(1ULL << i);
		}

		epdc_clear_lut_complete_irq(fb_data->rev, i);

		fb_data->luts_updating &= ~(u64)(1ULL<<i) ;
#if 1
		if(fb_data->lut_rect[i].width>fb_data->active_updating_w) {
			dev_warn(fb_data->dev,"[WARN] cur update w(%d)>updating w(%d)\n",
					fb_data->lut_rect[i].width,fb_data->active_updating_w);
			fb_data->active_updating_w = 0;
		}
		else {
			fb_data->active_updating_w -= fb_data->lut_rect[i].width;
		}
		if(fb_data->lut_rect[i].height>fb_data->active_updating_h) {
			dev_warn(fb_data->dev,"[WARN] cur update h(%d)>updating h(%d)\n",
					fb_data->lut_rect[i].height,fb_data->active_updating_h);
			fb_data->active_updating_h = 0;
		}
		else {
			fb_data->active_updating_h -= fb_data->lut_rect[i].height;
		}
#endif

		fb_data->luts_complete_wb |= 1ULL << i;
		if (i != 0)
			fb_data->luts_complete |= 1ULL << i;

		fb_data->lut_update_order[i] = 0;

		/* Signal completion if submit workqueue needs a LUT */
		if (fb_data->waiting_for_lut) {
			complete(&fb_data->update_res_free);
			fb_data->waiting_for_lut = false;
		}

		/* Signal completion if LUT15 free and is needed */
		if (fb_data->waiting_for_lut15 && (i == 15)) {
			GALLEN_DBGLOCAL_RUNLOG(1);
			complete(&fb_data->lut15_free);
			fb_data->waiting_for_lut15 = false;
		}

		/* Detect race condition where WB and its LUT complete
		   (i.e. full update completes) in one swoop */
		if (epdc_waiting_on_wb &&
			(i == fb_data->cur_update->lut_num)) {

			GALLEN_DBGLOCAL_RUNLOG(2);
			wb_lut_done = true;
		}

		/* Signal completion if anyone waiting on this LUT */
		if (!wb_lut_done)
			GALLEN_DBGLOCAL_RUNLOG(3);
			list_for_each_entry_safe(next_marker, temp,
				&fb_data->full_marker_list,
				full_list) {
				if (next_marker->lut_num != i) {
					continue;
				}

				/* Found marker to signal - remove from list */
				list_del_init(&next_marker->full_list);

				/* Signal completion of update */
				EPDC_VPRINT(fb_data,3,"%s() : Signaling marker %d\n",
					__FUNCTION__,next_marker->update_marker);

				if (next_marker->waiting)
					complete(&next_marker->update_completion);
				else
					kfree(next_marker);
			}
	}




	/* Check to see if all updates have completed */
	if (list_empty(&fb_data->upd_pending_list) &&
		is_free_list_full(fb_data) &&
		!epdc_waiting_on_wb &&
		!epdc_luts_active) {

		GALLEN_DBGLOCAL_RUNLOG(4);
		fb_data->updates_active = false;

		fb_data->active_updating_h=0;
		fb_data->active_updating_w=0;


		/* Reset counter to reduce chance of overflow */
		fb_data->order_cnt = 0;

		if (fb_data->waiting_for_idle) {
			GALLEN_DBGLOCAL_RUNLOG(6);
			complete(&fb_data->updates_done);
		}
#if 0 //[  
		printk("%s() : cur_update=%p\n",__func__,fb_data->cur_update);
		if(fb_data->cur_update) {
			printk("%s() : update_desc=%p\n",__func__,fb_data->cur_update->update_desc);
		}
#else //][
		if( ( (giLast_waveform_mode==fb_data->wv_modes.mode_glkw16 && \
				(fb_data->wv_modes.mode_gl16!=fb_data->wv_modes.mode_glkw16)) ||\
			(giLast_waveform_mode==fb_data->wv_modes.mode_gck16 &&\
				(fb_data->wv_modes.mode_gc16!=fb_data->wv_modes.mode_gck16)) ) ) 
		{
			epd_power_down = 2;
		}
		else
#endif //]
		{
			epd_power_down = 1;
		}

	}

	

	if(epd_power_down) {
		if (fb_data->pwrdown_delay != FB_POWERDOWN_DISABLE) {
			/*
			 * Set variable to prevent overlapping
			 * enable/disable requests
			 */
			GALLEN_DBGLOCAL_RUNLOG(5);
			fb_data->powering_down = true;


			/* Schedule task to disable EPDC HW until next update */
			if(fb_data->vcom_off_with_data) {
				if (regulator_is_enabled(fb_data->vcom_regulator)) {
					udelay(VCOM_OFF_DELAY_US);
					regulator_disable(fb_data->vcom_regulator);
				}
			
			}

			if(2==epd_power_down) {
				schedule_delayed_work(&fb_data->epdc_done_work,msecs_to_jiffies(NIGHTMODE_TVG_MS));
			}
			else {
				schedule_delayed_work(&fb_data->epdc_done_work,
					msecs_to_jiffies(fb_data->pwrdown_delay));
			}

		}
	}

	/* Is Working Buffer busy? */
	if (epdc_wb_busy) {
		GALLEN_DBGLOCAL_RUNLOG(7);
		/* Can't submit another update until WB is done */
		mutex_unlock(&fb_data->queue_mutex);
		GALLEN_DBGLOCAL_ESC();
		return;
	}

	/*
	 * Were we waiting on working buffer?
	 * If so, update queues and check for collisions
	 */
	if (epdc_waiting_on_wb) {
		dev_dbg(fb_data->dev, "\nWorking buffer completed\n");
		EPDC_VPRINT(fb_data,3, "\nWorking buffer completed\n");

		/* Signal completion if submit workqueue was waiting on WB */
		if (fb_data->waiting_for_wb) {
			complete(&fb_data->update_res_free);
			fb_data->waiting_for_wb = false;
		}

		if (fb_data->cur_update->update_desc->upd_data.flags
			& EPDC_FLAG_TEST_COLLISION) {
			GALLEN_DBGLOCAL_RUNLOG(9);
			/* This was a dry run to test for collision */

			/* Signal marker */
			list_for_each_entry_safe(next_marker, temp,
				&fb_data->full_marker_list,
				full_list) {
				if (next_marker->lut_num != DRY_RUN_NO_LUT)
					continue;

				if (epdc_collision)
					next_marker->collision_test = true;
				else
					next_marker->collision_test = false;

				dev_dbg(fb_data->dev,
					"In IRQ, collision_test = %d\n",
					next_marker->collision_test);

				/* Found marker to signal - remove from list */
				list_del_init(&next_marker->full_list);

				/* Signal completion of update */
				EPDC_VPRINT(fb_data,3,"Signaling marker "
					"for dry-run - %d\n",
					next_marker->update_marker);
				complete(&next_marker->update_completion);
			}
			memset(&fb_data->col_info, 0x0, sizeof(struct pxp_collision_info));
		} else if (epdc_lut_cancelled && !epdc_collision) {
			/*
			* Note: The update may be cancelled (void) if all
			* pixels collided. In that case we handle it as a
			* collision, not a cancel.
			*/

			/* Clear LUT status (might be set if no AUTOWV used) */

			/*
			 * Disable and clear IRQ for the LUT used.
			 * Even though LUT is cancelled in HW, the LUT
			 * complete bit may be set if AUTOWV not used.
			 */
			GALLEN_DBGLOCAL_RUNLOG(10);
			epdc_lut_complete_intr(fb_data->rev,
					fb_data->cur_update->lut_num, false);
			epdc_clear_lut_complete_irq(fb_data->rev,
					fb_data->cur_update->lut_num);

			fb_data->lut_update_order[fb_data->cur_update->lut_num] = 0;

			/* Signal completion if submit workqueue needs a LUT */
			if (fb_data->waiting_for_lut) {
				GALLEN_DBGLOCAL_RUNLOG(11);
				complete(&fb_data->update_res_free);
				fb_data->waiting_for_lut = false;
			}

			list_for_each_entry_safe(next_marker, temp,
				&fb_data->cur_update->update_desc->upd_marker_list,
				upd_list) {

				/* Del from per-update & full list */
				list_del_init(&next_marker->upd_list);
				list_del_init(&next_marker->full_list);

				/* Signal completion of update */
				EPDC_VPRINT(fb_data,3,
					"Signaling marker (cancelled) %d\n",
					next_marker->update_marker);
				if (next_marker->waiting)
					complete(&next_marker->update_completion);
				else
					kfree(next_marker);
			}
		} else if (epdc_collision) {
			/* Real update (no dry-run), collision occurred */

			GALLEN_DBGLOCAL_RUNLOG(12);
			/* Check list of colliding LUTs, and add to our collision mask */
			if (fb_data->epdc_wb_mode)
				fb_data->epdc_colliding_luts = (u64)fb_data->col_info.victim_luts[0] |
					(((u64)fb_data->col_info.victim_luts[1]) << 32);

			fb_data->cur_update->collision_mask =
			    fb_data->epdc_colliding_luts;

			/* Clear collisions that completed since WB began */
			fb_data->cur_update->collision_mask &=
				~fb_data->luts_complete_wb;

			NTX_TimeStamp_printf("epdc_collision_mask",0,"collision mask=0x%llx",fb_data->epdc_colliding_luts);
			dev_dbg(fb_data->dev, "Collision mask = 0x%llx\n",
			       fb_data->epdc_colliding_luts);
			EPDC_VPRINT(fb_data,3, "Collision mask = 0x%llx\n",
			       fb_data->epdc_colliding_luts);

			/* For EPDC 2.0 and later, minimum collision bounds
			   are provided by HW.  Recompute new bounds here. */
			if ((fb_data->upd_scheme != UPDATE_SCHEME_SNAPSHOT)
				&& (fb_data->rev >= 20)) {
				u32 xres, yres, rotate;
				struct mxcfb_rect adj_update_region;
				struct mxcfb_rect *cur_upd_rect =
					&fb_data->cur_update->update_desc->upd_data.update_region;
				GALLEN_DBGLOCAL_RUNLOG(13);

				if (fb_data->epdc_wb_mode) {
					adjust_coordinates(fb_data->epdc_fb_var.xres,
						fb_data->epdc_fb_var.yres, fb_data->epdc_fb_var.rotate,
						cur_upd_rect, &adj_update_region);


					EPDC_VPRINT(fb_data,2,"%s(%d) : Collision min_x=%d,min_y=%d,max_x=%d,max_y=%d,adj l=%d,t=%d,w=%d,h=%d,cur l=%d,t=%d,w=%d,h=%d\n",
							__FUNCTION__,__LINE__,
							fb_data->col_info.rect_min_x,fb_data->col_info.rect_min_y,
							fb_data->col_info.rect_max_x,fb_data->col_info.rect_max_y,
							adj_update_region.left,adj_update_region.top,
							adj_update_region.width,adj_update_region.height,
							cur_upd_rect->left,cur_upd_rect->top,
							cur_upd_rect->width,cur_upd_rect->height);
#if (ADJ_COLLISION_REGION_PATCH==1) //[
					if ( ((fb_data->wv_modes.mode_aa!=fb_data->wv_modes.mode_gl16)&&(fb_data->cur_update->update_desc->upd_data.waveform_mode == fb_data->wv_modes.mode_aa)) || \
							((fb_data->wv_modes.mode_aad!=fb_data->wv_modes.mode_gc16)&&(fb_data->cur_update->update_desc->upd_data.waveform_mode == fb_data->wv_modes.mode_aad)) || \
						 	((fb_data->wv_modes.mode_gck16!=fb_data->wv_modes.mode_gl16)&&(fb_data->cur_update->update_desc->upd_data.waveform_mode == fb_data->wv_modes.mode_gck16)) || \
							((fb_data->wv_modes.mode_glkw16!=fb_data->wv_modes.mode_gc16)&&(fb_data->cur_update->update_desc->upd_data.waveform_mode == fb_data->wv_modes.mode_glkw16))
						)
					{
						coll_region.left = adj_update_region.left;
						coll_region.top  = adj_update_region.top;
						coll_region.width  = adj_update_region.width;
						coll_region.height = adj_update_region.height;
					}
					else 
#endif //] ADJ_COLLISION_REGION_PATCH
#if (DITHER_GC16_COLLISION_PATCH==1) //[
					if(fb_data->cur_update->update_desc->upd_data.dither_mode) 
					{
						coll_region.left = adj_update_region.left;
						coll_region.top  = adj_update_region.top;
						coll_region.width  = adj_update_region.width;
						coll_region.height = adj_update_region.height;
					}
					else
#endif //]DITHER_GC16_COLLISION_PATCH
					{
						coll_region.left = fb_data->col_info.rect_min_x + adj_update_region.left;
						coll_region.top  = fb_data->col_info.rect_min_y + adj_update_region.top;
						coll_region.width  = fb_data->col_info.rect_max_x - fb_data->col_info.rect_min_x + 1;
						coll_region.height = fb_data->col_info.rect_max_y - fb_data->col_info.rect_min_y + 1;
					}
					memset(&fb_data->col_info, 0x0, sizeof(struct pxp_collision_info));
				} else {
				/* Get collision region coords from EPDC */
					coll_coord = __raw_readl(EPDC_UPD_COL_CORD);
					coll_size = __raw_readl(EPDC_UPD_COL_SIZE);
					coll_region.left =
						(coll_coord & EPDC_UPD_COL_CORD_XCORD_MASK)
						>> EPDC_UPD_COL_CORD_XCORD_OFFSET;
					coll_region.top =
						(coll_coord & EPDC_UPD_COL_CORD_YCORD_MASK)
						>> EPDC_UPD_COL_CORD_YCORD_OFFSET;
					coll_region.width =
						(coll_size & EPDC_UPD_COL_SIZE_WIDTH_MASK)
						>> EPDC_UPD_COL_SIZE_WIDTH_OFFSET;
					coll_region.height =
						(coll_size & EPDC_UPD_COL_SIZE_HEIGHT_MASK)
						>> EPDC_UPD_COL_SIZE_HEIGHT_OFFSET;
				}
				
				EPDC_VPRINT(fb_data,2, "Coll region: l = %d, "
					"t = %d, w = %d, h = %d\n",
					coll_region.left, coll_region.top,
					coll_region.width, coll_region.height);

				/* Convert coords back to orig orientation */
				switch (fb_data->epdc_fb_var.rotate) {
				case FB_ROTATE_CW:
					xres = fb_data->epdc_fb_var.yres;
					yres = fb_data->epdc_fb_var.xres;
					rotate = FB_ROTATE_CCW;
					break;
				case FB_ROTATE_UD:
					xres = fb_data->epdc_fb_var.xres;
					yres = fb_data->epdc_fb_var.yres;
					rotate = FB_ROTATE_UD;
					break;
				case FB_ROTATE_CCW:
					xres = fb_data->epdc_fb_var.yres;
					yres = fb_data->epdc_fb_var.xres;
					rotate = FB_ROTATE_CW;
					break;
				default:
					xres = fb_data->epdc_fb_var.xres;
					yres = fb_data->epdc_fb_var.yres;
					rotate = FB_ROTATE_UR;
					break;
				}
				adjust_coordinates(xres, yres, rotate,
						&coll_region, cur_upd_rect);

				EPDC_VPRINT(fb_data,2, "Adj coll region: l = %d, "
					"t = %d, w = %d, h = %d\n",
					cur_upd_rect->left, cur_upd_rect->top,
					cur_upd_rect->width,
					cur_upd_rect->height);
			}

			/*
			 * If we collide with newer updates, then
			 * we don't need to re-submit the update. The
			 * idea is that the newer updates should take
			 * precedence anyways, so we don't want to
			 * overwrite them.
			 */
			for (temp_mask = fb_data->cur_update->collision_mask, lut = 0;
				temp_mask != 0;
				lut++, temp_mask = temp_mask >> 1) {
				GALLEN_DBGLOCAL_RUNLOG(13);
				if (!(temp_mask & 0x1))
					continue;

				if (fb_data->lut_update_order[lut] >=
					fb_data->cur_update->update_desc->update_order) {
					dev_info(fb_data->dev,
						"Ignoring collision with"
						"newer update.\n");
					ignore_collision = true;
					GALLEN_DBGLOCAL_RUNLOG(15);
					break;
				}
			}

			if (!ignore_collision) {
				GALLEN_DBGLOCAL_RUNLOG(16);
				free_update = false;
				/*
				 * If update has markers, clear the LUTs to
				 * avoid signalling that they have completed.
				 */
				list_for_each_entry_safe(next_marker, temp,
					&fb_data->cur_update->update_desc->upd_marker_list,
					upd_list)
					next_marker->lut_num = INVALID_LUT;

				/* Move to collision list */
				EPDC_VPRINT(fb_data,1,"lut%d re-setup for collision region ,flags=0x%x,upd_order=%d\n",
						fb_data->cur_update->lut_num,
						fb_data->cur_update->update_desc->upd_data.flags,
						fb_data->cur_update->update_desc->update_order);

				list_add_tail(&fb_data->cur_update->list,
					 &fb_data->upd_buf_collision_list);
#if (TCE_UNDERRUN_PREVENT_PATCH==1) // avoid TCE underrun after collision occurring . 
				{
					int i,iMaxWaitCnt=50;

					EPDC_VPRINT(fb_data,1,"waiting for lut%d complete to re-send collision ...\n",
							fb_data->lastest_lut_num);
					
					//mutex_unlock(&fb_data->queue_mutex);

					for(i=0;i<iMaxWaitCnt;i++) {
						if(epdc_is_lut_complete(fb_data->rev,fb_data->lastest_lut_num)) {
							EPDC_VPRINT(fb_data,1,"lut%d complete !\n",fb_data->lastest_lut_num);
							break;
						}
						msleep(10);
					}
					if(i>=iMaxWaitCnt) {
						EPDC_VPRINT(fb_data,1,"[collision] waiting for lut%d complete timeout !!\n",fb_data->lastest_lut_num);
					}
					EPDC_VPRINT(fb_data,1,"[collision]lut%d wait %dms done, re-sending update !!\n",fb_data->lastest_lut_num,10*i);

					//mutex_lock(&fb_data->queue_mutex);
				}
#endif//] (TCE_UNDERRUN_PREVENT_PATCH==1) 

			}
		}

		/* Do we need to free the current update descriptor? */
		if (free_update) {
			GALLEN_DBGLOCAL_RUNLOG(17);
			/* Handle condition where WB & LUT are both complete */
			if (wb_lut_done)
				list_for_each_entry_safe(next_marker, temp,
					&fb_data->cur_update->update_desc->upd_marker_list,
					upd_list) {

					/* Del from per-update & full list */
					list_del_init(&next_marker->upd_list);
					list_del_init(&next_marker->full_list);

					/* Signal completion of update */
					EPDC_VPRINT(fb_data,3,
						"Signaling marker (wb) %d\n",
						next_marker->update_marker);
					if (next_marker->waiting)
						complete(&next_marker->update_completion);
					else
						kfree(next_marker);
				}

			/* Free marker list and update descriptor */
			kfree(fb_data->cur_update->update_desc);

			/* Add to free buffer list */
			list_add_tail(&fb_data->cur_update->list,
				 &fb_data->upd_buf_free_list);


			GALLEN_DBGLOCAL_RUNLOG(20);
			/* Check to see if all updates have completed */
			if (list_empty(&fb_data->upd_pending_list) &&
				is_free_list_full(fb_data) &&
				!epdc_luts_active) {

				GALLEN_DBGLOCAL_RUNLOG(21);
				fb_data->updates_active = false;

				if (fb_data->pwrdown_delay !=
						FB_POWERDOWN_DISABLE) {
					GALLEN_DBGLOCAL_RUNLOG(22);
					/*
					 * Set variable to prevent overlapping
					 * enable/disable requests
					 */
					fb_data->powering_down = true;

					/* Schedule EPDC disable */
					if(fb_data->vcom_off_with_data) {
						if (regulator_is_enabled(fb_data->vcom_regulator)) {
							udelay(VCOM_OFF_DELAY_US);
							regulator_disable(fb_data->vcom_regulator);
						}
					}
					if( ( (giLast_waveform_mode==fb_data->wv_modes.mode_glkw16 && \
							(fb_data->wv_modes.mode_gl16!=fb_data->wv_modes.mode_glkw16)) ||\
						(giLast_waveform_mode==fb_data->wv_modes.mode_gck16 &&\
							(fb_data->wv_modes.mode_gc16!=fb_data->wv_modes.mode_gck16)) ) ) 
					{
						schedule_delayed_work(&fb_data->epdc_done_work,msecs_to_jiffies(NIGHTMODE_TVG_MS));
					}
					else {
						schedule_delayed_work(&fb_data->epdc_done_work,
							msecs_to_jiffies(fb_data->pwrdown_delay));
					}

					/* Reset counter to reduce chance of overflow */
					fb_data->order_cnt = 0;
				}

				if (fb_data->waiting_for_idle) {
					GALLEN_DBGLOCAL_RUNLOG(23);
					complete(&fb_data->updates_done);
				}
			}
		}

		/* Clear current update */
		fb_data->cur_update = NULL;

		/* Clear IRQ for working buffer */
		epdc_working_buf_intr(false);
		epdc_clear_working_buf_irq();
	}

	if (fb_data->upd_scheme != UPDATE_SCHEME_SNAPSHOT) {
		/* Queued update scheme processing */
		GALLEN_DBGLOCAL_RUNLOG(24);

		/* Schedule task to submit collision and pending update */
		if (!fb_data->powering_down) {
			GALLEN_DBGLOCAL_RUNLOG(31);
			queue_work(fb_data->epdc_submit_workqueue,
				&fb_data->epdc_submit_work);
		}

		/* Release buffer queues */
		mutex_unlock(&fb_data->queue_mutex);

		GALLEN_DBGLOCAL_ESC();
		return;
	}

	/* Snapshot update scheme processing */

	/* Check to see if any LUTs are free */
	if (!epdc_luts_avail) {
		dev_dbg(fb_data->dev, "No luts available.\n");
		mutex_unlock(&fb_data->queue_mutex);
		GALLEN_DBGLOCAL_ESC();
		return;
	}

	epdc_next_lut_15 = epdc_choose_next_lut(fb_data, &next_lut);
	/* Check to see if there is a valid LUT to use */
	if (epdc_next_lut_15 && fb_data->tce_prevent && (fb_data->rev < 20)) {
		dev_dbg(fb_data->dev, "Must wait for LUT15\n");
		mutex_unlock(&fb_data->queue_mutex);
		GALLEN_DBGLOCAL_ESC();
		return;
	}

	/*
	 * Are any of our collision updates able to go now?
	 * Go through all updates in the collision list and check to see
	 * if the collision mask has been fully cleared
	 */
	list_for_each_entry(collision_update,
			    &fb_data->upd_buf_collision_list, list) {

		if (collision_update->collision_mask != 0)
			continue;

		dev_dbg(fb_data->dev, "A collision update is ready to go!\n");
		/*
		 * We have a collision cleared, so select it
		 * and we will retry the update
		 */
		fb_data->cur_update = collision_update;
		list_del_init(&fb_data->cur_update->list);
		break;
	}

	/*
	 * If we didn't find a collision update ready to go,
	 * we try to grab one from the update queue
	 */
	if (fb_data->cur_update == NULL) {
		GALLEN_DBGLOCAL_RUNLOG(25);
		/* Is update list empty? */
		if (list_empty(&fb_data->upd_buf_queue)) {
			GALLEN_DBGLOCAL_RUNLOG(26);
			dev_dbg(fb_data->dev, "No pending updates.\n");

			/* No updates pending, so we are done */
			mutex_unlock(&fb_data->queue_mutex);
			GALLEN_DBGLOCAL_ESC();
			return;
		} else {
			dev_dbg(fb_data->dev, "Found a pending update!\n");
			GALLEN_DBGLOCAL_RUNLOG(27);

			/* Process next item in update list */
			fb_data->cur_update =
			    list_entry(fb_data->upd_buf_queue.next,
				       struct update_data_list, list);
			list_del_init(&fb_data->cur_update->list);
		}
	}

	/* Use LUT selected above */
	fb_data->cur_update->lut_num = next_lut;

	/* Associate LUT with update markers */
	list_for_each_entry_safe(next_marker, temp,
		&fb_data->cur_update->update_desc->upd_marker_list, upd_list)
		next_marker->lut_num = fb_data->cur_update->lut_num;

	/* Mark LUT as containing new update */
	fb_data->lut_update_order[fb_data->cur_update->lut_num] =
		fb_data->cur_update->update_desc->update_order;

	/* Enable Collision and WB complete IRQs */
	epdc_working_buf_intr(true);
	epdc_lut_complete_intr(fb_data->rev, fb_data->cur_update->lut_num, true);

	/* Program EPDC update to process buffer */
	next_upd_region =
		&fb_data->cur_update->update_desc->upd_data.update_region;

	/* add working buffer update here for external mode */
	if (fb_data->epdc_wb_mode)
		epdc_working_buffer_update(fb_data, fb_data->cur_update,
				next_upd_region);

	if (fb_data->cur_update->update_desc->upd_data.temp
		!= TEMP_USE_AMBIENT) {
		temp_index = mxc_epdc_fb_get_temp_index(fb_data,
			fb_data->cur_update->update_desc->upd_data.temp);
		epdc_set_temp(temp_index);
	} else
		epdc_set_temp(fb_data->temp_index);
	epdc_set_update_addr(fb_data->cur_update->phys_addr +
				fb_data->cur_update->update_desc->epdc_offs);
	epdc_set_update_coord(next_upd_region->left, next_upd_region->top);
	epdc_set_update_dimensions(next_upd_region->width,
				   next_upd_region->height);
	if (fb_data->rev > 20)
		epdc_set_update_stride(fb_data->cur_update->update_desc->epdc_stride);
	if (fb_data->wv_modes_update &&
		(fb_data->cur_update->update_desc->upd_data.waveform_mode
			== WAVEFORM_MODE_AUTO)) {
		epdc_set_update_waveform(&fb_data->wv_modes);
		fb_data->wv_modes_update = false;
	}

	epdc_submit_update(fb_data->cur_update->lut_num,
			   fb_data->cur_update->update_desc->upd_data.waveform_mode,
			   fb_data->cur_update->update_desc->upd_data.update_mode,
			   false, false, 0);

	/* Release buffer queues */
	mutex_unlock(&fb_data->queue_mutex);

	GALLEN_DBGLOCAL_END();
	return;
}

static void draw_mode0(struct mxc_epdc_fb_data *fb_data)
{
	u32 *upd_buf_ptr;
	int i;
	struct fb_var_screeninfo *screeninfo = &fb_data->epdc_fb_var;
	u32 xres, yres;

	upd_buf_ptr = (u32 *)fb_data->info.screen_base;

	epdc_working_buf_intr(true);
	epdc_lut_complete_intr(fb_data->rev, 0, true);

	/* Use unrotated (native) width/height */
	if ((screeninfo->rotate == FB_ROTATE_CW) ||
		(screeninfo->rotate == FB_ROTATE_CCW)) {
		xres = screeninfo->yres;
		yres = screeninfo->xres;
	} else {
		xres = screeninfo->xres;
		yres = screeninfo->yres;
	}

	/* Program EPDC update to process buffer */
	epdc_set_update_addr(fb_data->phys_start);
	epdc_set_update_coord(0, 0);
	epdc_set_update_dimensions(xres, yres);
	if (fb_data->rev > 20)
		epdc_set_update_stride(0);
	epdc_submit_update(0, fb_data->wv_modes.mode_init, UPDATE_MODE_FULL,
		false, true, 0xFF);

	dev_dbg(fb_data->dev, "Mode0 update - Waiting for LUT to complete...\n");

	/* Will timeout after ~4-5 seconds */

	for (i = 0; i < 40; i++) {
		if (!epdc_is_lut_active(0)) {
			dev_dbg(fb_data->dev, "Mode0 init complete\n");
			return;
		}
		msleep(100);
	}

	dev_err(fb_data->dev, "Mode0 init failed!\n");

	return;
}


static void mxc_epdc_fb_fw_handler(const struct firmware *fw,
						     void *context)
{
	struct mxc_epdc_fb_data *fb_data = context;
	int ret;
	struct mxcfb_waveform_data_file *wv_file;
	int wv_data_offs;
	int i;
	struct mxcfb_update_data update;
	struct mxcfb_update_marker_data upd_marker_data;
	struct fb_var_screeninfo *screeninfo = &fb_data->epdc_fb_var;
	u32 xres, yres;
	struct clk *epdc_parent;
	unsigned long rounded_parent_rate, epdc_pix_rate,
			rounded_pix_clk, target_pix_clk;
#ifdef FW_IN_RAM //[
	static struct firmware ram_fw;
#endif //] FW_IN_RAM 

	DBG0_MSG("%s(%d),fw=%p\n",__FUNCTION__,__LINE__,fw);
#ifdef FW_IN_RAM //[
	if(gpbWF_vaddr) {
		ram_fw.size = gdwWF_size ;
		ram_fw.data = (u8*) gpbWF_vaddr ;
		printk("%s(): fw p=%p,size=%u\n",__FUNCTION__,ram_fw.data,ram_fw.size);
		ret = 0;
		fw=&ram_fw;
	}
	else 
#endif //] FW_IN_RAM
	{

		if (fw == NULL) {
			/* If default FW file load failed, we give up */
			if (fb_data->fw_default_load)
				return;

		
			/* Try to load default waveform */
			dev_dbg(fb_data->dev,
				"Can't find firmware. Trying fallback fw\n");
			fb_data->fw_default_load = true;
			ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				"imx/epdc/epdc.fw", fb_data->dev, GFP_KERNEL, fb_data,
				mxc_epdc_fb_fw_handler);
			if (ret)
				dev_err(fb_data->dev,
					"Failed request_firmware_nowait err %d\n", ret);
		

			return;
		}
	}
	gbModeVersion = *(fw->data+0x10);
	gbWFM_REV = *(fw->data+0x16);
	gbFPL_Platform=*(fw->data+0x0d);


	if((0x20==gbModeVersion)||(0x19==gbModeVersion)||(0x18==gbModeVersion)||(0x43==gbModeVersion))
	{
		if(0x18==gbModeVersion) {
			fb_data->wv_modes.mode_gc4 = 2; /* GC4 mode */
		}
		else {
			fb_data->wv_modes.mode_gc4 = 7; /* GC4 mode */
		}
		fb_data->wv_modes.mode_du = 1; /* DU mode */
#ifdef MXCFB_WAVEFORM_MODES_NTX//[
		fb_data->wv_modes.mode_gl16 = 3; /* GL16 mode */
		fb_data->wv_modes.mode_a2 = 6; /* A2 mode */

#ifdef WFM_ENABLE_AA//[
		fb_data->wv_modes.mode_aa = 4; /* REAGL mode */
#endif //]WFM_ENABLE_AA

#ifdef WFM_ENABLE_AAD//[
		fb_data->wv_modes.mode_aad = 5; /*REAGL-D mode */
#endif //]WFM_ENABLE_AAD

#endif //]MXCFB_WAVEFORM_MODES_NTX

	}
	else if(0x31==gbModeVersion || 0x58==gbModeVersion) {
		// AG/HH TYPE waveform with night modes . 
		fb_data->wv_modes.mode_du = 1; /* DU mode */
#ifdef MXCFB_WAVEFORM_MODES_NTX//[

		fb_data->wv_modes.mode_gl16 = 3; /* GL16 mode as GLR16*/
		fb_data->wv_modes.mode_a2 = 6; /* A2 mode */

#ifdef WFM_ENABLE_AA//[
		fb_data->wv_modes.mode_aa = 4; /* REAGL GLR16 AA*/
#endif //]WFM_ENABLE_AA
#ifdef WFM_ENABLE_AAD//[
		// no GLD waveform mode . 
		fb_data->wv_modes.mode_aad = fb_data->wv_modes.mode_gc16; /*REAGL-D mode as GC16*/
#endif //]WFM_ENABLE_AAD

#ifdef WFM_ENABLE_DU4//[
		fb_data->wv_modes.mode_du4 = 7; /* DU4 */
#endif //]WFM_ENABLE_DU4
#ifdef WFM_ENABLE_GCK16//[
		fb_data->wv_modes.mode_gck16 = 8; /* GCK16 */
#endif //]WFM_ENABLE_GCK16
#ifdef WFM_ENABLE_GLKW16//[
		fb_data->wv_modes.mode_glkw16 = 9; /* GLKW16 */
#endif //]WFM_ENABLE_GLKW16

#endif //]MXCFB_WAVEFORM_MODES_NTX
	}
	else if(0x13==gbModeVersion) {
		fb_data->wv_modes.mode_du = 1; /* DU mode */
#ifdef MXCFB_WAVEFORM_MODES_NTX//[

		fb_data->wv_modes.mode_gl16 = 3; /* GL16 mode as GLR16*/
		fb_data->wv_modes.mode_a2 = 4; /* A2 mode */

#ifdef WFM_ENABLE_AA//[
		fb_data->wv_modes.mode_aa = 3; /* REAGL mode as GLR16*/
#endif //]WFM_ENABLE_AA
#ifdef WFM_ENABLE_AAD//[
		fb_data->wv_modes.mode_aad = fb_data->wv_modes.mode_gc16; /*REAGL-D mode as GC16*/
#endif //]WFM_ENABLE_AAD
#endif //]MXCFB_WAVEFORM_MODES_NTX
	}
	else if(0x23==gbModeVersion) {
		// AD type non regal waveform .
		fb_data->wv_modes.mode_du = 1; /* DU mode */
#ifdef MXCFB_WAVEFORM_MODES_NTX//[
		fb_data->wv_modes.mode_gl16 = 3; /* GL16 mode */
		fb_data->wv_modes.mode_a2 = 4; /* A2 mode */

#ifdef WFM_ENABLE_AA//[
		fb_data->wv_modes.mode_aa = fb_data->wv_modes.mode_gl16; /* REAGL mode as GLR16*/
#endif //]WFM_ENABLE_AA

#ifdef WFM_ENABLE_AAD//[
		fb_data->wv_modes.mode_aad = fb_data->wv_modes.mode_gc16; /*REAGL-D mode as GC16*/
#endif //]WFM_ENABLE_AAD
#endif //]MXCFB_WAVEFORM_MODES_NTX
	}
	else if(0x4==gbModeVersion) {
#ifdef MXCFB_WAVEFORM_MODES_NTX//[
		fb_data->wv_modes.mode_gl16 = 5; /* GL16 mode */
		fb_data->wv_modes.mode_a2 = 4; /* A2 mode */
#ifdef WFM_ENABLE_AA//[
		fb_data->wv_modes.mode_aa = 5; /* REAGL mode */
#endif //]WFM_ENABLE_AA
#ifdef WFM_ENABLE_AAD//[
		fb_data->wv_modes.mode_aad = 5; /* REAGL-D mode */
#endif //]WFM_ENABLE_AAD
#endif //]MXCFB_WAVEFORM_MODES_NTX
	}
	else if(0x15==gbModeVersion||0x9==gbModeVersion) {
		// WY type .
#ifdef MXCFB_WAVEFORM_MODES_NTX//[
		fb_data->wv_modes.mode_gl16 = 3; /* GL16 mode */
		fb_data->wv_modes.mode_a2 = 4; /* A2 mode */

#ifdef WFM_ENABLE_AA//[
		fb_data->wv_modes.mode_aa = fb_data->wv_modes.mode_gl16; /* REAGL mode */
#endif //]WFM_ENABLE_AA

#ifdef WFM_ENABLE_AAD//[
		fb_data->wv_modes.mode_aad = fb_data->wv_modes.mode_gc16; /* REAGL-D mode */
#endif //]WFM_ENABLE_AAD

#endif //]MXCFB_WAVEFORM_MODES_NTX
		// when mode version is 0x15
		//  GC4=5 
		//  GL4=6 
		  
	}
	else {
		// no a2/aa/aad/gl16 .
#ifdef MXCFB_WAVEFORM_MODES_NTX//[
		fb_data->wv_modes.mode_gl16 = fb_data->wv_modes.mode_gc16; /* GL16 mode */
		fb_data->wv_modes.mode_a2 = fb_data->wv_modes.mode_du; /* A2 mode as DU */

#ifdef WFM_ENABLE_AA//[
		fb_data->wv_modes.mode_aa = fb_data->wv_modes.mode_gc16; /* REAGL mode */
#endif //]WFM_ENABLE_AA

#ifdef WFM_ENABLE_AAD//[
		fb_data->wv_modes.mode_aad = fb_data->wv_modes.mode_gc16; /* REAGL-D mode */
#endif //]WFM_ENABLE_AAD
#endif //]MXCFB_WAVEFORM_MODES_NTX
	}

#ifdef WFM_ENABLE_DU4//[
	if(0==fb_data->wv_modes.mode_du4) {
		fb_data->wv_modes.mode_du4 = fb_data->wv_modes.mode_du;
	}
#endif //] WFM_ENABLE_DU4
#ifdef WFM_ENABLE_GCK16//[
	if(0==fb_data->wv_modes.mode_gck16) {
		fb_data->wv_modes.mode_gck16 = fb_data->wv_modes.mode_gc16;
	}
#endif //] WFM_ENABLE_GCK16
#ifdef WFM_ENABLE_GLKW16//[
	if(0==fb_data->wv_modes.mode_glkw16) {
		fb_data->wv_modes.mode_glkw16 = fb_data->wv_modes.mode_gl16;
	}
#endif //] WFM_ENABLE_GLKW16
	
#if defined(NO_CUS_REAGL_MODE) && defined(MXCFB_WAVEFORM_MODES_NTX) //[

#ifdef WFM_ENABLE_AA//[
	fb_data->wv_modes.mode_aa = fb_data->wv_modes.mode_gc16; /* REAGL mode */
#endif //]WFM_ENABLE_AA
#ifdef WFM_ENABLE_AAD//[
	fb_data->wv_modes.mode_aad = fb_data->wv_modes.mode_gc16; /* REAGL-D mode */
#endif //]WFM_ENABLE_AAD
#endif //]NO_CUS_REAGL_MODE && MXCFB_WAVEFORM_MODES_NTX


	fb_data->wv_modes_update = true;

	giNTX_waveform_modeA[NTX_WFM_MODE_INIT] = fb_data->wv_modes.mode_init;
	giNTX_waveform_modeA[NTX_WFM_MODE_DU] = fb_data->wv_modes.mode_du;
	giNTX_waveform_modeA[NTX_WFM_MODE_GC16] = fb_data->wv_modes.mode_gc16;
	giNTX_waveform_modeA[NTX_WFM_MODE_GC4] = fb_data->wv_modes.mode_gc4;



#ifdef MXCFB_WAVEFORM_MODES_NTX//[
	giNTX_waveform_modeA[NTX_WFM_MODE_A2] = fb_data->wv_modes.mode_a2;
	giNTX_waveform_modeA[NTX_WFM_MODE_GL16] = fb_data->wv_modes.mode_gl16;

#ifdef WFM_ENABLE_AA//[
	giNTX_waveform_modeA[NTX_WFM_MODE_GLR16] = fb_data->wv_modes.mode_aa;
#else //][!WFM_ENABLE_AA
	giNTX_waveform_modeA[NTX_WFM_MODE_GLR16] = fb_data->wv_modes.mode_gl16;
#endif //]WFM_ENABLE_AA
#ifdef WFM_ENABLE_AAD//[
	giNTX_waveform_modeA[NTX_WFM_MODE_GLD16] = fb_data->wv_modes.mode_aad;
#else //][!WFM_ENABLE_AAD
	giNTX_waveform_modeA[NTX_WFM_MODE_GLD16] = fb_data->wv_modes.mode_gc16;
#endif //]WFM_ENABLE_AAD

#ifdef WFM_ENABLE_DU4//[
	giNTX_waveform_modeA[NTX_WFM_MODE_DU4] = fb_data->wv_modes.mode_du4;
#endif //]WFM_ENABLE_DU4

#ifdef WFM_ENABLE_GCK16//[
	giNTX_waveform_modeA[NTX_WFM_MODE_GCK16] = fb_data->wv_modes.mode_gck16;
#endif //]WFM_ENABLE_GCK16

#ifdef WFM_ENABLE_GLKW16//[
	giNTX_waveform_modeA[NTX_WFM_MODE_GLKW16] = fb_data->wv_modes.mode_glkw16;
#endif //]WFM_ENABLE_GLKW16

#endif //] MXCFB_WAVEFORM_MODES_NTX



	wv_file = (struct mxcfb_waveform_data_file *)fw->data;
	if(0==gpbWF_vaddr) {
		gpbWF_vaddr = (unsigned char *)fw->data;
	}

	dump_fw_header(fb_data->dev, wv_file);

	/* Get size and allocate temperature range table */
	fb_data->trt_entries = wv_file->wdh.trc + 1;
	fb_data->temp_range_bounds = kzalloc(fb_data->trt_entries+1, GFP_KERNEL);

	/* Copy TRT data */
	memcpy(fb_data->temp_range_bounds, &wv_file->data, fb_data->trt_entries+1);

	for (i = 0; i <= fb_data->trt_entries; i++) {
		DBG_MSG("trt entry #%d = 0x%x(%d~%d)\n", i, *((u8 *)&wv_file->data + i),
				fb_data->temp_range_bounds[i],fb_data->temp_range_bounds[i+1]-1);
	}

	/* Set default temperature index using TRT and room temp */
	fb_data->temp_index = mxc_epdc_fb_get_temp_index(fb_data, DEFAULT_TEMP);

	/* Get offset and size for waveform data */
	wv_data_offs = sizeof(wv_file->wdh) + fb_data->trt_entries + 1;
	fb_data->waveform_buffer_size = fw->size - wv_data_offs;

	/* process 2nd generation waveform data which may contain 
	 * the voltage control data, advance waveform data,
	 * and extra waveform  data
	 */

	/* 2nd gen waveform */
	fb_data->waveform_vcd_buffer = NULL;
	fb_data->waveform_acd_buffer = NULL;

	{
		int awv, wmc, wtrc, xwia;
		u64 longOffset;
		u32 bufferSize;
		u8 *fwDataBuffer = (u8 *)(fw->data) + wv_data_offs;

		wtrc = wv_file->wdh.trc + 1;
		wmc = wv_file->wdh.mc + 1;
		awv = wv_file->wdh.awf;
		xwia = wv_file->wdh.xwia;
		memcpy (&longOffset,fwDataBuffer,8);
		if ((unsigned) longOffset > (8*wmc))
		{
			u64 avcOffset, acdOffset, acdMagic, xwiOffset;
			avcOffset = acdOffset = acdMagic = xwiOffset = 0l;
			EPDC_VPRINT(fb_data,0,"advance waveform awv : %d \n",awv);
			/* look at the advance waveform flags */
			switch ( awv ) {
				case 0 : /* voltage control flag is set */
					if (xwia > 0) {
						/* extra waveform information */
						memcpy (&xwiOffset,fwDataBuffer + (8*wmc),8);
						bufferSize = (unsigned)(fb_data->waveform_buffer_size - xwiOffset);
						fb_data->waveform_xwi_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_xwi_buffer, fwDataBuffer+xwiOffset, bufferSize );
						fb_data->waveform_buffer_size = xwiOffset;
					}
					break;
				case 1 : /* voltage control flag is set */
					memcpy (&avcOffset,fwDataBuffer + (8*wmc),8);
					if (xwia > 0) {
						/* extra waveform information */
						memcpy (&xwiOffset,fwDataBuffer + (8*wmc) +8,8);
						bufferSize = (unsigned)(fb_data->waveform_buffer_size - xwiOffset);
						fb_data->waveform_xwi_buffer = kmalloc(bufferSize, GFP_KERNEL);
						/* voltage control data */
						memcpy(fb_data->waveform_xwi_buffer, fwDataBuffer+xwiOffset, bufferSize );
						bufferSize = (unsigned)(xwiOffset - avcOffset);
						fb_data->waveform_vcd_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_vcd_buffer, fwDataBuffer+avcOffset, bufferSize );
					}
					else {
						/* voltage control data */
						bufferSize = (unsigned)(fb_data->waveform_buffer_size - avcOffset);
						fb_data->waveform_vcd_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_vcd_buffer, fwDataBuffer+avcOffset, bufferSize );
					}
					fb_data->waveform_buffer_size = avcOffset;
					break;
				case 2 : /* voltage control flag is set */
					memcpy (&acdOffset,fwDataBuffer + (8*wmc),8);
					memcpy (&acdMagic,fwDataBuffer + (8*wmc) + 8,8);
					if (xwia > 0) {
						/* extra waveform information */
						memcpy (&xwiOffset,fwDataBuffer + (8*wmc) + 16,8);
						bufferSize = (unsigned)(fb_data->waveform_buffer_size - xwiOffset);
						fb_data->waveform_xwi_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_xwi_buffer, fwDataBuffer+xwiOffset, bufferSize );
						/* algorithm control data */
						bufferSize = (unsigned)(xwiOffset - acdOffset);
						fb_data->waveform_acd_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_acd_buffer, fwDataBuffer+acdOffset, bufferSize );
					}
					else {
						/* algorithm control data */
						bufferSize = (unsigned)(fb_data->waveform_buffer_size - acdOffset);
						fb_data->waveform_acd_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_acd_buffer, fwDataBuffer+acdOffset, bufferSize );
					}
					fb_data->waveform_buffer_size = acdOffset;
					break;
				case 3 : /* voltage control flag is set */
					memcpy (&avcOffset,fwDataBuffer + (8*wmc),8);
					memcpy (&acdOffset,fwDataBuffer + (8*wmc) + 8,8);
					memcpy (&acdMagic,fwDataBuffer + (8*wmc) + 16,8);
					if (xwia > 0) {
						/* extra waveform information */
						memcpy (&xwiOffset,fwDataBuffer + (8*wmc) + 24,8);
						bufferSize = (unsigned)(fb_data->waveform_buffer_size - xwiOffset);
						fb_data->waveform_xwi_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_xwi_buffer, fwDataBuffer+xwiOffset, bufferSize );
						/* algorithm control data */
						bufferSize = (unsigned)(xwiOffset - acdOffset);
						fb_data->waveform_acd_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_acd_buffer, fwDataBuffer+acdOffset, bufferSize );
						/* voltage control data */
						bufferSize = (unsigned)(acdOffset - avcOffset);
						fb_data->waveform_vcd_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_vcd_buffer, fwDataBuffer+avcOffset, bufferSize );
					}
					else {
						/* algorithm control data */
						bufferSize = (unsigned)(fb_data->waveform_buffer_size - acdOffset);
						fb_data->waveform_acd_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_acd_buffer, fwDataBuffer+avcOffset, bufferSize );
						/* voltage control data */
						bufferSize = (unsigned)(acdOffset - avcOffset);
						fb_data->waveform_vcd_buffer = kmalloc(bufferSize, GFP_KERNEL);
						memcpy(fb_data->waveform_vcd_buffer, fwDataBuffer+avcOffset, bufferSize );
					}
					fb_data->waveform_buffer_size = avcOffset;
					break;
				}
			if (acdMagic) fb_data->waveform_magic_number = acdMagic;
			/* store the waveform mode count and waveform temperature range count
			 */
			fb_data->waveform_mc = wmc;
			fb_data->waveform_trc =wtrc;
		}
	}

	/* get the extra waveform info and display it - This can be removed! It is here for illustration only */
	if (fb_data->waveform_xwi_buffer) {
		//char *xwiString;
		unsigned strLength = mxc_epdc_fb_fetch_wxi_data(fb_data->waveform_xwi_buffer, NULL);

		dev_info(fb_data->dev, " --- Extra Waveform Data length: %d bytes---\n",strLength);
		if (strLength > 0) {
			//xwiString = (char *) kmalloc(strLength + 1, GFP_KERNEL);
			fb_data->waveform_xwi_string = (char *) kmalloc(strLength + 1, GFP_KERNEL);
			//if (mxc_epdc_fb_fetch_wxi_data(fb_data->waveform_xwi_buffer, xwiString) > 0) 
			if (mxc_epdc_fb_fetch_wxi_data(fb_data->waveform_xwi_buffer, fb_data->waveform_xwi_string) > 0) 
			{
				//xwiString[strLength+1] = '\0';
				fb_data->waveform_xwi_string[strLength] = '\0' ;
				fb_data->waveform_xwi_string_length = strLength ;
				//dev_info(fb_data->dev, "     Extra Waveform Data: %s\n",xwiString);
				dev_info(fb_data->dev, "     Extra Waveform Data: %s\n",fb_data->waveform_xwi_string);
			}
			else
				dev_err(fb_data->dev, " *** Extra Waveform Data checksum error ***\n");

			//kfree(xwiString);
		}	
	}

	/* show the vcd */
	if (fb_data->waveform_vcd_buffer) {
		{
			struct epd_vc_data vcd;
			/* fetch and display the voltage control data for waveform mode 0, temp range 0 */
			fetch_Epdc_Pmic_Voltages(&vcd, fb_data, 0, 0);
		}
	}

#ifdef FW_IN_RAM //[
	fb_data->waveform_buffer_virt = gpbWF_vaddr + wv_data_offs;
	fb_data->waveform_buffer_phys = gpbWF_paddr + wv_data_offs;
#else // ][!FW_IN_RAM
	/* Allocate memory for waveform data */
	fb_data->waveform_buffer_virt = dma_alloc_coherent(fb_data->dev,
						fb_data->waveform_buffer_size,
						&fb_data->waveform_buffer_phys,
						GFP_DMA | GFP_KERNEL);
	if (fb_data->waveform_buffer_virt == NULL) {
		dev_err(fb_data->dev, "Can't allocate mem for waveform!\n");
		return;
	}

	memcpy(fb_data->waveform_buffer_virt, (u8 *)(fw->data) + wv_data_offs,
		fb_data->waveform_buffer_size);
#endif //]FW_IN_RAM


	/* 2nd gen waveform */
	fb_data->waveform_vcd_buffer = NULL;
	fb_data->waveform_acd_buffer = NULL;
	/* Check for advanced waveform data */
//	if (wv_file->wdh.awf != 0) 
	if((wv_file->wdh.luts & WAVEFORM_HDR_LUT_ADVANCED_ALGO_MASK) != 0)
	{
		dev_dbg(fb_data->dev,
			"Waveform file supports advanced algorithms\n");

		fb_data->waveform_is_advanced = true;
		/* Parse advance waveform data. */
		epdc_parse_awf_data(fb_data, &wv_file->wdh,
			(u8*)fb_data->waveform_buffer_virt);
	} else {
		dev_info(fb_data->dev,
			"Waveform file does not support advanced algorithms\n");
		fb_data->waveform_is_advanced = false;
	}

#ifdef FW_IN_RAM //[
	if(gpbWF_vaddr) {
	}
	else 
#endif //] FW_IN_RAM
	{
		release_firmware(fw);
	}

	/* Enable clocks to access EPDC regs */
	clk_prepare_enable(fb_data->epdc_clk_axi);

	target_pix_clk = fb_data->cur_mode->vmode->pixclock;


	rounded_pix_clk = clk_round_rate(fb_data->epdc_clk_pix, target_pix_clk);

	if (((rounded_pix_clk >= target_pix_clk + target_pix_clk/100) ||
		(rounded_pix_clk <= target_pix_clk - target_pix_clk/100))) {
		/* Can't get close enough without changing parent clk */
		epdc_parent = clk_get_parent(fb_data->epdc_clk_pix);
		rounded_parent_rate = clk_round_rate(epdc_parent, target_pix_clk);

		epdc_pix_rate = target_pix_clk;
		while (epdc_pix_rate < rounded_parent_rate)
			epdc_pix_rate *= 2;
		clk_set_rate(epdc_parent, epdc_pix_rate);

		rounded_pix_clk = clk_round_rate(fb_data->epdc_clk_pix, target_pix_clk);
		if (((rounded_pix_clk >= target_pix_clk + target_pix_clk/100) ||
			(rounded_pix_clk <= target_pix_clk - target_pix_clk/100)))
			/* Still can't get a good clock, provide warning */
			dev_err(fb_data->dev, "Unable to get an accurate EPDC pix clk"
				"desired = %lu, actual = %lu\n", target_pix_clk,
				rounded_pix_clk);
	}

	clk_set_rate(fb_data->epdc_clk_pix, rounded_pix_clk);

	/* Enable pix clk for EPDC */
	clk_prepare_enable(fb_data->epdc_clk_pix);

	epdc_init_sequence(fb_data);

	/* Disable clocks */
	clk_disable_unprepare(fb_data->epdc_clk_axi);
	clk_disable_unprepare(fb_data->epdc_clk_pix);

	fb_data->hw_ready = true;
	fb_data->hw_initializing = false;

	/* Use unrotated (native) width/height */
	if ((screeninfo->rotate == FB_ROTATE_CW) ||
		(screeninfo->rotate == FB_ROTATE_CCW)) {
		xres = screeninfo->yres;
		yres = screeninfo->xres;
	} else {
		xres = screeninfo->xres;
		yres = screeninfo->yres;
	}

#if 0
	{
		struct mxcfb_update_data update;
		struct mxcfb_update_marker_data upd_marker_data;

		update.update_region.left = 0;
		update.update_region.width = xres;
		update.update_region.top = 0;
		update.update_region.height = yres;
		update.update_mode = UPDATE_MODE_FULL;
		update.waveform_mode = ;
		update.update_marker = INIT_UPDATE_MARKER;
		update.temp = TEMP_USE_AMBIENT;
		update.flags = 0;
		update.dither_mode = 0;

		upd_marker_data.update_marker = update.update_marker;

		mxc_epdc_fb_send_update(&update, &fb_data->info);

		/* Block on initial update */
		ret = mxc_epdc_fb_wait_update_complete(&upd_marker_data,
			&fb_data->info);
		if (ret < 0)
			dev_err(fb_data->dev,
				"Wait for initial update complete failed."
				" Error = 0x%x", ret);
	}
#endif


	k_fake_s1d13522_logo_progress((unsigned char*)gpbLOGO_vaddr);
}

static int mxc_epdc_fb_init_hw(struct fb_info *info)
{
	struct mxc_epdc_fb_data *fb_data = (struct mxc_epdc_fb_data *)info;
	int ret;

	DBG0_MSG("%s(%d)\n",__FUNCTION__,__LINE__);

#ifdef FW_IN_RAM //[
	if(gpbWF_vaddr) {
		#if 1 //[
		schedule_delayed_work(&fb_data->epdc_firmware_work,0);
		#else //][!
		mxc_epdc_fb_fw_handler(0,(void *)fb_data);
		#endif//]
		ret = 0;
	}
	else 
#endif //]FW_IN_RAM
	{

		/*
		 * Create fw search string based on ID string in selected videomode.
		 * Format is "imx/epdc/epdc_[panel string].fw"
		 */
		if (fb_data->cur_mode) {
			strcat(fb_data->fw_str, "imx/epdc/epdc_");
			strcat(fb_data->fw_str, fb_data->cur_mode->vmode->name);
			strcat(fb_data->fw_str, ".fw");
			dev_info(fb_data->dev,"epdc firmware name=\"%s\"",fb_data->fw_str);
		}

		fb_data->fw_default_load = false;
	
		ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					fb_data->fw_str, fb_data->dev, GFP_KERNEL,
					fb_data, mxc_epdc_fb_fw_handler);
		if (ret)
			dev_err(fb_data->dev,
				"Failed request_firmware_nowait err %d\n", ret);

	}

	return ret;
}

static ssize_t store_update(struct device *device,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct mxcfb_update_data update;
	struct fb_info *info = dev_get_drvdata(device);
	struct mxc_epdc_fb_data *fb_data = (struct mxc_epdc_fb_data *)info;

	if (strncmp(buf, "direct", 6) == 0)
		update.waveform_mode = fb_data->wv_modes.mode_du;
	else if (strncmp(buf, "gc16", 4) == 0)
		update.waveform_mode = fb_data->wv_modes.mode_gc16;
	else if (strncmp(buf, "gc4", 3) == 0)
		update.waveform_mode = fb_data->wv_modes.mode_gc4;

	/* Now, request full screen update */
	update.update_region.left = 0;
	update.update_region.width = fb_data->epdc_fb_var.xres;
	update.update_region.top = 0;
	update.update_region.height = fb_data->epdc_fb_var.yres;
	update.update_mode = UPDATE_MODE_FULL;
	update.temp = TEMP_USE_AMBIENT;
	update.update_marker = 0;
	update.flags = 0;

	mxc_epdc_fb_send_update(&update, info);

	return count;
}

static struct device_attribute fb_attrs[] = {
	__ATTR(update, S_IRUGO|S_IWUSR, NULL, store_update),
};

static const struct of_device_id imx_epdc_dt_ids[] = {
	{ .compatible = "fsl,imx7d-epdc", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_epdc_dt_ids);


static int mxc_epdc_fb_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pinctrl;
	struct mxc_epdc_fb_data *fb_data;
	struct resource *res;
	struct fb_info *info;
	char *options, *opt;
	char *panel_str = NULL;
	char name[] = "mxcepdcfb";
	struct fb_videomode *vmode;
	int xres_virt, yres_virt, buf_size;
	int xres_virt_rot, yres_virt_rot, pix_size_rot;
	struct fb_var_screeninfo *var_info;
	struct fb_fix_screeninfo *fix_info;
	struct pxp_config_data *pxp_conf;
	struct pxp_proc_data *proc_data;
	struct scatterlist *sg;
	struct update_data_list *upd_list;
	struct update_data_list *plist, *temp_list;
	int i;
	unsigned long x_mem_size = 0;
	u32 val;
	int irq;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *node;
	phandle phandle;
	u32 out_val[3];
	int enable_gpio;
	enum of_gpio_flags flag;
	unsigned short *wk_p;
	unsigned int dwSafeTicksTurnoffEP3V3;

	DBG_MSG("%s,%s(%d)\n",__FILE__,__FUNCTION__,__LINE__);

	if (!np)
		return -EINVAL;

	fb_data = (struct mxc_epdc_fb_data *)framebuffer_alloc(
			sizeof(struct mxc_epdc_fb_data), &pdev->dev);
	if (fb_data == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	fb_data->night_mode_test = -1; // -2=don't controll , -1=initial 1, 0=initail 0 , >0 night mode test .
	fb_data->pdev = pdev;
	fb_data->temp_auto_update_period = DEFAULT_TEMP_AUTO_UPDATE_PERIOD;

#if (GDEBUG>0) || defined(DEBUG)
	fb_data->verbose_lvl = 2;
#endif

	ret = of_property_read_u32_array(np, "epdc-ram", out_val, 3);
	if (ret) {
		dev_dbg(&pdev->dev, "no epdc-ram property found\n");
	} else {
		phandle = *out_val;

		node = of_find_node_by_phandle(phandle);
		if (!node) {
			dev_dbg(&pdev->dev, "not find gpr node by phandle\n");
			ret = PTR_ERR(node);
			goto out_fbdata;
		}
		fb_data->gpr = syscon_node_to_regmap(node);
		if (IS_ERR(fb_data->gpr)) {
			dev_err(&pdev->dev, "failed to get gpr regmap\n");
			ret = PTR_ERR(fb_data->gpr);
			goto out_fbdata;
		}
		of_node_put(node);
		fb_data->req_gpr = out_val[1];
		fb_data->req_bit = out_val[2];

		regmap_update_bits(fb_data->gpr, fb_data->req_gpr,
			1 << fb_data->req_bit, 0);
	}

	if (of_find_property(np, "en-gpios", NULL)) {
		enable_gpio = of_get_named_gpio_flags(np, "en-gpios", 0, &flag);
		if (enable_gpio == -EPROBE_DEFER) {
			dev_info(&pdev->dev, "GPIO requested is not"
				"here yet, deferring the probe\n");
			return -EPROBE_DEFER;
		}
		if (!gpio_is_valid(enable_gpio)) {
			dev_warn(&pdev->dev, "No dt property: en-gpios\n");
		} else {

			ret = devm_gpio_request_one(&pdev->dev,
						    enable_gpio,
						    (flag & OF_GPIO_ACTIVE_LOW)
						    ? GPIOF_OUT_INIT_LOW :
						    GPIOF_OUT_INIT_HIGH,
						    "en_pins");
			if (ret) {
				dev_err(&pdev->dev, "failed to request gpio"
					" %d: %d\n", enable_gpio, ret);
				return -EINVAL;
			}
		}
	}

#ifdef NIGHT_MODE_XON_TIMING //[
	if (of_find_property(np, "gpio_xon", NULL)) {
		fb_data->gpio_xon = of_get_named_gpio_flags(np, "gpio_xon", 0, &flag);
		if (fb_data->gpio_xon == -EPROBE_DEFER) {
			dev_info(&pdev->dev, "XON GPIO requested is not here yet\n");
		}
		else {
			if (!gpio_is_valid(fb_data->gpio_xon)) {
				dev_warn(&pdev->dev, "No dt property: gpio_xon\n");
			} else {

				ret = devm_gpio_request_one(&pdev->dev,
						    fb_data->gpio_xon,
						    (flag & OF_GPIO_ACTIVE_LOW)
						    ? GPIOF_OUT_INIT_LOW :
						    GPIOF_OUT_INIT_HIGH,
						    "en_pins");
				if (ret) {
					dev_err(&pdev->dev, "failed to request xon gpio"
						" %d: %d\n", fb_data->gpio_xon, ret);
					return -EINVAL;
				}
				else {
					dev_info(&pdev->dev, "Night mode XON ready !\n");
					fb_data->gpio_xon_desc = gpio_to_desc(fb_data->gpio_xon);
					fb_data->night_mode_test = 1;

					hrtimer_init(&fb_data->hrt_xon_on_ctrl,CLOCK_MONOTONIC,HRTIMER_MODE_REL);
					fb_data->hrt_xon_on_ctrl.function = _hrtint_xon_on_ctrl;

					hrtimer_init(&fb_data->hrt_xon_off_ctrl,CLOCK_MONOTONIC,HRTIMER_MODE_REL);
					fb_data->hrt_xon_off_ctrl.function = _hrtint_xon_off_ctrl;
				}

			}
		}
	}

	if (of_find_property(np, "xon_on_delay_us", NULL)) {
		if(of_property_read_u32(np,"xon_on_delay_us",&fb_data->xon_on_delay_us)) {
			dev_err(&pdev->dev, "xon_on_delay_us reading failed !\n");
		}
	}

	if (of_find_property(np, "xon_off_delay_us", NULL)) {
		if(of_property_read_u32(np,"xon_off_delay_us",&fb_data->xon_off_delay_us)) {
			dev_err(&pdev->dev, "xon_off_delay_us reading failed !\n");
			fb_data->xon_off_delay_us = 350000;
		}
	}

	if (of_find_property(np, "xon_off_day_delay_us", NULL)) {
		if(of_property_read_u32(np,"xon_off_day_delay_us",&fb_data->xon_off_day_delay_us)) {
			dev_err(&pdev->dev, "xon_off_day_delay_us reading failed !\n");
		}
	}
#endif //] NIGHT_MODE_XON_TIMING

	/* Get platform data and check validity */
	fb_data->pdata = &epdc_data;
	if ((fb_data->pdata == NULL) || (fb_data->pdata->num_modes < 1)
		|| (fb_data->pdata->epdc_mode == NULL)
		|| (fb_data->pdata->epdc_mode->vmode == NULL)) {
		ret = -EINVAL;
		goto out_fbdata;
	}

	if (fb_get_options(name, &options)) {
		ret = -ENODEV;
		goto out_fbdata;
	}

	fb_data->epdc_wb_mode = 1;
	fb_data->tce_prevent = 0;

	if (options)
		while ((opt = strsep(&options, ",")) != NULL) {
			if (!*opt)
				continue;

			if (!strncmp(opt, "bpp=", 4))
				fb_data->default_bpp =
					simple_strtoul(opt + 4, NULL, 0);
			else if (!strncmp(opt, "x_mem=", 6))
				x_mem_size = memparse(opt + 6, NULL);
			else if (!strncmp(opt, "tce_prevent", 11))
				fb_data->tce_prevent = 1;
			else
				panel_str = opt;
		}

	fb_data->dev = &pdev->dev;

	if (!fb_data->default_bpp)
		fb_data->default_bpp = default_bpp;

	/* Set default (first defined mode) before searching for a match */
#if 1
	if(1==gptHWCFG->m_val.bDisplayResolution) {
		printk("%s(%d):EPD 1024x758 \n",__FILE__,__LINE__);
		fb_data->cur_mode = &fb_data->pdata->epdc_mode[2];//
		fb_data->dwSafeTicksEP3V3 = 400;
	}
	else if(3==gptHWCFG->m_val.bDisplayResolution) {
		printk("%s(%d):EPD 1440x1080 \n",__FILE__,__LINE__);
		fb_data->cur_mode = &fb_data->pdata->epdc_mode[5];//
		fb_data->dwSafeTicksEP3V3 = 200;
	}
	else if(5==gptHWCFG->m_val.bDisplayResolution) {
		if(3==gptHWCFG->m_val.bPanelModel) {
			// ED060XHC
			printk("%s(%d):ED060XHC 1448x1072 \n",__FILE__,__LINE__);
			fb_data->cur_mode = &fb_data->pdata->epdc_mode[17];//
			fb_data->dwSafeTicksEP3V3 = 800;
		}
		else {
			printk("%s(%d):EPD 1448x1072 \n",__FILE__,__LINE__);
			fb_data->cur_mode = &fb_data->pdata->epdc_mode[6];//
			fb_data->dwSafeTicksEP3V3 = 800;
		}
	}
	else if(6==gptHWCFG->m_val.bDisplayResolution) {
		printk("%s(%d):EPD 1600x1200 \n",__FILE__,__LINE__);
		switch (gptHWCFG->m_val.bDisplayBusWidth) {
		case 1: // 16 bits .
		case 3:
			fb_data->cur_mode = &fb_data->pdata->epdc_mode[7];//
			break;
		case 0:
		case 2: // 8 bits mirror 
		default: // PENG060D  
			fb_data->cur_mode = &fb_data->pdata->epdc_mode[11];//
			break;
		}
		fb_data->dwSafeTicksEP3V3 = 400;
	}
	else if(8==gptHWCFG->m_val.bDisplayResolution) {
#ifdef ED078KH1_75HZ //[
		printk("%s(%d):EPD 1872x1404 75Hz\n",__FILE__,__LINE__);
		fb_data->cur_mode = &fb_data->pdata->epdc_mode[12];//
#else //][ED078KH1_75HZ
		printk("%s(%d):EPD 1872x1404 \n",__FILE__,__LINE__);
		fb_data->cur_mode = &fb_data->pdata->epdc_mode[9];//
#endif //]ED078KH1_75HZ
		fb_data->dwSafeTicksEP3V3 = 900;
	}
	else if(2==gptHWCFG->m_val.bDisplayResolution) {
		printk("%s(%d):EPD 1024x768 \n",__FILE__,__LINE__);
		fb_data->cur_mode = &fb_data->pdata->epdc_mode[3];//
		fb_data->dwSafeTicksEP3V3 = 400;
	}
	else if(14==gptHWCFG->m_val.bDisplayResolution) {
		printk("%s(%d):EPD 1920x1440 \n",__FILE__,__LINE__);
		fb_data->cur_mode = &fb_data->pdata->epdc_mode[13];//
		fb_data->dwSafeTicksEP3V3 = 900;
	}
	else if(15==gptHWCFG->m_val.bDisplayResolution) {
		printk("%s(%d):EPD 1264x1680 \n",__FILE__,__LINE__);
		fb_data->cur_mode = &fb_data->pdata->epdc_mode[15];//
		fb_data->dwSafeTicksEP3V3 = 900;
	}
	else if(16==gptHWCFG->m_val.bDisplayResolution) {
		printk("%s(%d):EPD 1680x1264 \n",__FILE__,__LINE__);
		fb_data->cur_mode = &fb_data->pdata->epdc_mode[16];//
		fb_data->dwSafeTicksEP3V3 = 900;
	}
	else {
		if (gptHWCFG->m_val.bPCB>=85) {
			fb_data->cur_mode = &fb_data->pdata->epdc_mode[14];
			fb_data->dwSafeTicksEP3V3 = 400;
		}
		else {
			fb_data->cur_mode = &fb_data->pdata->epdc_mode[8];
			fb_data->dwSafeTicksEP3V3 = 400;
		}
	}
#else
	fb_data->cur_mode = &fb_data->pdata->epdc_mode[0];
#endif

#ifdef CONFIG_SOC_IMX6SLL //[
	if(cpu_is_imx6sll()) {
		extern void imx6sll_epdc_pre_sel(int iSelFreq);

		if( fb_data->cur_mode->vmode->pixclock==135000000 ) //E70K02, special case
		{
			do {
				// := 135MHz
				// original parent clock is 320MHz . 
				// change the parent clock to 540MHz . 
				printk("%s():changing the epdc_pre_sel parent clock to 540MHz \n",__FUNCTION__);
				imx6sll_epdc_pre_sel(540000000);
			} while(0);
		}
		else if( fb_data->cur_mode->vmode->pixclock>=132500000 && 
			 fb_data->cur_mode->vmode->pixclock<=133500000) 
		{
			do {
				// := 133MHz
				// original parent clock is 320MHz . 
				// change the parent clock to 400MHz . 
				printk("%s():changing the epdc_pre_sel parent clock to 400MHz \n",__FUNCTION__);
				imx6sll_epdc_pre_sel(400000000); 
			} while(0);
		}
		else if( fb_data->cur_mode->vmode->pixclock>=115000000 && 
			 fb_data->cur_mode->vmode->pixclock<=125000000) 
		{
			do {
				// := 120MHz
				// original parent clock is 320MHz . 
				// change the parent clock to 480MHz . 
				printk("%s():changing the epdc_pre_sel parent clock to 480MHz \n",__FUNCTION__);
				imx6sll_epdc_pre_sel(480000000); 
			} while(0);
		}
		else
		if( fb_data->cur_mode->vmode->pixclock>=87500000 && 
			 fb_data->cur_mode->vmode->pixclock<=88500000) 
		{
			do {
				// := 88MHz
				// original parent clock is 320MHz . 
				// change the parent clock to 528MHz . 
				printk("%s():changing the epdc_pre_sel parent clock to 528MHz \n",__FUNCTION__);
				imx6sll_epdc_pre_sel(528000000);
			} while(0);
		}
	}
#endif //] CONFIG_SOC_IMX6SLL


	if (!of_property_read_u32(np, "safe-ticks-turnoff-ep3v3", &dwSafeTicksTurnoffEP3V3))
	{
		// safe-ticks-turnoff-ep3v3 property .
		fb_data->dwSafeTicksEP3V3 = dwSafeTicksTurnoffEP3V3;
	}
	fb_data->dwJiffies_To_TurnOFF_EP3V3 = jiffies;

	if (panel_str)
		for (i = 0; i < fb_data->pdata->num_modes; i++)
			if (!strcmp(fb_data->pdata->epdc_mode[i].vmode->name,
						panel_str)) {
				fb_data->cur_mode =
					&fb_data->pdata->epdc_mode[i];
				break;
			}

	vmode = fb_data->cur_mode->vmode;

	platform_set_drvdata(pdev, fb_data);
	info = &fb_data->info;
	info->par = fb_data;

	/* Allocate color map for the FB */
	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret)
		goto out_fbdata;

	dev_info(&pdev->dev, "resolution %dx%d, bpp %d\n",
		vmode->xres, vmode->yres, fb_data->default_bpp);

	/*
	 * GPU alignment restrictions dictate framebuffer parameters:
	 * - 32-byte alignment for buffer width
	 * - 128-byte alignment for buffer height
	 * => 4K buffer alignment for buffer start
	 */
	xres_virt = ALIGN(vmode->xres, 32);
	yres_virt = ALIGN(vmode->yres, 128);
	fb_data->max_pix_size = PAGE_ALIGN(xres_virt * yres_virt);

	/*
	 * Have to check to see if aligned buffer size when rotated
	 * is bigger than when not rotated, and use the max
	 */
	xres_virt_rot = ALIGN(vmode->yres, 32);
	yres_virt_rot = ALIGN(vmode->xres, 128);
	pix_size_rot = PAGE_ALIGN(xres_virt_rot * yres_virt_rot);
	fb_data->max_pix_size = (fb_data->max_pix_size > pix_size_rot) ?
				fb_data->max_pix_size : pix_size_rot;

	buf_size = fb_data->max_pix_size * fb_data->default_bpp/8;

	/* Compute the number of screens needed based on X memory requested */
	if (x_mem_size > 0) {
		fb_data->num_screens = DIV_ROUND_UP(x_mem_size, buf_size);
		if (fb_data->num_screens < NUM_SCREENS_MIN)
			fb_data->num_screens = NUM_SCREENS_MIN;
		else if (buf_size * fb_data->num_screens > SZ_16M)
			fb_data->num_screens = SZ_16M / buf_size;
	} else
		fb_data->num_screens = NUM_SCREENS_MIN;

	if(0==gptHWCFG->m_val.bUIStyle) {
		fb_data->map_size = buf_size * fb_data->num_screens * 2;
	}
	else {
		fb_data->map_size = buf_size * fb_data->num_screens;
	}
	dev_dbg(&pdev->dev, "memory to allocate: %d\n", fb_data->map_size);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		ret = -ENODEV;
		goto out_cmap;
	}

	epdc_v2_base = devm_ioremap_resource(&pdev->dev, res);
	if (epdc_v2_base == NULL) {
		ret = -ENOMEM;
		goto out_cmap;
	}

	/* Allocate FB memory */
	info->screen_base = dma_alloc_writecombine(&pdev->dev,
						  fb_data->map_size,
						  &fb_data->phys_start,
						  GFP_DMA | GFP_KERNEL);

	if (info->screen_base == NULL) {
		ret = -ENOMEM;
		goto out_cmap;
	}
	dev_dbg(&pdev->dev, "allocated at %p:0x%x\n", info->screen_base,
		fb_data->phys_start);

	var_info = &info->var;
	var_info->activate = FB_ACTIVATE_TEST;
	var_info->bits_per_pixel = fb_data->default_bpp;
	var_info->xres = vmode->xres;
	var_info->yres = vmode->yres;
	var_info->xres_virtual = xres_virt;
	/* Additional screens allow for panning  and buffer flipping */
	var_info->yres_virtual = yres_virt * fb_data->num_screens;

	var_info->pixclock = vmode->pixclock;
	var_info->left_margin = vmode->left_margin;
	var_info->right_margin = vmode->right_margin;
	var_info->upper_margin = vmode->upper_margin;
	var_info->lower_margin = vmode->lower_margin;
	var_info->hsync_len = vmode->hsync_len;
	var_info->vsync_len = vmode->vsync_len;
	var_info->vmode = FB_VMODE_NONINTERLACED;

	switch (fb_data->default_bpp) {
	case 32:
	case 24:
		var_info->red.offset = 16;
		var_info->red.length = 8;
		var_info->green.offset = 8;
		var_info->green.length = 8;
		var_info->blue.offset = 0;
		var_info->blue.length = 8;
		break;

	case 16:
		var_info->red.offset = 11;
		var_info->red.length = 5;
		var_info->green.offset = 5;
		var_info->green.length = 6;
		var_info->blue.offset = 0;
		var_info->blue.length = 5;
		break;

	case 8:
		/*
		 * For 8-bit grayscale, R, G, and B offset are equal.
		 *
		 */
		var_info->grayscale = GRAYSCALE_8BIT;

		var_info->red.length = 8;
		var_info->red.offset = 0;
		var_info->red.msb_right = 0;
		var_info->green.length = 8;
		var_info->green.offset = 0;
		var_info->green.msb_right = 0;
		var_info->blue.length = 8;
		var_info->blue.offset = 0;
		var_info->blue.msb_right = 0;
		break;

	default:
		dev_err(&pdev->dev, "unsupported bitwidth %d\n",
			fb_data->default_bpp);
		ret = -EINVAL;
		goto out_dma_fb;
	}

	fix_info = &info->fix;

	strcpy(fix_info->id, "mxc_epdc_fb");
	fix_info->type = FB_TYPE_PACKED_PIXELS;
	fix_info->visual = FB_VISUAL_TRUECOLOR;
	fix_info->xpanstep = 0;
	fix_info->ypanstep = 0;
	fix_info->ywrapstep = 0;
	fix_info->accel = FB_ACCEL_NONE;
	fix_info->smem_start = fb_data->phys_start;
	if(0==gptHWCFG->m_val.bUIStyle) {
		fix_info->smem_len = fb_data->map_size>>1;
	}
	else {
		fix_info->smem_len = fb_data->map_size;
	}
	fix_info->ypanstep = 0;

	fb_data->native_width = vmode->xres;
	fb_data->native_height = vmode->yres;

	info->fbops = &mxc_epdc_fb_ops;
	info->var.activate = FB_ACTIVATE_NOW;
	info->pseudo_palette = fb_data->pseudo_palette;
	info->screen_size = info->fix.smem_len;
	info->flags = FBINFO_FLAG_DEFAULT;

	mxc_epdc_fb_set_fix(info);

	fb_data->auto_mode = AUTO_UPDATE_MODE_REGION_MODE;
	fb_data->upd_scheme = UPDATE_SCHEME_QUEUE_AND_MERGE;

	/* Initialize our internal copy of the screeninfo */
	fb_data->epdc_fb_var = *var_info;
	fb_data->fb_offset = 0;
	fb_data->eof_sync_period = 0;

	fb_data->epdc_clk_axi = clk_get(fb_data->dev, "epdc_axi");
	if (IS_ERR(fb_data->epdc_clk_axi)) {
		dev_err(&pdev->dev, "Unable to get EPDC AXI clk."
			"err = %d\n", (int)fb_data->epdc_clk_axi);
		ret = -ENODEV;
		goto out_dma_fb;
	}
	fb_data->epdc_clk_pix = clk_get(fb_data->dev, "epdc_pix");
	if (IS_ERR(fb_data->epdc_clk_pix)) {
		dev_err(&pdev->dev, "Unable to get EPDC pix clk."
			"err = %d\n", (int)fb_data->epdc_clk_pix);
		ret = -ENODEV;
		goto out_dma_fb;
	}

	clk_prepare_enable(fb_data->epdc_clk_axi);
	clk_prepare_enable(fb_data->epdc_clk_pix);
	val = __raw_readl(EPDC_VERSION);

	if( QOS_SOC_MX6SLL() || QOS_SOC_MX6SL() ) {
		_epdc_qos_setup();
	}

	clk_disable_unprepare(fb_data->epdc_clk_pix);
	clk_disable_unprepare(fb_data->epdc_clk_axi);
	fb_data->rev = ((val & EPDC_VERSION_MAJOR_MASK) >>
				EPDC_VERSION_MAJOR_OFFSET) * 10
			+ ((val & EPDC_VERSION_MINOR_MASK) >>
				EPDC_VERSION_MINOR_OFFSET);
	dev_info(&pdev->dev, "EPDC version = %d\n", fb_data->rev);

	if (fb_data->rev < 20) {
		fb_data->num_luts = EPDC_V1_NUM_LUTS;
		fb_data->max_num_updates = EPDC_V1_MAX_NUM_UPDATES;
	} else {
		fb_data->num_luts = EPDC_V2_NUM_LUTS;
		fb_data->max_num_updates = EPDC_V2_MAX_NUM_UPDATES;
		if (vmode->xres > EPDC_V2_MAX_UPDATE_WIDTH)
			fb_data->restrict_width = true;
	}
	fb_data->max_num_buffers = EPDC_MAX_NUM_BUFFERS;

	/*
	 * Initialize lists for pending updates,
	 * active update requests, update collisions,
	 * and freely available updates.
	 */
	INIT_LIST_HEAD(&fb_data->upd_pending_list);
	INIT_LIST_HEAD(&fb_data->upd_buf_queue);
	INIT_LIST_HEAD(&fb_data->upd_buf_free_list);
	INIT_LIST_HEAD(&fb_data->upd_buf_collision_list);

	/* Allocate update buffers and add them to the list */
	for (i = 0; i < fb_data->max_num_updates; i++) {
		upd_list = kzalloc(sizeof(*upd_list), GFP_KERNEL);
		if (upd_list == NULL) {
			ret = -ENOMEM;
			goto out_upd_lists;
		}

		/* Add newly allocated buffer to free list */
		list_add(&upd_list->list, &fb_data->upd_buf_free_list);
	}

	fb_data->virt_addr_updbuf =
		kzalloc(sizeof(void *) * fb_data->max_num_buffers, GFP_KERNEL);
	fb_data->phys_addr_updbuf =
		kzalloc(sizeof(dma_addr_t) * fb_data->max_num_buffers,
			GFP_KERNEL);
	for (i = 0; i < fb_data->max_num_buffers; i++) {
		/*
		 * Allocate memory for PxP output buffer.
		 * Each update buffer is 1 byte per pixel, and can
		 * be as big as the full-screen frame buffer
		 */
		fb_data->virt_addr_updbuf[i] =
			kmalloc(fb_data->max_pix_size, GFP_KERNEL);
		fb_data->phys_addr_updbuf[i] =
			virt_to_phys(fb_data->virt_addr_updbuf[i]);
		if (fb_data->virt_addr_updbuf[i] == NULL) {
			ret = -ENOMEM;
			goto out_upd_buffers;
		}

		dev_dbg(fb_data->info.device, "allocated %d bytes @ 0x%08X\n",
			fb_data->max_pix_size, fb_data->phys_addr_updbuf[i]);
	}

	/* Counter indicating which update buffer should be used next. */
	fb_data->upd_buffer_num = 0;

	/*
	 * Allocate memory for PxP SW workaround buffer
	 * These buffers are used to hold copy of the update region,
	 * before sending it to PxP for processing.
	 */
	fb_data->virt_addr_copybuf =
	    dma_alloc_coherent(fb_data->info.device, fb_data->max_pix_size*2,
			       &fb_data->phys_addr_copybuf,
			       GFP_DMA | GFP_KERNEL);
	if (fb_data->virt_addr_copybuf == NULL) {
		ret = -ENOMEM;
		goto out_upd_buffers;
	}

#ifdef EPDC_V2_ENABLE_HW_DITHER //[
	fb_data->virt_addr_y4 =
	    dma_alloc_coherent(fb_data->info.device, fb_data->max_pix_size*2,
			       &fb_data->phys_addr_y4,
			       GFP_DMA | GFP_KERNEL);
	if (fb_data->virt_addr_y4 == NULL) {
		ret = -ENOMEM;
		goto out_upd_buffers;
	}
#endif //]EPDC_V2_ENABLE_HW_DITHER

	fb_data->virt_addr_y4c =
	    dma_alloc_coherent(fb_data->info.device, fb_data->max_pix_size*2,
			       &fb_data->phys_addr_y4c,
			       GFP_DMA | GFP_KERNEL);
	if (fb_data->virt_addr_y4c == NULL) {
		ret = -ENOMEM;
		goto out_upd_buffers;
	}

	fb_data->virt_addr_black =
	    dma_alloc_coherent(fb_data->info.device, fb_data->max_pix_size*2,
			       &fb_data->phys_addr_black,
			       GFP_DMA | GFP_KERNEL);
	if (fb_data->virt_addr_black == NULL) {
		ret = -ENOMEM;
		goto out_upd_buffers;
	}

	fb_data->working_buffer_size = vmode->yres * vmode->xres * 2;

	/* Allocate memory for EPDC working buffer */
	fb_data->working_buffer_virt =
	    dma_alloc_coherent(&pdev->dev, fb_data->working_buffer_size,
			       &fb_data->working_buffer_phys,
			       GFP_DMA | GFP_KERNEL);
	if (fb_data->working_buffer_virt == NULL) {
		dev_err(&pdev->dev, "Can't allocate mem for working buf!\n");
		ret = -ENOMEM;
		goto out_copybuffer;
	}

	/* initialize the working buffer */
	wk_p = (unsigned short *)fb_data->working_buffer_virt;
	for (i = 0; i < fb_data->cur_mode->vmode->xres *
			fb_data->cur_mode->vmode->yres; i++) {
		*wk_p = 0x00F0;
		wk_p++;
	}

	fb_data->tmp_working_buffer_virt =
	    dma_alloc_coherent(&pdev->dev, fb_data->working_buffer_size,
			       &fb_data->tmp_working_buffer_phys,
			       GFP_DMA | GFP_KERNEL);
	if (fb_data->tmp_working_buffer_virt == NULL) {
		dev_err(&pdev->dev, "Can't allocate mem for tmp working buf!\n");
		ret = -ENOMEM;
		goto out_copybuffer;
	}

	/* Initialize EPDC pins */
	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		dev_err(&pdev->dev, "can't get/select pinctrl\n");
		ret = PTR_ERR(pinctrl);
		goto out_copybuffer;
	}

	fb_data->in_init = false;

	fb_data->hw_ready = false;
	fb_data->hw_initializing = false;

	/*
	 * Set default waveform mode values.
	 * Should be overwritten via ioctl.
	 */
	fb_data->wv_modes.mode_init = 0;
	fb_data->wv_modes.mode_du = 1;
	fb_data->wv_modes.mode_gc4 = 3;
	fb_data->wv_modes.mode_gc8 = 2;
	fb_data->wv_modes.mode_gc16 = 2;
	fb_data->wv_modes.mode_gc32 = 2;
	fb_data->wv_modes_update = true;

	/* Initialize marker list */
	INIT_LIST_HEAD(&fb_data->full_marker_list);

	/* Initialize all LUTs to inactive */
	fb_data->lut_update_order =
		kzalloc(fb_data->num_luts * sizeof(u32 *), GFP_KERNEL);
	for (i = 0; i < fb_data->num_luts; i++)
		fb_data->lut_update_order[i] = 0;

	fb_data->latest_update_region.left = (__u32)-1;
	fb_data->latest_update_region.top = (__u32)-1;
	fb_data->latest_update_region.width = (__u32)-1;
	fb_data->latest_update_region.height = (__u32)-1;
	INIT_DELAYED_WORK(&fb_data->epdc_reupdate_work, epdc_reupdate_work_func);
	
	INIT_DELAYED_WORK(&fb_data->epdc_done_work, epdc_done_work_func);
	fb_data->epdc_submit_workqueue = alloc_workqueue("EPDC Submit",
					WQ_MEM_RECLAIM | WQ_HIGHPRI |
					WQ_CPU_INTENSIVE | WQ_UNBOUND, 1);
	INIT_WORK(&fb_data->epdc_submit_work, epdc_submit_work_func);
	fb_data->epdc_intr_workqueue = alloc_workqueue("EPDC Interrupt",
					WQ_MEM_RECLAIM | WQ_HIGHPRI |
					WQ_CPU_INTENSIVE | WQ_UNBOUND, 1);
	INIT_WORK(&fb_data->epdc_intr_work, epdc_intr_work_func);

#ifdef FW_IN_RAM//[
	//fb_data->epdc_firmware_workqueue = alloc_workqueue("EPDC Firmware",0, 1);
	//INIT_WORK(&fb_data->epdc_firmware_work, epdc_firmware_work_func);
	sema_init(&fb_data->firmware_work_lock,1);
	INIT_DELAYED_WORK(&fb_data->epdc_firmware_work, epdc_firmware_work_func);
#endif //]FW_IN_RAM

	/* Retrieve EPDC IRQ num */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "cannot get IRQ resource\n");
		ret = -ENODEV;
		goto out_dma_work_buf;
	}
	fb_data->epdc_irq = irq;

	/* Register IRQ handler */
	ret = devm_request_irq(&pdev->dev, fb_data->epdc_irq,
				mxc_epdc_irq_handler, 0, "epdc", fb_data);
	if (ret) {
		dev_err(&pdev->dev, "request_irq (%d) failed with error %d\n",
			fb_data->epdc_irq, ret);
		ret = -ENODEV;
		goto out_dma_work_buf;
	}

	info->fbdefio = &mxc_epdc_fb_defio;
#ifdef CONFIG_FB_MXC_EINK_AUTO_UPDATE_MODE
	fb_deferred_io_init(info);
#endif

	switch(gptHWCFG->m_val.bDisplayCtrl) {
	case 19: // mx6sl+SY7636
	case 21: // mx6ull+SY7636
	case 20: // mx6sll+SY7636
	case 22: // mx6dl+SY7636
		gpszDisplayReg = gszDisplayReg_sy7636;
		gpszVcomReg = gszVcomReg_sy7636;
		gpszVP3V3Reg = gszVP3V3Reg_sy7636;
		gpszTMSTReg = gszTMSTReg_sy7636;
		break;
	case 26: // mx6sll+JD9930
	case 27: // mx6ull+JD9930
		gpszDisplayReg = gszDisplayReg_jd9930;
		gpszVcomReg = gszVcomReg_jd9930;
		gpszVP3V3Reg = gszVP3V3Reg_jd9930;
		gpszTMSTReg = gszTMSTReg_jd9930;
		break;
	default:
		break;
	}

	/* get pmic regulators */
	fb_data->display_regulator = devm_regulator_get(&pdev->dev, gpszDisplayReg);
	if (IS_ERR(fb_data->display_regulator)) {
		dev_err(&pdev->dev, "Unable to get display PMIC regulator."
			"err = 0x%x\n", (int)fb_data->display_regulator);
		ret = -ENODEV;
		goto out_dma_work_buf;
	}
	fb_data->vcom_regulator = devm_regulator_get(&pdev->dev, gpszVcomReg);
	if (IS_ERR(fb_data->vcom_regulator)) {
		dev_err(&pdev->dev, "Unable to get VCOM regulator."
			"err = 0x%x\n", (int)fb_data->vcom_regulator);
		ret = -ENODEV;
		goto out_dma_work_buf;
	}
	fb_data->v3p3_regulator = devm_regulator_get(&pdev->dev, gpszVP3V3Reg);
	if (IS_ERR(fb_data->v3p3_regulator)) {
		dev_err(&pdev->dev, "Unable to get V3P3 regulator."
			"err = 0x%x\n", (int)fb_data->v3p3_regulator);
		ret = -ENODEV;
		goto out_dma_work_buf;
	}
	fb_data->tmst_regulator = devm_regulator_get(&pdev->dev, gpszTMSTReg);
	if (IS_ERR(fb_data->tmst_regulator)) {
		dev_info(&pdev->dev, "Unable to get TMST regulator."
			"err = 0x%x\n", (int)fb_data->tmst_regulator);
	}

	/* save the nominal vcom value */
	fb_data->wfm = 0; /* initial waveform mode should be INIT */
	vcom_nominal = regulator_get_voltage(fb_data->vcom_regulator); /* save the vcom_nominal value in uV */

	if (device_create_file(info->dev, &fb_attrs[0]))
		dev_err(&pdev->dev, "Unable to create file from fb_attrs\n");

	fb_data->cur_update = NULL;

	mutex_init(&fb_data->queue_mutex);
	mutex_init(&fb_data->pxp_mutex);
	mutex_init(&fb_data->power_mutex);

	/*
	 * Fill out PxP config data structure based on FB info and
	 * processing tasks required
	 */
	pxp_conf = &fb_data->pxp_conf;
	proc_data = &pxp_conf->proc_data;

	/* Initialize non-channel-specific PxP parameters */
	proc_data->drect.left = proc_data->srect.left = 0;
	proc_data->drect.top = proc_data->srect.top = 0;
	proc_data->drect.width = proc_data->srect.width = fb_data->info.var.xres;
	proc_data->drect.height = proc_data->srect.height = fb_data->info.var.yres;
	proc_data->scaling = 0;
	proc_data->hflip = 0;
	proc_data->vflip = 0;
	proc_data->rotate = 0;
	proc_data->bgcolor = 0;
	proc_data->overlay_state = 0;
	proc_data->lut_transform = PXP_LUT_NONE;
	proc_data->lut_map = NULL;

	/*
	 * We initially configure PxP for RGB->YUV conversion,
	 * and only write out Y component of the result.
	 */

	/*
	 * Initialize S0 channel parameters
	 * Parameters should match FB format/width/height
	 */
	pxp_conf->s0_param.pixel_fmt = PXP_PIX_FMT_RGB565;
	pxp_conf->s0_param.width = fb_data->info.var.xres_virtual;
	pxp_conf->s0_param.height = fb_data->info.var.yres;
	pxp_conf->s0_param.color_key = -1;
	pxp_conf->s0_param.color_key_enable = false;

	/*
	 * Initialize OL0 channel parameters
	 * No overlay will be used for PxP operation
	 */
	for (i = 0; i < 8; i++) {
		pxp_conf->ol_param[i].combine_enable = false;
		pxp_conf->ol_param[i].width = 0;
		pxp_conf->ol_param[i].height = 0;
		pxp_conf->ol_param[i].pixel_fmt = PXP_PIX_FMT_RGB565;
		pxp_conf->ol_param[i].color_key_enable = false;
		pxp_conf->ol_param[i].color_key = -1;
		pxp_conf->ol_param[i].global_alpha_enable = false;
		pxp_conf->ol_param[i].global_alpha = 0;
		pxp_conf->ol_param[i].local_alpha_enable = false;
	}

	/*
	 * Initialize Output channel parameters
	 * Output is Y-only greyscale
	 * Output width/height will vary based on update region size
	 */
	pxp_conf->out_param.width = fb_data->info.var.xres;
	pxp_conf->out_param.height = fb_data->info.var.yres;
	pxp_conf->out_param.stride = pxp_conf->out_param.width;
	pxp_conf->out_param.pixel_fmt = PXP_PIX_FMT_GREY;

	/* Initialize color map for conversion of 8-bit gray pixels */
	fb_data->pxp_conf.proc_data.lut_map = kmalloc(256, GFP_KERNEL);
	if (fb_data->pxp_conf.proc_data.lut_map == NULL) {
		dev_err(&pdev->dev, "Can't allocate mem for lut map!\n");
		ret = -ENOMEM;
		goto out_dma_work_buf;
	}
	for (i = 0; i < 256; i++)
		fb_data->pxp_conf.proc_data.lut_map[i] = i;

	fb_data->pxp_conf.proc_data.lut_map_updated = true;

	/*
	 * Ensure this is set to NULL here...we will initialize pxp_chan
	 * later in our thread.
	 */
	fb_data->pxp_chan = NULL;

	/* Initialize Scatter-gather list containing 2 buffer addresses. */
	sg = fb_data->sg;
	sg_init_table(sg, SG_NUM);

	/*
	 * For use in PxP transfers:
	 * sg[0] holds the FB buffer pointer
	 * sg[1] holds the Output buffer pointer (configured before TX request)
	 */
	sg_dma_address(&sg[0]) = info->fix.smem_start;
	sg_set_page(&sg[0], virt_to_page(info->screen_base),
		    info->fix.smem_len, offset_in_page(info->screen_base));

	fb_data->order_cnt = 0;
	fb_data->waiting_for_wb = false;
	fb_data->waiting_for_lut = false;
	fb_data->waiting_for_lut15 = false;
	fb_data->waiting_for_idle = false;
	fb_data->blank = FB_BLANK_UNBLANK;
	fb_data->power_state = POWER_STATE_OFF;
	fb_data->powering_down = false;
	fb_data->wait_for_powerdown = false;
	fb_data->updates_active = false;
	//fb_data->pwrdown_delay = 0;
	fb_data->pwrdown_delay = 20;
	
	fake_s1d13522_parse_epd_cmdline();

	/* Register FB */
	ret = register_framebuffer(info);
	if (ret) {
		dev_err(&pdev->dev,
			"register_framebuffer failed with error %d\n", ret);
		goto out_lutmap;
	}

	g_fb_data = fb_data;

	pm_runtime_enable(fb_data->dev);

#ifdef DEFAULT_PANEL_HW_INIT
	ret = mxc_epdc_fb_init_hw((struct fb_info *)fb_data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize HW!\n");
	}
#endif


	if( QOS_SOC_MX6ULL() ) {
		_epdc_qos_setup();
	}
#ifdef TCE_UNDERRUN_PREVENT_WORKFUNC//[
	INIT_DELAYED_WORK(&fb_data->tce_safe_work, tce_safe_work_func);
	fb_data->tce_safe_ms = TCE_UNDERRUN_PREVENT_WORKFUNC_BUSYMS;
	fb_data->tce_safe_freems = TCE_UNDERRUN_PREVENT_WORKFUNC_FREEMS;
	fb_data->tce_safe_loops = TCE_UNDERRUN_PREVENT_WORKFUNC_LOOPCNT;
	fb_data->tce_safe_work_running = 0;
	fb_data->tce_safe_lock=__SPIN_LOCK_UNLOCKED(fb_data->tce_safe_lock);
#endif //]TCE_UNDERRUN_PREVENT_WORKFUNC

	k_fake_s1d13522_init((unsigned char*)gpbLOGO_vaddr);
	goto out;

out_lutmap:
	kfree(fb_data->pxp_conf.proc_data.lut_map);
out_dma_work_buf:
	dma_free_writecombine(&pdev->dev, fb_data->working_buffer_size,
		fb_data->working_buffer_virt, fb_data->working_buffer_phys);
out_copybuffer:
	dma_free_writecombine(&pdev->dev, fb_data->max_pix_size*2,
			      fb_data->virt_addr_copybuf,
			      fb_data->phys_addr_copybuf);
out_upd_buffers:
	for (i = 0; i < fb_data->max_num_buffers; i++)
		if (fb_data->virt_addr_updbuf[i] != NULL)
			kfree(fb_data->virt_addr_updbuf[i]);
	if (fb_data->virt_addr_updbuf != NULL)
		kfree(fb_data->virt_addr_updbuf);
	if (fb_data->phys_addr_updbuf != NULL)
		kfree(fb_data->phys_addr_updbuf);
out_upd_lists:
	list_for_each_entry_safe(plist, temp_list, &fb_data->upd_buf_free_list,
			list) {
		list_del(&plist->list);
		kfree(plist);
	}
out_dma_fb:
	dma_free_writecombine(&pdev->dev, fb_data->map_size, info->screen_base,
			      fb_data->phys_start);

out_cmap:
	fb_dealloc_cmap(&info->cmap);
out_fbdata:
	kfree(fb_data);
out:
	return ret;
}

static int mxc_epdc_fb_remove(struct platform_device *pdev)
{
	struct update_data_list *plist, *temp_list;
	struct mxc_epdc_fb_data *fb_data = platform_get_drvdata(pdev);
	int i;

	/* restore the vcom_nominal value */
	regulator_set_voltage(fb_data->vcom_regulator, vcom_nominal, vcom_nominal);

	mxc_epdc_fb_blank(FB_BLANK_POWERDOWN, &fb_data->info);

	flush_workqueue(fb_data->epdc_submit_workqueue);
	destroy_workqueue(fb_data->epdc_submit_workqueue);
#ifdef FW_IN_RAM //[
	flush_workqueue(fb_data->epdc_firmware_workqueue);
	destroy_workqueue(fb_data->epdc_firmware_workqueue);
#endif //]FW_IN_RAM

	unregister_framebuffer(&fb_data->info);

	for (i = 0; i < fb_data->max_num_buffers; i++)
		if (fb_data->virt_addr_updbuf[i] != NULL)
			kfree(fb_data->virt_addr_updbuf[i]);
	if (fb_data->virt_addr_updbuf != NULL)
		kfree(fb_data->virt_addr_updbuf);
	if (fb_data->phys_addr_updbuf != NULL)
		kfree(fb_data->phys_addr_updbuf);
	if (fb_data->waveform_acd_buffer != NULL)
		kfree(fb_data->waveform_acd_buffer);
	if (fb_data->waveform_vcd_buffer != NULL)
		kfree(fb_data->waveform_vcd_buffer);

	dma_free_writecombine(&pdev->dev, fb_data->working_buffer_size,
				fb_data->working_buffer_virt,
				fb_data->working_buffer_phys);
#ifdef FW_IN_RAM //[
#else //][! FW_IN_RAM
	if (fb_data->waveform_buffer_virt != NULL)
		dma_free_writecombine(&pdev->dev, fb_data->waveform_buffer_size,
				fb_data->waveform_buffer_virt,
				fb_data->waveform_buffer_phys);
#endif //] FW_IN_RAM
	if (fb_data->virt_addr_copybuf != NULL)
		dma_free_writecombine(&pdev->dev, fb_data->max_pix_size*2,
				fb_data->virt_addr_copybuf,
				fb_data->phys_addr_copybuf);

	/* release gen2 waveform buffers */
	if (fb_data->waveform_vcd_buffer) kfree (fb_data->waveform_vcd_buffer);
	if (fb_data->waveform_acd_buffer) kfree (fb_data->waveform_acd_buffer);
	if (fb_data->waveform_xwi_buffer) kfree (fb_data->waveform_xwi_buffer);
	if (fb_data->waveform_xwi_string) kfree (fb_data->waveform_xwi_string);

	list_for_each_entry_safe(plist, temp_list, &fb_data->upd_buf_free_list,
			list) {
		list_del(&plist->list);
		kfree(plist);
	}
#ifdef CONFIG_FB_MXC_EINK_AUTO_UPDATE_MODE
	fb_deferred_io_cleanup(&fb_data->info);
#endif

	dma_free_writecombine(&pdev->dev, fb_data->map_size, fb_data->info.screen_base,
			      fb_data->phys_start);

	/* Release PxP-related resources */
	if (fb_data->pxp_chan != NULL)
		dma_release_channel(&fb_data->pxp_chan->dma_chan);

	fb_dealloc_cmap(&fb_data->info.cmap);

	framebuffer_release(&fb_data->info);
	if (!IS_ERR_OR_NULL(fb_data->gpr))
		regmap_update_bits(fb_data->gpr, fb_data->req_gpr,
			1 << fb_data->req_bit, 1 << fb_data->req_bit);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mxc_epdc_fb_suspend(struct device *dev)
{
	struct mxc_epdc_fb_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if((POWER_STATE_ON == data->power_state) && !data->powering_down ) {

		dev_warn(data->dev,"%s() EPD power ON !? ,current state=%d...\n",
				__func__,data->power_state);

		data->powering_down = true;
		schedule_delayed_work(&data->epdc_done_work,
			msecs_to_jiffies(0));
	}

	if (POWER_STATE_OFF != data->power_state)
	{
		dev_err(dev,"%s() : waiting for EPD power down ,current state=%d...\n",
			__FUNCTION__,data->power_state);
		return -1;
	}

#ifdef EPD_SUSPEND_BLANK//[
	
	//data->pwrdown_delay = FB_POWERDOWN_DISABLE;
	ret = mxc_epdc_fb_blank(FB_BLANK_POWERDOWN, &data->info);

	if (ret)
		goto out;
#endif //]EPD_SUSPEND_BLANK

	if(ntx_epdc_suspend()<0)
		return -3;

	if(gSleep_Mode_Suspend) {
		if(time_before(jiffies,data->dwJiffies_To_TurnOFF_EP3V3)) {
			dev_warn(dev,"waiting for VEE stable %d->%d ,please retry suspend later !!!\n",
				(int)jiffies,(int)data->dwJiffies_To_TurnOFF_EP3V3);
			return -2;
		}
#ifdef NIGHT_MODE_XON_TIMING //[
		if(data->gpio_xon_desc) {
			hrtimer_cancel(&data->hrt_xon_on_ctrl);
			hrtimer_cancel(&data->hrt_xon_off_ctrl);
			gpiod_set_value(data->gpio_xon_desc,0);
		}
#endif //]NIGHT_MODE_XON_TIMING
	}


out:
	pinctrl_pm_select_sleep_state(dev);

	return ret;
}

static int mxc_epdc_fb_resume(struct device *dev)
{
	struct mxc_epdc_fb_data *data = dev_get_drvdata(dev);

	pinctrl_pm_select_default_state(dev);
	
	if(gSleep_Mode_Suspend) {
#ifdef NIGHT_MODE_XON_TIMING //[
		if(data->gpio_xon_desc) {
			gpiod_set_value(data->gpio_xon_desc,1);
		}
#endif //]NIGHT_MODE_XON_TIMING
	}
#ifdef EPD_SUSPEND_BLANK//[
	mxc_epdc_fb_blank(FB_BLANK_UNBLANK, &data->info);
#endif //]EPD_SUSPEND_BLANK
	epdc_init_settings(data);
	data->updates_active = false;

#ifdef QOS_ENABLE//[
	clk_prepare_enable(data->epdc_clk_axi);
	clk_prepare_enable(data->epdc_clk_pix);
	_epdc_qos_setup();
	clk_disable_unprepare(data->epdc_clk_pix);
	clk_disable_unprepare(data->epdc_clk_axi);
#endif QOS_ENABLE//]

	ntx_epdc_resume();
	return 0;
}
#else
#define mxc_epdc_fb_suspend	NULL
#define mxc_epdc_fb_resume	NULL
#endif

#ifdef CONFIG_PM
static int mxc_epdc_fb_runtime_suspend(struct device *dev)
{
	release_bus_freq(BUS_FREQ_HIGH);
	dev_dbg(dev, "epdc busfreq high release.\n");

	return 0;
}

static int mxc_epdc_fb_runtime_resume(struct device *dev)
{
	request_bus_freq(BUS_FREQ_HIGH);
	dev_dbg(dev, "epdc busfreq high request.\n");

	return 0;
}
#else
#define mxc_epdc_fb_runtime_suspend	NULL
#define mxc_epdc_fb_runtime_resume	NULL
#endif

static const struct dev_pm_ops mxc_epdc_fb_pm_ops = {
	SET_RUNTIME_PM_OPS(mxc_epdc_fb_runtime_suspend,
				mxc_epdc_fb_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(mxc_epdc_fb_suspend, mxc_epdc_fb_resume)
};



int mxc_epdc_fb_shutdown(struct platform_device *pdev)
{
	struct mxc_epdc_fb_data *fb_data = platform_get_drvdata(pdev);
	volatile unsigned long tickNow;


	if((POWER_STATE_ON == fb_data->power_state) && !fb_data->powering_down ) {
		dev_warn(fb_data->dev,"%s() EPD power ON !? ,current state=%d...\n",
				__func__,fb_data->power_state);

		fb_data->powering_down = true;
		schedule_delayed_work(&fb_data->epdc_done_work,
			msecs_to_jiffies(0));
	}

	while (POWER_STATE_OFF != fb_data->power_state)
	{
		dev_warn(fb_data->dev,"waiting for EPD power down ,current state=%d...\n",
				fb_data->power_state);
		msleep(100);
	}

#if 1
	/* Disable power to the EPD panel */
	if (regulator_is_enabled(fb_data->vcom_regulator))
		regulator_disable(fb_data->vcom_regulator);
	if (regulator_is_enabled(fb_data->display_regulator))
		regulator_disable(fb_data->display_regulator);

	/* Disable clocks to EPDC */
	clk_prepare_enable(fb_data->epdc_clk_axi);
	clk_prepare_enable(fb_data->epdc_clk_pix);
	__raw_writel(EPDC_CTRL_CLKGATE, EPDC_CTRL_SET);
	clk_disable_unprepare(fb_data->epdc_clk_pix);
	clk_disable_unprepare(fb_data->epdc_clk_axi);
#endif

	/* turn off the V3p3 */
	if ( !fb_data->v3p3_fixed && regulator_is_enabled(fb_data->v3p3_regulator)) {
		dev_dbg(fb_data->dev, "EPDC V3P3 regulator disabling ...\n");
		regulator_disable(fb_data->v3p3_regulator);
	}

#if 1
	while (1) {
		tickNow = jiffies;
		if(time_before(tickNow,fb_data->dwJiffies_To_TurnOFF_EP3V3)) {
			dev_warn(fb_data->dev,"waiting for VEE stable %d->%d,to turn off EP3V3 !!!\n",(int)tickNow,(int)fb_data->dwJiffies_To_TurnOFF_EP3V3);
			msleep(500);
		}
		else {
			dev_info(fb_data->dev,"wait VEE stable ok .\n");
			break;
		}
	}
#endif


	return 0;
}

void mxc_epdc_fb_shutdown_proc(void)
{
	mxc_epdc_fb_shutdown(g_fb_data->pdev);
}

static struct platform_driver mxc_epdc_fb_driver = {
	.probe = mxc_epdc_fb_probe,
	.remove = mxc_epdc_fb_remove,
	.shutdown = mxc_epdc_fb_shutdown,
	.driver = {
		   .name = "imx_epdc_v2_fb",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(imx_epdc_dt_ids),
		   .pm = &mxc_epdc_fb_pm_ops,
		   },
};

/* Callback function triggered after PxP receives an EOF interrupt */
static void pxp_dma_done(void *arg)
{
	struct pxp_tx_desc *tx_desc = to_tx_desc(arg);
	struct dma_chan *chan = tx_desc->txd.chan;
	struct pxp_channel *pxp_chan = to_pxp_channel(chan);
	struct mxc_epdc_fb_data *fb_data = pxp_chan->client;

	/*
	 * if epd works in external mode, we should queue epdc_intr_workqueue
	 * after a wfe_a process finishes.
	 */
	if (fb_data->epdc_wb_mode && (tx_desc->proc_data.engine_enable & PXP_ENABLE_WFE_A)) {
		pxp_get_collision_info(&fb_data->col_info);
		queue_work(fb_data->epdc_intr_workqueue,
			   &fb_data->epdc_intr_work);
	}

	/* This call will signal wait_for_completion_timeout() in send_buffer_to_pxp */
	complete(&fb_data->pxp_tx_cmpl);
}

static bool chan_filter(struct dma_chan *chan, void *arg)
{
	if (imx_dma_is_pxp(chan))
		return true;
	else
		return false;
}

/* Function to request PXP DMA channel */
static int pxp_chan_init(struct mxc_epdc_fb_data *fb_data)
{
	dma_cap_mask_t mask;
	struct dma_chan *chan;

	/*
	 * Request a free channel
	 */
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_PRIVATE, mask);
	chan = dma_request_channel(mask, chan_filter, NULL);
	if (!chan) {
		dev_err(fb_data->dev, "Unsuccessfully received channel!!!!\n");
		return -EBUSY;
	}

	fb_data->pxp_chan = to_pxp_channel(chan);
	fb_data->pxp_chan->client = fb_data;

	init_completion(&fb_data->pxp_tx_cmpl);

	return 0;
}

static int pxp_wfe_a_process_clear_workingbuffer(struct mxc_epdc_fb_data *fb_data,
			      u32 panel_width, u32 panel_height)
{
	dma_cookie_t cookie;
	struct scatterlist *sg = fb_data->sg;
	struct dma_chan *dma_chan;
	struct pxp_tx_desc *desc;
	struct dma_async_tx_descriptor *txd;
	struct pxp_config_data *pxp_conf = &fb_data->pxp_conf;
	struct pxp_proc_data *proc_data = &fb_data->pxp_conf.proc_data;
	int i, j = 0, ret;
	int length;

	dev_dbg(fb_data->dev, "Starting PxP WFE_A process for clearing WB.\n");

	/* First, check to see that we have acquired a PxP Channel object */
	if (fb_data->pxp_chan == NULL) {
		/*
		 * PxP Channel has not yet been created and initialized,
		 * so let's go ahead and try
		 */
		ret = pxp_chan_init(fb_data);
		if (ret) {
			/*
			 * PxP channel init failed, and we can't use the
			 * PxP until the PxP DMA driver has loaded, so we abort
			 */
			dev_err(fb_data->dev, "PxP chan init failed\n");
			return -ENODEV;
		}
	}

	/*
	 * Init completion, so that we
	 * can be properly informed of the completion
	 * of the PxP task when it is done.
	 */
	init_completion(&fb_data->pxp_tx_cmpl);

	dma_chan = &fb_data->pxp_chan->dma_chan;

	txd = dma_chan->device->device_prep_slave_sg(dma_chan, sg, 2 + 4,
						     DMA_TO_DEVICE,
						     DMA_PREP_INTERRUPT,
						     NULL);
	if (!txd) {
		dev_err(fb_data->info.device,
			"Error preparing a DMA transaction descriptor.\n");
		return -EIO;
	}

	txd->callback_param = txd;
	txd->callback = pxp_dma_done;

	proc_data->working_mode = PXP_MODE_STANDARD;
	proc_data->engine_enable = PXP_ENABLE_WFE_A;
	proc_data->lut = 0;
	proc_data->detection_only = 0;
	proc_data->reagl_en = 0;
	proc_data->partial_update = 0;
	proc_data->alpha_en = 1;
	proc_data->lut_sels = fb_data->luts_complete;
	proc_data->lut_cleanup = 1;

	pxp_conf->wfe_a_fetch_param[0].stride = panel_width;
	pxp_conf->wfe_a_fetch_param[0].width = panel_width;
	pxp_conf->wfe_a_fetch_param[0].height = panel_height;
	pxp_conf->wfe_a_fetch_param[0].paddr = fb_data->phys_addr_black;
	pxp_conf->wfe_a_fetch_param[1].stride = panel_width;
	pxp_conf->wfe_a_fetch_param[1].width = panel_width;
	pxp_conf->wfe_a_fetch_param[1].height = panel_height;
	pxp_conf->wfe_a_fetch_param[1].paddr = fb_data->working_buffer_phys;
	pxp_conf->wfe_a_fetch_param[0].left = 0;
	pxp_conf->wfe_a_fetch_param[0].top = 0;
	pxp_conf->wfe_a_fetch_param[1].left = 0;
	pxp_conf->wfe_a_fetch_param[1].top = 0;

	pxp_conf->wfe_a_store_param[0].stride = panel_width;
	pxp_conf->wfe_a_store_param[0].width = panel_width;
	pxp_conf->wfe_a_store_param[0].height = panel_height;
	pxp_conf->wfe_a_store_param[0].paddr = fb_data->phys_addr_y4c;
	pxp_conf->wfe_a_store_param[1].stride = panel_width;
	pxp_conf->wfe_a_store_param[1].width = panel_width;
	pxp_conf->wfe_a_store_param[1].height = panel_height;
	pxp_conf->wfe_a_store_param[1].paddr = fb_data->working_buffer_phys;
	pxp_conf->wfe_a_store_param[0].left = 0;
	pxp_conf->wfe_a_store_param[0].top = 0;
	pxp_conf->wfe_a_store_param[1].left = 0;
	pxp_conf->wfe_a_store_param[1].top = 0;

	desc = to_tx_desc(txd);
	length = desc->len;

	memcpy(&desc->proc_data, proc_data, sizeof(struct pxp_proc_data));
	for (i = 0; i < length; i++) {
		if (i == 0 || i == 1) {/* wfe_a won't use s0 or output at all */
			desc = desc->next;

		} else if ((pxp_conf->proc_data.engine_enable & PXP_ENABLE_WFE_A) && (j < 4)) {
			for (j = 0; j < 4; j++) {
				if (j == 0) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->wfe_a_fetch_param[0],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_WFE_A_FETCH0;
				} else if (j == 1) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->wfe_a_fetch_param[1],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_WFE_A_FETCH1;
				} else if (j == 2) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->wfe_a_store_param[0],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_WFE_A_STORE0;
				} else if (j == 3) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->wfe_a_store_param[1],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_WFE_A_STORE1;
				}

				desc = desc->next;
			}

			i += 4;
		}
	}

	/* Submitting our TX starts the PxP processing task */
	cookie = txd->tx_submit(txd);
	if (cookie < 0) {
		dev_err(fb_data->info.device, "Error sending FB through PxP\n");
		return -EIO;
	}

	fb_data->txd = txd;

	/* trigger ePxP */
	dma_async_issue_pending(dma_chan);

	return 0;
}

static int pxp_clear_wb_work_func(struct mxc_epdc_fb_data *fb_data)
{
	unsigned int hist_stat;
	unsigned int pixel_nums;
	int ret;

	dev_dbg(fb_data->dev, "PxP WFE to clear working buffer.\n");

	mutex_lock(&fb_data->pxp_mutex);
	ret = pxp_wfe_a_process_clear_workingbuffer(fb_data, fb_data->cur_mode->vmode->xres, fb_data->cur_mode->vmode->yres);
	if (ret) {
		dev_err(fb_data->dev, "Unable to submit PxP update task.\n");
		mutex_unlock(&fb_data->pxp_mutex);
		return ret;
	}
	mutex_unlock(&fb_data->pxp_mutex);

	/* If needed, enable EPDC HW while ePxP is processing */
	if ((fb_data->power_state == POWER_STATE_OFF)
		|| fb_data->powering_down) {
		epdc_powerup(fb_data);
	}

	/* This is a blocking call, so upon return PxP tx should be done */
	mutex_lock(&fb_data->pxp_mutex);
	ret = pxp_complete_update(fb_data, &hist_stat, &pixel_nums);
	if (ret) {
		dev_err(fb_data->dev, "Unable to complete PxP update task: clear wb process\n");
		mutex_unlock(&fb_data->pxp_mutex);
		return ret;
	}
	mutex_unlock(&fb_data->pxp_mutex);

	return 0;
}


/* PS_AS_OUT */
static int pxp_legacy_process(struct mxc_epdc_fb_data *fb_data,
			      u32 src_width, u32 src_height,
			      struct mxcfb_rect *update_region)
{
	dma_cookie_t cookie;
	struct scatterlist *sg = fb_data->sg;
	struct dma_chan *dma_chan;
	struct pxp_tx_desc *desc;
	struct dma_async_tx_descriptor *txd;
	struct pxp_config_data *pxp_conf = &fb_data->pxp_conf;
	struct pxp_proc_data *proc_data = &fb_data->pxp_conf.proc_data;
	int i, ret;
	int length;

	dev_dbg(fb_data->dev, "Starting PxP legacy process.\n");

	/* First, check to see that we have acquired a PxP Channel object */
	if (fb_data->pxp_chan == NULL) {
		/*
		 * PxP Channel has not yet been created and initialized,
		 * so let's go ahead and try
		 */
		ret = pxp_chan_init(fb_data);
		if (ret) {
			/*
			 * PxP channel init failed, and we can't use the
			 * PxP until the PxP DMA driver has loaded, so we abort
			 */
			dev_err(fb_data->dev, "PxP chan init failed\n");
			return -ENODEV;
		}
	}

	/*
	 * Init completion, so that we
	 * can be properly informed of the completion
	 * of the PxP task when it is done.
	 */
	init_completion(&fb_data->pxp_tx_cmpl);

	dma_chan = &fb_data->pxp_chan->dma_chan;

	txd = dma_chan->device->device_prep_slave_sg(dma_chan, sg, 2,
						     DMA_TO_DEVICE,
						     DMA_PREP_INTERRUPT,
						     NULL);
	if (!txd) {
		dev_err(fb_data->info.device,
			"Error preparing a DMA transaction descriptor.\n");
		return -EIO;
	}

	txd->callback_param = txd;
	txd->callback = pxp_dma_done;

	proc_data->working_mode = PXP_MODE_LEGACY;
	proc_data->engine_enable = PXP_ENABLE_PS_AS_OUT;

	/*
	 * Configure PxP for processing of new update region
	 * The rest of our config params were set up in
	 * probe() and should not need to be changed.
	 */
	pxp_conf->s0_param.width = src_width;
	pxp_conf->s0_param.height = src_height;
	proc_data->srect.top = update_region->top;
	proc_data->srect.left = update_region->left;
	proc_data->srect.width = update_region->width;
	proc_data->srect.height = update_region->height;
	proc_data->lut_cleanup = 0;

	/*
	 * Because only YUV/YCbCr image can be scaled, configure
	 * drect equivalent to srect, as such do not perform scaling.
	 */
	proc_data->drect.top = 0;
	proc_data->drect.left = 0;

	/* PXP expects rotation in terms of degrees */
	proc_data->rotate = fb_data->epdc_fb_var.rotate * 90;
	if (proc_data->rotate > 270)
		proc_data->rotate = 0;

	/* we should pass the rotated values to PXP */
	if ((proc_data->rotate == 90) || (proc_data->rotate == 270)) {
		proc_data->drect.width = proc_data->srect.height;
		proc_data->drect.height = proc_data->srect.width;
		pxp_conf->out_param.width = update_region->height;
		pxp_conf->out_param.height = update_region->width;
		pxp_conf->out_param.stride = update_region->height;
	} else {
		proc_data->drect.width = proc_data->srect.width;
		proc_data->drect.height = proc_data->srect.height;
		pxp_conf->out_param.width = update_region->width;
		pxp_conf->out_param.height = update_region->height;
		pxp_conf->out_param.stride = update_region->width;
	}

	/* For EPDC v2.0, we need output to be 64-bit
	 * aligned since EPDC stride does not work. */
	if (fb_data->rev <= 20)
		pxp_conf->out_param.stride = ALIGN(pxp_conf->out_param.stride, 8);

	desc = to_tx_desc(txd);
	length = desc->len;

	for (i = 0; i < length; i++) {
		if (i == 0) {/* S0 */
			memcpy(&desc->proc_data, proc_data, sizeof(struct pxp_proc_data));
			pxp_conf->s0_param.paddr = sg_dma_address(&sg[0]);
			memcpy(&desc->layer_param.s0_param, &pxp_conf->s0_param,
				sizeof(struct pxp_layer_param));
			desc = desc->next;

		} else if (i == 1) {
			pxp_conf->out_param.paddr = sg_dma_address(&sg[1]);
			memcpy(&desc->layer_param.out_param, &pxp_conf->out_param,
				sizeof(struct pxp_layer_param));
			desc = desc->next;
		}
	}

	/* Submitting our TX starts the PxP processing task */
	cookie = txd->tx_submit(txd);
	if (cookie < 0) {
		dev_err(fb_data->info.device, "Error sending FB through PxP\n");
		return -EIO;
	}

	fb_data->txd = txd;

	/* trigger ePxP */
	dma_async_issue_pending(dma_chan);

	return 0;
}
#ifdef EPDC_V2_ENABLE_HW_DITHER //[

static int pxp_process_dithering(struct mxc_epdc_fb_data *fb_data,
			      struct mxcfb_rect *update_region)
{
	dma_cookie_t cookie;
	struct scatterlist *sg = fb_data->sg;
	struct dma_chan *dma_chan;
	struct pxp_tx_desc *desc;
	struct dma_async_tx_descriptor *txd;
	struct pxp_config_data *pxp_conf = &fb_data->pxp_conf;
	struct pxp_proc_data *proc_data = &fb_data->pxp_conf.proc_data;
	int i, j = 0, ret;
	int length;

	dev_dbg(fb_data->dev, "Starting PxP Dithering process.\n");

	/* First, check to see that we have acquired a PxP Channel object */
	if (fb_data->pxp_chan == NULL) {
		/*
		 * PxP Channel has not yet been created and initialized,
		 * so let's go ahead and try
		 */
		ret = pxp_chan_init(fb_data);
		if (ret) {
			/*
			 * PxP channel init failed, and we can't use the
			 * PxP until the PxP DMA driver has loaded, so we abort
			 */
			dev_err(fb_data->dev, "PxP chan init failed\n");
			return -ENODEV;
		}
	}

	/*
	 * Init completion, so that we
	 * can be properly informed of the completion
	 * of the PxP task when it is done.
	 */
	init_completion(&fb_data->pxp_tx_cmpl);

	dma_chan = &fb_data->pxp_chan->dma_chan;

	txd = dma_chan->device->device_prep_slave_sg(dma_chan, sg, 2 + 4,
						     DMA_TO_DEVICE,
						     DMA_PREP_INTERRUPT,
						     NULL);
	if (!txd) {
		dev_err(fb_data->info.device,
			"Error preparing a DMA transaction descriptor.\n");
		return -EIO;
	}

	txd->callback_param = txd;
	txd->callback = pxp_dma_done;

	proc_data->working_mode = PXP_MODE_STANDARD;
	proc_data->engine_enable = PXP_ENABLE_DITHER;

	pxp_conf->dither_fetch_param[0].stride = update_region->width;
	pxp_conf->dither_fetch_param[0].width = update_region->width;
	pxp_conf->dither_fetch_param[0].height = update_region->height;
#ifdef USE_PS_AS_OUTPUT
	pxp_conf->dither_fetch_param[0].paddr = sg_dma_address(&sg[1]);
#else
	pxp_conf->dither_fetch_param[0].paddr = sg_dma_address(&sg[0]);
#endif
	pxp_conf->dither_fetch_param[1].stride = update_region->width;
	pxp_conf->dither_fetch_param[1].width = update_region->width;
	pxp_conf->dither_fetch_param[1].height = update_region->height;
	pxp_conf->dither_fetch_param[1].paddr = pxp_conf->dither_fetch_param[0].paddr;

	pxp_conf->dither_store_param[0].stride = update_region->width;
	pxp_conf->dither_store_param[0].width = update_region->width;
	pxp_conf->dither_store_param[0].height = update_region->height;
	pxp_conf->dither_store_param[0].paddr = fb_data->phys_addr_y4;
	pxp_conf->dither_store_param[1].stride = update_region->width;
	pxp_conf->dither_store_param[1].width = update_region->width;
	pxp_conf->dither_store_param[1].height = update_region->height;
	pxp_conf->dither_store_param[1].paddr = pxp_conf->dither_store_param[0].paddr;

	desc = to_tx_desc(txd);
	length = desc->len;

	for (i = 0; i < length; i++) {
		if (i == 0) {/* S0 */
			memcpy(&desc->proc_data, proc_data, sizeof(struct pxp_proc_data));
			pxp_conf->s0_param.paddr = sg_dma_address(&sg[0]);
			memcpy(&desc->layer_param.s0_param, &pxp_conf->s0_param,
				sizeof(struct pxp_layer_param));
			desc = desc->next;
		} else if (i == 1) {
			pxp_conf->out_param.paddr = sg_dma_address(&sg[1]);
			memcpy(&desc->layer_param.out_param, &pxp_conf->out_param,
				sizeof(struct pxp_layer_param));
			desc = desc->next;
		} else if ((pxp_conf->proc_data.engine_enable & PXP_ENABLE_DITHER) && (j < 4)) {
			for (j = 0; j < 4; j++) {
				if (j == 0) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->dither_fetch_param[0],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_DITHER_FETCH0;
				} else if (j == 1) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->dither_fetch_param[1],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_DITHER_FETCH1;
				} else if (j == 2) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->dither_store_param[0],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_DITHER_STORE0;
				} else if (j == 3) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->dither_store_param[1],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_DITHER_STORE1;
				}

				desc = desc->next;
			}

			i += 4;
		}
	}

	/* Submitting our TX starts the PxP processing task */
	cookie = txd->tx_submit(txd);
	if (cookie < 0) {
		dev_err(fb_data->info.device, "Error sending FB through PxP\n");
		return -EIO;
	}

	fb_data->txd = txd;

	/* trigger ePxP */
	dma_async_issue_pending(dma_chan);

	return 0;
}
#else //][!EPDC_V2_ENABLE_HW_DITHER
static int pxp_process_dithering(struct mxc_epdc_fb_data *fb_data,
			      struct mxcfb_rect *update_region) {return 0;}
#endif //] EPDC_V2_ENABLE_HW_DITHER

/*
 * Function to call PxP DMA driver and send our latest FB update region
 * through the PxP and out to an intermediate buffer.
 * Note: This is a blocking call, so upon return the PxP tx should be complete.
 */
static int pxp_wfe_a_process(struct mxc_epdc_fb_data *fb_data,
			     struct mxcfb_rect *update_region,
			     struct update_data_list *upd_data_list)
{
	dma_cookie_t cookie;
	struct scatterlist *sg = fb_data->sg;
	struct dma_chan *dma_chan;
	struct pxp_tx_desc *desc;
	struct dma_async_tx_descriptor *txd;
	struct pxp_config_data *pxp_conf = &fb_data->pxp_conf;
	struct pxp_proc_data *proc_data = &fb_data->pxp_conf.proc_data;
	int i, j = 0, ret;
	int length;
	bool is_transform;

	dev_dbg(fb_data->dev, "Starting PxP WFE_A process.\n");

	/* First, check to see that we have acquired a PxP Channel object */
	if (fb_data->pxp_chan == NULL) {
		/*
		 * PxP Channel has not yet been created and initialized,
		 * so let's go ahead and try
		 */
		ret = pxp_chan_init(fb_data);
		if (ret) {
			/*
			 * PxP channel init failed, and we can't use the
			 * PxP until the PxP DMA driver has loaded, so we abort
			 */
			dev_err(fb_data->dev, "PxP chan init failed\n");
			return -ENODEV;
		}
	}

	/*
	 * Init completion, so that we
	 * can be properly informed of the completion
	 * of the PxP task when it is done.
	 */
	init_completion(&fb_data->pxp_tx_cmpl);

	dma_chan = &fb_data->pxp_chan->dma_chan;

	txd = dma_chan->device->device_prep_slave_sg(dma_chan, sg, 2 + 4,
						     DMA_TO_DEVICE,
						     DMA_PREP_INTERRUPT,
						     NULL);
	if (!txd) {
		dev_err(fb_data->info.device,
			"Error preparing a DMA transaction descriptor.\n");
		return -EIO;
	}

	txd->callback_param = txd;
	txd->callback = pxp_dma_done;

	proc_data->working_mode = PXP_MODE_STANDARD;
	proc_data->engine_enable = PXP_ENABLE_WFE_A;
	proc_data->lut = upd_data_list->lut_num;
	proc_data->alpha_en = 0;
	proc_data->lut_sels = fb_data->luts_complete;
	proc_data->lut_status_1 = __raw_readl(EPDC_STATUS_LUTS);
	proc_data->lut_status_2 = __raw_readl(EPDC_STATUS_LUTS2);
	proc_data->lut_cleanup = 0;

	if (upd_data_list->update_desc->upd_data.flags & EPDC_FLAG_TEST_COLLISION) {
		proc_data->detection_only = 1;
		dev_dbg(fb_data->info.device,
			 "collision test_only send to pxp\n");
	} else
		proc_data->detection_only = 0;

	if (upd_data_list->update_desc->upd_data.update_mode == UPDATE_MODE_PARTIAL)
		proc_data->partial_update = 1;
	else
		proc_data->partial_update = 0;

	/* fetch0 is upd buffer */
	pxp_conf->wfe_a_fetch_param[0].stride = upd_data_list->update_desc->epdc_stride;
	pxp_conf->wfe_a_fetch_param[0].width = update_region->width;
	pxp_conf->wfe_a_fetch_param[0].height = update_region->height;
	/* upd buffer left and top should be always 0 */
	pxp_conf->wfe_a_fetch_param[0].left = 0;
	pxp_conf->wfe_a_fetch_param[0].top = 0;
#ifdef EPDC_V2_ENABLE_HW_DITHER //[
	if (proc_data->dither_mode) {
		pxp_conf->wfe_a_fetch_param[0].paddr = fb_data->phys_addr_y4;
	} 
	else 
#endif //] EPDC_V2_ENABLE_HW_DITHER
	{
		is_transform = ((upd_data_list->update_desc->upd_data.flags &
			(EPDC_FLAG_ENABLE_INVERSION | EPDC_FLAG_USE_DITHERING_Y1 |
			EPDC_FLAG_USE_DITHERING_Y4 | EPDC_FLAG_FORCE_MONOCHROME |
			EPDC_FLAG_USE_CMAP)) && (proc_data->scaling == 0) &&
			(proc_data->hflip == 0) && (proc_data->vflip == 0)) ?
			true : false;

		if ((fb_data->epdc_fb_var.rotate == FB_ROTATE_UR) &&
			(fb_data->epdc_fb_var.grayscale == GRAYSCALE_8BIT) &&
			!is_transform && (proc_data->dither_mode == 0) &&
			!(upd_data_list->update_desc->upd_data.flags &
			EPDC_FLAG_USE_ALT_BUFFER) &&
			!fb_data->restrict_width) {
			sg_dma_address(&sg[0]) = fb_data->info.fix.smem_start;
			sg_set_page(&sg[0],
				virt_to_page(fb_data->info.screen_base),
				fb_data->info.fix.smem_len,
				offset_in_page(fb_data->info.screen_base));
			pxp_conf->wfe_a_fetch_param[0].paddr =
					sg_dma_address(&sg[0]);

			pxp_conf->wfe_a_fetch_param[0].left = update_region->left;
			pxp_conf->wfe_a_fetch_param[0].top = update_region->top;
		} else
			pxp_conf->wfe_a_fetch_param[0].paddr =
				upd_data_list->phys_addr + upd_data_list->update_desc->epdc_offs;
	}

	/* fetch1 is working buffer */
	pxp_conf->wfe_a_fetch_param[1].stride = fb_data->cur_mode->vmode->xres;
	pxp_conf->wfe_a_fetch_param[1].width = update_region->width;
	pxp_conf->wfe_a_fetch_param[1].height = update_region->height;
	pxp_conf->wfe_a_fetch_param[1].paddr = fb_data->working_buffer_phys;
	pxp_conf->wfe_a_fetch_param[1].left = update_region->left;
	pxp_conf->wfe_a_fetch_param[1].top = update_region->top;

	/* store0 is y4c buffer */
	pxp_conf->wfe_a_store_param[0].stride = fb_data->cur_mode->vmode->xres;
	pxp_conf->wfe_a_store_param[0].width = update_region->width;
	pxp_conf->wfe_a_store_param[0].height = update_region->height;
	pxp_conf->wfe_a_store_param[0].paddr = fb_data->phys_addr_y4c;

	/* store1 is (temp) working buffer */
	pxp_conf->wfe_a_store_param[1].stride = fb_data->cur_mode->vmode->xres;
	pxp_conf->wfe_a_store_param[1].width = update_region->width;
	pxp_conf->wfe_a_store_param[1].height = update_region->height;
	if (proc_data->reagl_en)
		pxp_conf->wfe_a_store_param[1].paddr = fb_data->tmp_working_buffer_phys;
	else
		pxp_conf->wfe_a_store_param[1].paddr = fb_data->working_buffer_phys;
	pxp_conf->wfe_a_store_param[1].left = update_region->left;
	pxp_conf->wfe_a_store_param[1].top = update_region->top;

	desc = to_tx_desc(txd);
	length = desc->len;

	memcpy(&desc->proc_data, proc_data, sizeof(struct pxp_proc_data));
	for (i = 0; i < length; i++) {
		if (i == 0 || i == 1) {/* wfe_a won't use s0 or output at all */
			desc = desc->next;
		} else if ((pxp_conf->proc_data.engine_enable & PXP_ENABLE_WFE_A) && (j < 4)) {
			for (j = 0; j < 4; j++) {
				if (j == 0) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->wfe_a_fetch_param[0],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_WFE_A_FETCH0;
				} else if (j == 1) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->wfe_a_fetch_param[1],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_WFE_A_FETCH1;
				} else if (j == 2) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->wfe_a_store_param[0],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_WFE_A_STORE0;
				} else if (j == 3) {
					memcpy(&desc->layer_param.processing_param,
					       &pxp_conf->wfe_a_store_param[1],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.flag = PXP_BUF_FLAG_WFE_A_STORE1;
				}

				desc = desc->next;
			}

			i += 4;
		}
	}

	/* Submitting our TX starts the PxP processing task */
	cookie = txd->tx_submit(txd);
	if (cookie < 0) {
		dev_err(fb_data->info.device, "Error sending FB through PxP\n");
		return -EIO;
	}

	fb_data->txd = txd;

	/* trigger ePxP */
	dma_async_issue_pending(dma_chan);

	return 0;
}

/* For REAGL/-D processing */
static int pxp_wfe_b_process_update(struct mxc_epdc_fb_data *fb_data,
			      struct mxcfb_rect *update_region)
{
	dma_cookie_t cookie;
	struct scatterlist *sg = fb_data->sg;
	struct dma_chan *dma_chan;
	struct pxp_tx_desc *desc;
	struct dma_async_tx_descriptor *txd;
	struct pxp_config_data *pxp_conf = &fb_data->pxp_conf;
	struct pxp_proc_data *proc_data = &fb_data->pxp_conf.proc_data;
	int i, j = 0, ret;
	int length;

	dev_dbg(fb_data->dev, "Starting PxP WFE_B process.\n");

	/* First, check to see that we have acquired a PxP Channel object */
	if (fb_data->pxp_chan == NULL) {
		/*
		 * PxP Channel has not yet been created and initialized,
		 * so let's go ahead and try
		 */
		ret = pxp_chan_init(fb_data);
		if (ret) {
			/*
			 * PxP channel init failed, and we can't use the
			 * PxP until the PxP DMA driver has loaded, so we abort
			 */
			dev_err(fb_data->dev, "PxP chan init failed\n");
			return -ENODEV;
		}
	}

	/*
	 * Init completion, so that we
	 * can be properly informed of the completion
	 * of the PxP task when it is done.
	 */
	init_completion(&fb_data->pxp_tx_cmpl);

	dma_chan = &fb_data->pxp_chan->dma_chan;

	txd = dma_chan->device->device_prep_slave_sg(dma_chan, sg, 2 + 4,
						     DMA_TO_DEVICE,
						     DMA_PREP_INTERRUPT,
						     NULL);
	if (!txd) {
		dev_err(fb_data->info.device,
			"Error preparing a DMA transaction descriptor.\n");
		return -EIO;
	}

	txd->callback_param = txd;
	txd->callback = pxp_dma_done;

	proc_data->working_mode = PXP_MODE_STANDARD;
	proc_data->engine_enable = PXP_ENABLE_WFE_B;
	proc_data->lut_update = false;
	proc_data->lut_cleanup = 0;

	pxp_conf->wfe_b_fetch_param[0].stride = fb_data->cur_mode->vmode->xres;
	pxp_conf->wfe_b_fetch_param[0].width = fb_data->cur_mode->vmode->xres;
	pxp_conf->wfe_b_fetch_param[0].height = fb_data->cur_mode->vmode->yres;
	pxp_conf->wfe_b_fetch_param[0].paddr = fb_data->phys_addr_black;
	pxp_conf->wfe_b_fetch_param[1].stride = fb_data->cur_mode->vmode->xres;
	pxp_conf->wfe_b_fetch_param[1].width = update_region->width;
	pxp_conf->wfe_b_fetch_param[1].height = update_region->height;
	pxp_conf->wfe_b_fetch_param[1].top = update_region->top;
	pxp_conf->wfe_b_fetch_param[1].left = update_region->left;
	pxp_conf->wfe_b_fetch_param[1].paddr = fb_data->tmp_working_buffer_phys;

	pxp_conf->wfe_b_store_param[0].stride = fb_data->cur_mode->vmode->xres;
	pxp_conf->wfe_b_store_param[0].width = update_region->width;
	pxp_conf->wfe_b_store_param[0].height = update_region->height;
	pxp_conf->wfe_b_store_param[0].top = update_region->top;
	pxp_conf->wfe_b_store_param[0].left = update_region->left;
	pxp_conf->wfe_b_store_param[0].paddr = fb_data->working_buffer_phys;
	pxp_conf->wfe_b_store_param[1].stride = fb_data->cur_mode->vmode->xres;
	pxp_conf->wfe_b_store_param[1].width = update_region->width;
	pxp_conf->wfe_b_store_param[1].height = update_region->height;
	pxp_conf->wfe_b_store_param[1].paddr = 0;

	desc = to_tx_desc(txd);
	length = desc->len;

	for (i = 0; i < length; i++) {
		if (i == 0) {	/* S0 */
			memcpy(&desc->proc_data, proc_data,
			       sizeof(struct pxp_proc_data));
			pxp_conf->s0_param.paddr = sg_dma_address(&sg[0]);
			memcpy(&desc->layer_param.s0_param, &pxp_conf->s0_param,
			       sizeof(struct pxp_layer_param));
			desc = desc->next;
		} else if (i == 1) {
			pxp_conf->out_param.paddr = sg_dma_address(&sg[1]);
			memcpy(&desc->layer_param.out_param,
			       &pxp_conf->out_param,
			       sizeof(struct pxp_layer_param));
			desc = desc->next;
		} else
		    if ((pxp_conf->proc_data.engine_enable & PXP_ENABLE_WFE_B)
			&& (j < 4)) {
			for (j = 0; j < 4; j++) {
				if (j == 0) {
					memcpy(&desc->layer_param.
					       processing_param,
					       &pxp_conf->wfe_b_fetch_param[0],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.
					    flag = PXP_BUF_FLAG_WFE_B_FETCH0;
				} else if (j == 1) {
					memcpy(&desc->layer_param.
					       processing_param,
					       &pxp_conf->wfe_b_fetch_param[1],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.
					    flag = PXP_BUF_FLAG_WFE_B_FETCH1;
				} else if (j == 2) {
					memcpy(&desc->layer_param.
					       processing_param,
					       &pxp_conf->wfe_b_store_param[0],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.
					    flag = PXP_BUF_FLAG_WFE_B_STORE0;
				} else if (j == 3) {
					memcpy(&desc->layer_param.
					       processing_param,
					       &pxp_conf->wfe_b_store_param[1],
					       sizeof(struct pxp_layer_param));
					desc->layer_param.processing_param.
					    flag = PXP_BUF_FLAG_WFE_B_STORE1;
				}

				desc = desc->next;
			}

			i += 4;
		}
	}

	/* Submitting our TX starts the PxP processing task */
	cookie = txd->tx_submit(txd);
	if (cookie < 0) {
		dev_err(fb_data->info.device, "Error sending FB through PxP\n");
		return -EIO;
	}

	fb_data->txd = txd;

	/* trigger ePxP */
	dma_async_issue_pending(dma_chan);

	return 0;
}

static int pxp_complete_update(struct mxc_epdc_fb_data *fb_data, u32 *hist_stat,
				u32 *pixel_nums)
{
	int ret;
	/*
	 * Wait for completion event, which will be set
	 * through our TX callback function.
	 */
	ret = wait_for_completion_timeout(&fb_data->pxp_tx_cmpl, HZ * 2);
	if (ret <= 0) {
		dev_info(fb_data->info.device,
			 "PxP operation failed due to %s\n",
			 ret < 0 ? "user interrupt" : "timeout");
		dma_release_channel(&fb_data->pxp_chan->dma_chan);
		fb_data->pxp_chan = NULL;
		return ret ? : -ETIMEDOUT;
	}

	if ((fb_data->pxp_conf.proc_data.lut_transform & EPDC_FLAG_USE_CMAP) &&
		fb_data->pxp_conf.proc_data.lut_map_updated)
		fb_data->pxp_conf.proc_data.lut_map_updated = false;

	*hist_stat = to_tx_desc(fb_data->txd)->hist_status;
	*pixel_nums = to_tx_desc(fb_data->txd)->pixel_nums;
	dma_release_channel(&fb_data->pxp_chan->dma_chan);
	fb_data->pxp_chan = NULL;

	dev_dbg(fb_data->dev, "TX completed\n");

	return 0;
}

/*
 * Different dithering algorithm can be used. We chose
 * to implement Bill Atkinson's algorithm as an example
 * Thanks Bill Atkinson for his dithering algorithm.
 */

/*
 * Dithering algorithm implementation - Y8->Y1 version 1.0 for i.MX
 */
static void do_dithering_processing_Y1_v1_0(
		unsigned char *update_region_virt_ptr,
		dma_addr_t update_region_phys_ptr,
		struct mxcfb_rect *update_region,
		unsigned long update_region_stride,
		int *err_dist)
{

	/* create a temp error distribution array */
	int bwPix;
	int y;
	int col;
	int *err_dist_l0, *err_dist_l1, *err_dist_l2, distrib_error;
	int width_3 = update_region->width + 3;
	char *y8buf;
	int x_offset = 0;

	/* prime a few elements the error distribution array */
	for (y = 0; y < update_region->height; y++) {
		/* Dithering the Y8 in sbuf to BW suitable for A2 waveform */
		err_dist_l0 = err_dist + (width_3) * (y % 3);
		err_dist_l1 = err_dist + (width_3) * ((y + 1) % 3);
		err_dist_l2 = err_dist + (width_3) * ((y + 2) % 3);

		y8buf = update_region_virt_ptr + x_offset;

		/* scan the line and convert the Y8 to BW */
		for (col = 1; col <= update_region->width; col++) {
			bwPix = *(err_dist_l0 + col) + *y8buf;

			if (bwPix >= 128) {
				*y8buf++ = 0xff;
				distrib_error = (bwPix - 255) >> 3;
			} else {
				*y8buf++ = 0;
				distrib_error = bwPix >> 3;
			}

			/* modify the error distribution buffer */
			*(err_dist_l0 + col + 2) += distrib_error;
			*(err_dist_l1 + col + 1) += distrib_error;
			*(err_dist_l0 + col + 1) += distrib_error;
			*(err_dist_l1 + col - 1) += distrib_error;
			*(err_dist_l1 + col) += distrib_error;
			*(err_dist_l2 + col) = distrib_error;
		}
		x_offset += update_region_stride;
	}

	flush_cache_all();
	outer_flush_range(update_region_phys_ptr, update_region_phys_ptr +
			update_region->height * update_region->width);
}

/*
 * Dithering algorithm implementation - Y8->Y4 version 1.0 for i.MX
 */

static void do_dithering_processing_Y4_v1_0(
		unsigned char *update_region_virt_ptr,
		dma_addr_t update_region_phys_ptr,
		struct mxcfb_rect *update_region,
		unsigned long update_region_stride,
		int *err_dist)
{

	/* create a temp error distribution array */
	int gcPix;
	int y;
	int col;
	int *err_dist_l0, *err_dist_l1, *err_dist_l2, distrib_error;
	int width_3 = update_region->width + 3;
	char *y8buf;
	int x_offset = 0;

	/* prime a few elements the error distribution array */
	for (y = 0; y < update_region->height; y++) {
		/* Dithering the Y8 in sbuf to Y4 */
		err_dist_l0 = err_dist + (width_3) * (y % 3);
		err_dist_l1 = err_dist + (width_3) * ((y + 1) % 3);
		err_dist_l2 = err_dist + (width_3) * ((y + 2) % 3);

		y8buf = update_region_virt_ptr + x_offset;

		/* scan the line and convert the Y8 to Y4 */
		for (col = 1; col <= update_region->width; col++) {
			gcPix = *(err_dist_l0 + col) + *y8buf;

			if (gcPix > 255)
				gcPix = 255;
			else if (gcPix < 0)
				gcPix = 0;

			distrib_error = (*y8buf - (gcPix & 0xf0)) >> 3;

			*y8buf++ = gcPix & 0xf0;

			/* modify the error distribution buffer */
			*(err_dist_l0 + col + 2) += distrib_error;
			*(err_dist_l1 + col + 1) += distrib_error;
			*(err_dist_l0 + col + 1) += distrib_error;
			*(err_dist_l1 + col - 1) += distrib_error;
			*(err_dist_l1 + col) += distrib_error;
			*(err_dist_l2 + col) = distrib_error;
		}
		x_offset += update_region_stride;
	}

	flush_cache_all();
	outer_flush_range(update_region_phys_ptr, update_region_phys_ptr +
			update_region->height * update_region->width);
}

static int __init mxc_epdc_fb_init(void)
{
	return platform_driver_register(&mxc_epdc_fb_driver);
}
late_initcall(mxc_epdc_fb_init);

static void __exit mxc_epdc_fb_exit(void)
{
	platform_driver_unregister(&mxc_epdc_fb_driver);
}
module_exit(mxc_epdc_fb_exit);


MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MXC EPDC V2 framebuffer driver");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("fb");
