/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <mach/map.h>

#include <plat/cpu-freq.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/clock-clksrc.h>


#define SHIFT_SRC	28
#define SIZE_SRC	2

#define SHIFT_DIV1	0
#define SHIFT_DIV2	8
#define SHIFT_DIV3	13
#define SHIFT_DIV4	18
#define SHIFT_DIV5	23
#define SIZE_DIV	5

//#define DEBUG_CLOCKS

#ifdef DEBUG_CLOCKS
#define debug_printk(x...) printk(x)
#else
#define debug_printk(x...)
#endif

// PLLs

static int s5l_clock_always_on(struct clk *_clk, int _enable)
{
	return 0;
}

struct s5l_pll
{
	struct clk clk;
	void *__iomem con0;
	void *__iomem con1;
};

/*static int s5l_pll_set_rate(struct s5l_pll *_pll, uint32_t _p, uint32_t _m, uint32_t _s)
{
	uint32_t freq = (_m * (uint64_t)CLOCK_BASE_HZ) / _p;
	uint32_t freq_sel;
	if(freq > 1500000000)
		freq_sel = PLLCON1_FSEL_2;
	else if(freq > 1099999999)
		freq_sel = PLLCON1_FSEL_1;
	else
		freq_sel = PLLCON1_FSEL_0;

	
	writel((freq_sel << PLLCON1_FSEL_SHIFT) | PLLCON1_ENABLE, _pll->con1);
	writel((_m << PLLCON0_M_SHIFT) | (_p << PLLCON0_P_SHIFT) | (_s << PLLCON0_S_SHIFT)
			| PLLCON0_ENABLE | PLLCON0_UPDATE, _pll->con0);

	while(!(readl(_pll->con0) & PLLCON0_LOCKED));
	return 0;
}*/

static unsigned long s5l_pll_get_rate(struct s5l_pll *_pll)
{
	uint32_t conf = readl(_pll->con0);
	uint32_t base;
	uint64_t freq;
	if(!(conf & PLLCON0_ENABLE))
		return 0;

	base = (((uint64_t)PLLCON0_P(conf)) << PLLCON0_S(conf));
	freq = (PLLCON0_M(conf) * (uint64_t)CLOCK_BASE_HZ);
	do_div(freq, base);
	return freq;
}

static unsigned long s5l_pll_clk_get_rate(struct clk *_clk)
{
	struct s5l_pll *pll = container_of(_clk, struct s5l_pll, clk);
	return s5l_pll_get_rate(pll);
}

static struct clk_ops s5l_pll_clk_ops = {
	.get_rate = s5l_pll_clk_get_rate,
};

static struct s5l_pll s5l_plls[] = {
	{
		.clk = {
			.name = "apll",
			.id = -1,
			.ops = &s5l_pll_clk_ops,
			.enable = s5l_clock_always_on,
		},

		.con0 = S5L_APLL_CON0,
		.con1 = S5L_APLL_CON1,
	},
	{
		.clk = {
			.name = "mpll",
			.id = -1,
			.ops = &s5l_pll_clk_ops,
			.enable = s5l_clock_always_on,
		},

		.con0 = S5L_MPLL_CON0,
		.con1 = S5L_MPLL_CON1,
	},
	{
		.clk = {
			.name = "epll",
			.id = -1,
			.ops = &s5l_pll_clk_ops,
			.enable = s5l_clock_always_on,
		},

		.con0 = S5L_EPLL_CON0,
		.con1 = S5L_EPLL_CON1,
	},
	{
		.clk = {
			.name = "vpll",
			.id = -1,
			.ops = &s5l_pll_clk_ops,
			.enable = s5l_clock_always_on,
		},

		.con0 = S5L_VPLL_CON0,
		.con1 = S5L_VPLL_CON1,
	},
};

// Clock sources

static void s5l_clock_gate_toggle_idx(int _idx, int _enable)
{
	uint32_t __iomem *reg = &S5L_CLOCK_GATE[_idx];
	BUG_ON(_idx >= 64);

	debug_printk("%s: 0x%p 0x%x %d\n", __func__, reg, _idx, _enable);

	if(_enable)
		writel(readl(reg) | 0xF, reg);
	else
		writel(readl(reg) &~ 0xF, reg);

	while((readl(reg) & 0xF) != ((readl(reg) >> 4) & 0xF));
}

static int s5l_clock_gate_toggle(struct clk *_clk, int _enable)
{
	s5l_clock_gate_toggle_idx(_clk->ctrlbit, _enable);
	return 0;
}

struct s5l_power_zone
{
	const char *name;
	int gate;
	int count;
};

enum s5l_power_zones
{
	ZONE_MPERF,
	ZONE_HPERF0,
	ZONE_HPERF1,
	ZONE_HPERF2,
};

static struct s5l_power_zone s5l_power_zones[] = {
	[ZONE_MPERF] = {
		.name = "mperf",
		.gate = 0x15,
	},
	[ZONE_HPERF0] = {
		.name = "hperf0",
		.gate = 0x2,
	},
	[ZONE_HPERF1] = {
		.name = "hperf1",
		.gate = 0x5,
	},
	[ZONE_HPERF2] = {
		.name = "hperf2",
		.gate = 0xb,
	},
};

#define S5L_GET_POWER_ZONE(id)			(((id) >> 8) & 0xFF)
#define S5L_GET_CLOCK_GATE(id)			((id) & 0xFF)
#define S5L_MAKE_CLOCKGATE(zone, id)	((id) | ((zone) << 8))

static void s5l_power_zone_toggle(struct s5l_power_zone *_z, int _enable)
{
	debug_printk("%s_toggle: %d (%d).\n", _z->name, _enable, _z->count);
	if(_enable)
	{
		if((_z->count++) == 0)
			s5l_clock_gate_toggle_idx(_z->gate, 1);
	}
	else
	{
		_z->count--;
		if(_z->count < 0)
			_z->count = 0;

		if(_z->count == 0)
			s5l_clock_gate_toggle_idx(_z->gate, 0);
	}
	debug_printk("%s_toggled: %d (%d).\n", _z->name, _enable, _z->count);
}

static int s5l_clock_gate_toggle_zone(struct clk *_clk, int _enable)
{
	struct s5l_power_zone *zone =
		&s5l_power_zones[S5L_GET_POWER_ZONE(_clk->ctrlbit)];
	int clk = S5L_GET_CLOCK_GATE(_clk->ctrlbit);

	s5l_power_zone_toggle(zone, _enable);
	if(clk)
		s5l_clock_gate_toggle_idx(clk, _enable);

	return 0;
}

static int s5l_clock_gate_toggle_display(struct clk *_clk, int _enable)
{
	struct s5l_power_zone *zone = &s5l_power_zones[ZONE_HPERF2];
	int clk0 = _clk->ctrlbit & 0xFF;
	int clk1 = (_clk->ctrlbit >> 8) & 0xFF;

	s5l_power_zone_toggle(zone, _enable);
	s5l_clock_gate_toggle_idx(clk0, _enable);
	s5l_clock_gate_toggle_idx(clk1, _enable);

	return 0;
}

static struct clk *s5l_pll_source_list[] = {
	&s5l_plls[0].clk,
	&s5l_plls[1].clk,
	&s5l_plls[2].clk,
	&clk_xtal,
};

static struct clksrc_sources s5l_pll_sources = {
	.sources = s5l_pll_source_list,
	.nr_sources = ARRAY_SIZE(s5l_pll_source_list),
};

static struct clksrc_clk clk_system_source = {
	.clk = {
		.name = "system-source",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &s5l_pll_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x40, .shift = SHIFT_SRC, .size = SIZE_SRC }
};

static struct clksrc_clk clk_system0 = {
	.clk = {
		.name = "system0",
		.id = -1,
		.parent = &clk_system_source.clk,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { .reg = VA_PMGR0 + 0x40, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_system1 = {
	.clk = {
		.name = "system1",
		.id = -1,
		.parent = &clk_system0.clk,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { .reg = VA_PMGR0 + 0x40, .shift = SHIFT_DIV2, .size = SIZE_DIV },
};

static struct clksrc_clk clk_system2 = {
	.clk = {
		.name = "system2",
		.id = -1,
		.parent = &clk_system_source.clk,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { .reg = VA_PMGR0 + 0x40, .shift = SHIFT_DIV3, .size = SIZE_DIV },
};

static struct clksrc_clk clk_system3 = {
	.clk = {
		.name = "system3",
		.id = -1,
		.parent = &clk_system_source.clk,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { .reg = VA_PMGR0 + 0x40, .shift = SHIFT_DIV4, .size = SIZE_DIV },
};

static struct clksrc_clk clk_system4 = {
	.clk = {
		.name = "system4",
		.id = -1,
		.parent = &clk_system_source.clk,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { .reg = VA_PMGR0 + 0x40, .shift = SHIFT_DIV5, .size = SIZE_DIV },
};

static struct clk *clk_prediv_source_list[] = {
	&clk_system2.clk,
	&clk_system3.clk,
	&s5l_plls[2].clk,
	&clk_xtal,
};

static struct clksrc_sources clk_prediv_sources = {
	.sources = clk_prediv_source_list,
	.nr_sources = ARRAY_SIZE(clk_prediv_source_list),
};

static struct clksrc_clk clk_prediv0 = {
	.clk = { 
		.name = "prediv0",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_prediv_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x44, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x44, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_prediv1 = {
	.clk = { 
		.name = "prediv1",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_prediv_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x48, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x48, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_prediv2 = {
	.clk = { 
		.name = "prediv2",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_prediv_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x4C, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x4C, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_prediv3 = {
	.clk = { 
		.name = "prediv3",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_prediv_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x50, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x50, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_prediv4 = {
	.clk = { 
		.name = "prediv4",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_prediv_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x54, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x54, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clk *clk_base_source_list[] = {
	&clk_prediv0.clk,
	&clk_prediv1.clk,
	&clk_prediv2.clk,
	&clk_prediv3.clk,
};

static struct clksrc_sources clk_base_sources = {
	.sources = clk_base_source_list,
	.nr_sources = ARRAY_SIZE(clk_base_source_list),
};

static struct clksrc_clk clk_base0 = {
	.clk = {
		.name = "base0",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_base_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x70, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_base0_div0 = {
	.clk = {
		.name = "base0-div0",
		.id = -1,
		.parent = &clk_base0.clk,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { .reg = VA_PMGR0 + 0x70, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_base0_div1 = {
	.clk = {
		.name = "base0-div1",
		.id = -1,
		.parent = &clk_base0.clk,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { .reg = VA_PMGR0 + 0x70, .shift = SHIFT_DIV2, .size = SIZE_DIV },
};

static struct clksrc_clk clk_base1 = {
	.clk = {
		.name = "base1",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_base_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x74, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_base1_div0 = {
	.clk = {
		.name = "base1-div0",
		.id = -1,
		.parent = &clk_base1.clk,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { .reg = VA_PMGR0 + 0x74, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_base2 = {
	.clk = {
		.name = "base2",
		.id = -1,
		.parent = &clk_prediv4.clk,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { .reg = VA_PMGR0 + 0x78, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_medium0 = {
	.clk = {
		.name = "medium0",
		.id = -1,
		.enable = s5l_clock_always_on,
	},
	
	.sources = &clk_prediv_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x68, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x68, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_medium1 = {
	.clk = {
		.name = "medium1",
		.id = -1,
		.parent = &s5l_plls[3].clk,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { .reg = VA_PMGR0 + 0xCC, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_i2c_base = {
	.clk = {
		.name = "i2c-base",
		.id = -1,
		.parent = &clk_xtal,
		.enable = s5l_clock_always_on,
	},

	.reg_div = { VA_PMGR0 + 0xC4, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_nco_ref0 = {
	.clk = {
		.name = "nco-ref0",
		.id = -1,
		.enable = s5l_clock_always_on,
	},
	
	.sources = &clk_prediv_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x60, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x60, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_nco_ref1 = {
	.clk = {
		.name = "nco-ref1",
		.id = -1,
		.enable = s5l_clock_always_on,
	},
	
	.sources = &clk_prediv_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x64, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x64, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clk *clk_periph_sources_list0[] = {
	&clk_base0.clk,
	&clk_base0_div0.clk,
	&clk_base2.clk,
	&clk_base1.clk,
};

static struct clksrc_sources clk_periph_sources0 = {
	.sources = clk_periph_sources_list0,
	.nr_sources = ARRAY_SIZE(clk_periph_sources_list0),
};

static struct clk *clk_periph_sources_list1[] = {
	&clk_base1_div0.clk,
	&clk_base0_div0.clk,
	&clk_base0_div1.clk,
	&clk_base1.clk,
};

static struct clksrc_sources clk_periph_sources1 = {
	.sources = clk_periph_sources_list1,
	.nr_sources = ARRAY_SIZE(clk_periph_sources_list1),
};

static struct clksrc_clk clk_cdma = {
	.clk = {
		.name = "cdma",
		.id = -1,
		.enable = s5l_clock_gate_toggle,
		.ctrlbit = 0x14,
	},

	.sources = &clk_periph_sources0,
	.reg_src = { .reg = VA_PMGR0 + 0xA0, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0xA0, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

// Another nameless clock missing here... -- Ricky26

static struct clksrc_clk clk_hperf0 = {
	.clk = {
		.name = "hperf0",
		.id = -1,
		.enable = s5l_clock_gate_toggle_zone,
		.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF0, 0),
	},

	.sources = &clk_periph_sources0,
	.reg_src = { .reg = VA_PMGR0 + 0x98, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_hperf1 = {
	.clk = {
		.name = "hperf1",
		.id = -1,
		.enable = s5l_clock_gate_toggle_zone,
		.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF1, 0),
	},

	.sources = &clk_base_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x90, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x90, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_hperf2 = {
	.clk = {
		.name = "hperf2",
		.id = -1,
		.enable = s5l_clock_gate_toggle_zone,
		.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF2, 0),
	},

	.sources = &clk_periph_sources0,
	.reg_src = { .reg = VA_PMGR0 + 0x9C, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_mperf = { 
	.clk = {
		.name = "mperf",
		.id = -1,
		.enable = s5l_clock_gate_toggle_zone,
		.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_MPERF, 0),
	},

	.sources = &clk_base_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x8C, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x8C, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_lperf0 = {
	.clk = {
		.name = "lperf0",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_periph_sources1,
	.reg_src = { .reg = VA_PMGR0 + 0xA4, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_lperf1 = {
	.clk = {
		.name = "lperf1",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_periph_sources0,
	.reg_src = { .reg = VA_PMGR0 + 0xA8, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_lperf2 = {
	.clk = {
		.name = "lperf2",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_periph_sources1,
	.reg_src = { .reg = VA_PMGR0 + 0xAC, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_lperf3 = {
	.clk = {
		.name = "lperf3",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_periph_sources1,
	.reg_src = { .reg = VA_PMGR0 + 0xB0, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clk *clk_vid0_source_list[] = {
	&clk_prediv0.clk,
	&clk_prediv1.clk,
	&clk_prediv2.clk,
	&clk_prediv4.clk,
};

static struct clksrc_sources clk_vid0_sources = {
	.sources = clk_vid0_source_list,
	.nr_sources = ARRAY_SIZE(clk_vid0_source_list),
};

static struct clksrc_clk clk_vid0 = {
	.clk = {
		.name = "vid0",
		.id = -1,
		.enable = s5l_clock_gate_toggle_zone,
		.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF2, 0),
	},

	.sources = &clk_vid0_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x94, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x94, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_vid1 = {
	.clk = {
		.name = "vid1",
		.id = -1,
		.enable = s5l_clock_gate_toggle_zone,
		.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF2, 0),
	},

	.sources = &clk_prediv_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x5C, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x5C, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

// Another nameless clock here

static struct clksrc_clk clk_audio = {
	.clk = { 
		.name = "audio",
		.id = -1,
		.enable = s5l_clock_gate_toggle_zone,
		.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF2, 0x1f),
	},

	.sources = &clk_base_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x84, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x84, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_mipi = {
	.clk = {
		.name = "mipi",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_base_sources,
	.reg_src = { .reg = VA_PMGR0 + 0x88, .shift = SHIFT_SRC, .size = SIZE_SRC },
	.reg_div = { .reg = VA_PMGR0 + 0x88, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_sdio = {
	.clk = {
		.name = "sdio",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_periph_sources1,
	.reg_src = { .reg = VA_PMGR0 + 0xB8, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_clk50 = {
	.clk = {
		.name = "clk50",
		.id = -1,
		.enable = s5l_clock_always_on,
	},

	.sources = &clk_periph_sources1,
	.reg_src = { .reg = VA_PMGR0 + 0xBC, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clk *clk_spi_source_list[] = {
	&clk_medium0.clk,
	&clk_medium1.clk,
	&clk_xtal,
	&clk_xtal,
};

static struct clksrc_sources clk_spi_sources = {
	.sources = clk_spi_source_list,
	.nr_sources = ARRAY_SIZE(clk_spi_source_list),
};

static struct clksrc_clk clk_spi0 = {
	.clk = {
		.name = "spi",
		.id = 0,
		.enable = s5l_clock_gate_toggle,
		.ctrlbit = 0x2b,
	},

	.sources = &clk_spi_sources,
	.reg_src = { .reg = VA_PMGR0 + 0xDC, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_spi1 = {
	.clk = {
		.name = "spi",
		.id = 1,
		.enable = s5l_clock_gate_toggle,
		.ctrlbit = 0x2c,
	},

	.sources = &clk_spi_sources,
	.reg_src = { .reg = VA_PMGR0 + 0xE0, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_spi2 = {
	.clk = {
		.name = "spi",
		.id = 2,
		.enable = s5l_clock_gate_toggle,
		.ctrlbit = 0x2d,
	},

	.sources = &clk_spi_sources,
	.reg_src = { .reg = VA_PMGR0 + 0xE4, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_spi3 = {
	.clk = {
		.name = "spi",
		.id = 3,
		.enable = s5l_clock_gate_toggle,
		.ctrlbit = 0x2e,
	},

	.sources = &clk_spi_sources,
	.reg_src = { .reg = VA_PMGR0 + 0xEC, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_spi4 = {
	.clk = {
		.name = "spi",
		.id = 4,
		.enable = s5l_clock_gate_toggle,
		.ctrlbit = 0x2f,
	},

	.sources = &clk_spi_sources,
	.reg_src = { .reg = VA_PMGR0 + 0xEC, .shift = SHIFT_SRC, .size = SIZE_SRC },
};

static struct clksrc_clk clk_i2c0 = {
	.clk = {
		.name = "i2c",
		.id = 0,
		.parent = &clk_i2c_base.clk,
		.enable = s5l_clock_gate_toggle,
		.ctrlbit = 0x39,
	},

	.reg_div = { .reg = VA_PMGR0 + 0xF0, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_i2c1 = {
	.clk = {
		.name = "i2c",
		.id = 1,
		.parent = &clk_i2c_base.clk,
		.enable = s5l_clock_gate_toggle,
		.ctrlbit = 0x3a,
	},

	.reg_div = { .reg = VA_PMGR0 + 0xF4, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk clk_i2c2 = {
	.clk = {
		.name = "i2c",
		.id = 2,
		.parent = &clk_i2c_base.clk,
		.enable = s5l_clock_gate_toggle,
		.ctrlbit = 0x3b,
	},

	.reg_div = { .reg = VA_PMGR0 + 0xF8, .shift = SHIFT_DIV1, .size = SIZE_DIV },
};

static struct clksrc_clk *clksrcs[] = {
	&clk_system0,
	&clk_system1,
	&clk_system2,
	&clk_system3,
	&clk_system4,
	&clk_prediv0,
	&clk_prediv1,
	&clk_prediv2,
	&clk_prediv3,
	&clk_prediv4,
	&clk_base0,
	&clk_base0_div0,
	&clk_base0_div1,
	&clk_base1,
	&clk_base1_div0,
	&clk_base2,
	&clk_medium0,
	&clk_medium1,
	&clk_i2c_base,
	&clk_nco_ref0,
	&clk_nco_ref1,
	&clk_cdma,
	&clk_hperf0,
	&clk_hperf1,
	&clk_hperf2,
	&clk_mperf,
	&clk_lperf0,
	&clk_lperf1,
	&clk_lperf2,
	&clk_lperf3,
	&clk_vid0,
	&clk_vid1,
	&clk_audio,
	&clk_mipi,
	&clk_sdio,
	&clk_clk50,
	&clk_spi0,
	&clk_spi1,
	&clk_spi2,
	&clk_spi3,
	&clk_spi4,
	&clk_i2c0,
	&clk_i2c1,
	&clk_i2c2,
};

// Clock gates

static struct clk clk_usbreg = {
	.name = "usbreg",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_MPERF, 0x1d),
};

static struct clk clk_usb11 = {
	.name = "usb11",
	.id = -1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x1a,
	.parent = &clk_usbreg,
};

static struct clk clk_usbphy = {
	.name = "usbphy",
	.id = -1,
	.parent = &clk_usbreg,
	.enable = s5l_clock_always_on,
};

static struct clk clk_usbotg = {
	.name = "otg",
	.id = -1,
	.parent = &clk_usbreg,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_MPERF, 0x18),
};

static struct clk clk_usbehci = {
	.name = "usbehci",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_MPERF, 0x19),
};

static struct clk clk_usbohci0 = {
	.name = "usbohci0",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_MPERF, 0x1b),
	.parent = &clk_usb11,
};

static struct clk clk_usbohci1 = {
	.name = "usbohci1",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_MPERF, 0x1c),
	.parent = &clk_usb11,
};

static struct clk clk_sdio_wifi = {
	.name = "hsmmc",
	.id = 0,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x24,
};

static struct clk clk_sha = {
	.name = "sha",
	.id = -1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x25,
};

static struct clk clk_sdio_ceata = {
	.name = "hsmmc",
	.id = 1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x26,
};

static struct clk clk_fmi0 = {
	.name = "fmi0",
	.id = -1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x27,
};

static struct clk clk_fmi0_bch = {
	.name = "fmi0-bch",
	.id = -1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x28,
};

static struct clk clk_fmi1 = {
	.name = "fmi1",
	.id = -1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x29,
};

static struct clk clk_fmi1_bch = {
	.name = "fmi1-bch",
	.id = -1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x2A,
};

static struct clk clk_uart0 = {
	.name = "uart",
	.id = 0,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x30,
};

static struct clk clk_uart1 = {
	.name = "uart",
	.id = 1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x31,
};

static struct clk clk_uart2 = {
	.name = "uart",
	.id = 2,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x32,
};

static struct clk clk_uart3 = {
	.name = "uart",
	.id = 3,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x33,
};

static struct clk clk_uart4 = {
	.name = "uart",
	.id = 4,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x34,
};

static struct clk clk_uart5 = {
	.name = "uart",
	.id = 5,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x35,
};

static struct clk clk_uart6 = {
	.name = "uart",
	.id = 6,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x36,
};

static struct clk clk_i2s0 = {
	.name = "i2s",
	.id = 0,
	.parent = &clk_audio.clk,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x21,
};

static struct clk clk_i2s1 = {
	.name = "i2s",
	.id = 1,
	.parent = &clk_audio.clk,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x22,
};

static struct clk clk_i2s2 = {
	.name = "i2s",
	.id = 2,
	.parent = &clk_audio.clk,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x23,
};

static struct clk clk_spdif = {
	.name = "spdif",
	.id = -1,
	.parent = &clk_audio.clk,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x20,
};

static struct clk clk_pke = {
	.name = "pke",
	.id = -1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x38,
};

static struct clk clk_pwm = {
	.name = "pwm",
	.id = -1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x3c,
};

static struct clk clk_swi = {
	.name = "swi",
	.id = -1,
	.enable = s5l_clock_gate_toggle,
	.ctrlbit = 0x3e,
};

// Ethernet has source selection, but we don't know
// the sources. Also not hooked up in the devices
// we have. :) -- Ricky26
/*static struct clk clk_ethernet = {
	.name = "ethernet",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_MPERF, 0x17),
};*/

static struct clk clk_iop = {
	.name = "iop",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_MPERF, 0x16),
};

static struct clk clk_vxd = {
	.name = "vxd",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF0, 0x3),
};

static struct clk clk_sgx = {
	.name = "sgx",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF0, 0x4),
};

static struct clk clk_isp = {
	.name = "isp",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF1, 0x6),
};

static struct clk clk_jpeg = {
	.name = "jpeg",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF1, 0x7),
};

static struct clk clk_venc = {
	.name = "venc",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF1, 0x8),
};

static struct clk clk_mipi_csi = {
	.name = "mipi-csi",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF1, 0x9),
};

static struct clk clk_smia = {
	.name = "smia",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF1, 0xa),
};

static struct clk clk_clcd = {
	.name = "clcd",
	.id = -1,
	.enable = s5l_clock_gate_toggle_display,
	.ctrlbit = 0xf12,
};

static struct clk clk_mipi_dsi = {
	.name = "dsim",
	.id = -1,
	.parent = &clk_mipi.clk,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF2, 0x11),
};

static struct clk clk_rgbout = {
	.name = "rgbout",
	.id = -1,
	.enable = s5l_clock_gate_toggle_display,
	.ctrlbit = 0xd13,
};

static struct clk clk_tvout = {
	.name = "tvout",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF2, 0x10),
};

static struct clk clk_scaler = {
	.name = "scaler",
	.id = -1,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF2, 0xc),
};

static struct clk clk_displayport = {
	.name = "displayport",
	.id = -1,
	.parent = &clk_audio.clk,
	.enable = s5l_clock_gate_toggle_zone,
	.ctrlbit = S5L_MAKE_CLOCKGATE(ZONE_HPERF2, 0xe),
};

// Used by UART driver. -- Ricky26
static struct clk clk_uclk0 = {
	.name = "uclk0",
	.id = -1,
	.rate = 24000000,
};

struct clk clk_xusbxti = {
	.name = "xusbxti",
	.id = -1,
	.parent = &clk_xtal,
};

static struct clk *clocks_init[] = {
	&clk_uart0, // Used for debugging
	&clk_uclk0,
	&clk_xusbxti,
};

static struct clk *clocks_disable[] = {
	&clk_usb11,
	&clk_usbreg,
	&clk_usbphy,
	&clk_usbotg,
	&clk_usbehci,
	&clk_usbohci0,
	&clk_usbohci1,
	&clk_sdio_wifi,
	&clk_sdio_ceata,
	&clk_sha,
	&clk_uart1,
	&clk_uart2,
	&clk_uart3,
	&clk_uart4,
	&clk_uart5,
	&clk_uart6,
	&clk_fmi0,
	&clk_fmi0_bch,
	&clk_fmi1,
	&clk_fmi1_bch,
	&clk_i2s0,
	&clk_i2s1,
	&clk_i2s2,
	&clk_spdif,
	&clk_pke,
	&clk_pwm,
	&clk_swi,
	&clk_iop,
	&clk_vxd,
	&clk_sgx,
	&clk_isp,
	&clk_jpeg,
	&clk_venc,
	&clk_mipi_csi,
	&clk_mipi_dsi,
	&clk_smia,
	&clk_clcd,
	&clk_rgbout,
	&clk_tvout,
	&clk_scaler,
	&clk_displayport,
};

__init_or_cpufreq void s5l8930_setup_clocks(void)
{
}

__init void s5l8930_cpu_init_clocks(int _xtal)
{
	int i;

	s3c24xx_register_baseclocks(_xtal);
	clk_p.parent = &clk_lperf0.clk;

	// starts at 1 to not disable MPERF
	for(i = 0; i < ARRAY_SIZE(s5l_power_zones); i++)
	{
		if(i == 0)
			s5l_power_zone_toggle(&s5l_power_zones[i], 1);
		else
			s5l_power_zone_toggle(&s5l_power_zones[i], 0);
	}

	for(i = 0; i < ARRAY_SIZE(clksrcs); i++)
	{
		s3c_register_clksrc(clksrcs[i], 1);
		(clksrcs[i]->clk.enable)(&clksrcs[i]->clk, 1);
	}

	s3c24xx_register_clocks(clocks_init, ARRAY_SIZE(clocks_init));
	s3c24xx_register_clocks(clocks_disable, ARRAY_SIZE(clocks_disable));

	for(i = 0; i < ARRAY_SIZE(clocks_disable); i++)
		(clocks_disable[i]->enable)(clocks_disable[i], 0);

	s5l8930_setup_clocks();
}
