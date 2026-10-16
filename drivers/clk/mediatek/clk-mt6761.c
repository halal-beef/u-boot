// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek clock driver for MT6761 SoC
 *
 * Copyright (c) 2026 Umer Uddin <umer.uddin@mentallysanemainliners.org>
 */

#include <dm.h>
#include <asm/io.h>
#include <linux/build_bug.h>
#include <dt-bindings/clock/mt6761-clk.h>
#include "clk-mtk.h"

enum {
	CLK_PAD_CLK32K,
	CLK_PAD_CLK26M,
	CLK_PAD_ULPOSC1,
};

static const ulong ext_clock_rates[] = {
	[CLK_PAD_CLK32K] = 32768,
	[CLK_PAD_CLK26M] = 26000000,
	[CLK_PAD_ULPOSC1] = 260000000,	/* nominal, only used for rate reporting */
};

#define AP_PLL_CON3		0x0c
#define PLLON_CON0		0x44
#define PLLON_CON1		0x48

#define MT6761_PLL_FMAX		(3800UL * MHZ)
#define MT6761_PLL_FMIN		(1500UL * MHZ)
#define CON0_MT6761_RST_BAR	BIT(23)
#define PLL_AO			BIT(1)	/* informational, not used by the core */

#define PLL(_id, _reg, _pwr_reg, _en_mask, _flags, _pcwbits, _pd_reg,	\
	    _pd_shift, _pcw_reg, _pcw_shift, _rst_bar_mask, _pcw_chg_reg) {	\
		.id = _id,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.flags = _flags,					\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6761_PLL_FMAX,				\
		.fmin = MT6761_PLL_FMIN,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = 8,						\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcw_chg_reg = _pcw_chg_reg,				\
	}

static const struct mtk_pll_data apmixed_plls[] = {
	PLL(CLK_APMIXED_ARMPLL, 0x030C, 0x0318, 0x1, PLL_AO, 22,
	    0x0310, 24, 0x0310, 0, 0, 0),
	PLL(CLK_APMIXED_MAINPLL, 0x0228, 0x0234, 0x1,
	    CLK_PLL_HAVE_RST_BAR | PLL_AO, 22,
	    0x022C, 24, 0x022C, 0, CON0_MT6761_RST_BAR, 0),
	PLL(CLK_APMIXED_MFGPLL, 0x0218, 0x0224, 0x1, 0, 22,
	    0x021C, 24, 0x021C, 0, 0, 0),
	PLL(CLK_APMIXED_MMPLL, 0x0330, 0x033C, 0x1, 0, 22,
	    0x0334, 24, 0x0334, 0, 0, 0),
	PLL(CLK_APMIXED_UNIV2PLL, 0x0208, 0x0214, 0x1,
	    CLK_PLL_HAVE_RST_BAR, 22,
	    0x020C, 24, 0x020C, 0, CON0_MT6761_RST_BAR, 0),
	PLL(CLK_APMIXED_MSDCPLL, 0x0350, 0x035C, 0x1, 0, 22,
	    0x0354, 24, 0x0354, 0, 0, 0),
	/* 32-bit PCW: bit31 of 0x0324 is data, so PCW_CHG lives in CON1 (0x0320) */
	PLL(CLK_APMIXED_APLL1, 0x031C, 0x032C, 0x1, 0, 32,
	    0x0320, 24, 0x0324, 0, 0, 0x0320),
	PLL(CLK_APMIXED_MPLL, 0x0340, 0x034C, 0x1, PLL_AO, 22,
	    0x0344, 24, 0x0344, 0, 0, 0),
};

static const struct mtk_gate_regs apmixed_cg_regs = {
	.set_ofs = 0x14,
	.clr_ofs = 0x14,
	.sta_ofs = 0x14,
};

#define GATE_APMIXED(_id, _shift)					\
	GATE_FLAGS(_id, CLK_PAD_CLK26M, &apmixed_cg_regs, _shift,	\
		   CLK_GATE_NO_SETCLR_INV | CLK_PARENT_EXT)

static const struct mtk_gate apmixed_clks[] = {
	GATE_APMIXED(CLK_APMIXED_SSUSB26M, 4),
	GATE_APMIXED(CLK_APMIXED_APPLL26M, 5),
	GATE_APMIXED(CLK_APMIXED_MIPIC0_26M, 6),
	GATE_APMIXED(CLK_APMIXED_MDPLLGP26M, 7),
	GATE_APMIXED(CLK_APMIXED_MMSYS_F26M, 8),
	GATE_APMIXED(CLK_APMIXED_UFS26M, 9),
	GATE_APMIXED(CLK_APMIXED_MIPIC1_26M, 11),
	GATE_APMIXED(CLK_APMIXED_MEMPLL26M, 13),
	GATE_APMIXED(CLK_APMIXED_CLKSQ_LVPLL_26M, 14),
	GATE_APMIXED(CLK_APMIXED_MIPID0_26M, 16),
};

static const struct mtk_clk_tree mt6761_apmixed_tree = {
	.pll_parent = EXT_PARENT(CLK_PAD_CLK26M),
	.ext_clk_rates = ext_clock_rates,
	.num_ext_clks = ARRAY_SIZE(ext_clock_rates),
	.gates_offs = CLK_APMIXED_SSUSB26M,
	.plls = apmixed_plls,
	.gates = apmixed_clks,
	.num_plls = ARRAY_SIZE(apmixed_plls),
	.num_gates = ARRAY_SIZE(apmixed_clks),
	.flags = CLK_PARENT_APMIXED,
	.type = MTK_CLK_TREE_APMIXED,
};

/* clk cfg update */
#define CLK_CFG_0		0x40
#define CLK_CFG_0_SET		0x44
#define CLK_CFG_0_CLR		0x48
#define CLK_CFG_1		0x50
#define CLK_CFG_1_SET		0x54
#define CLK_CFG_1_CLR		0x58
#define CLK_CFG_2		0x60
#define CLK_CFG_2_SET		0x64
#define CLK_CFG_2_CLR		0x68
#define CLK_CFG_3		0x70
#define CLK_CFG_3_SET		0x74
#define CLK_CFG_3_CLR		0x78
#define CLK_CFG_4		0x80
#define CLK_CFG_4_SET		0x84
#define CLK_CFG_4_CLR		0x88
#define CLK_CFG_5		0x90
#define CLK_CFG_5_SET		0x94
#define CLK_CFG_5_CLR		0x98
#define CLK_CFG_6		0xa0
#define CLK_CFG_6_SET		0xa4
#define CLK_CFG_6_CLR		0xa8
#define CLK_CFG_7		0xb0
#define CLK_CFG_7_SET		0xb4
#define CLK_CFG_7_CLR		0xb8
#define CLK_CFG_UPDATE		0x004

#define CLK_SCP_CFG_0		0x200
#define CLK_SCP_CFG_1		0x204

static const struct mtk_fixed_clk top_fixed_clks[] = {
	FIXED_CLK(CLK_TOP_CLK32K, CLK_PAD_CLK32K, CLK_PARENT_EXT, 32768),
	FIXED_CLK(CLK_TOP_CLK26M, CLK_PAD_CLK26M, CLK_PARENT_EXT, 26000000),
	FIXED_CLK(CLK_TOP_DMPLL, CLK_PAD_CLK26M, CLK_PARENT_EXT, 466000000),
};

#define FACTOR_AP(_id, _parent, _mult, _div)				\
	FACTOR(_id, _parent, _mult, _div, CLK_PARENT_APMIXED)
#define FACTOR_TOP(_id, _parent, _mult, _div)				\
	FACTOR(_id, _parent, _mult, _div, CLK_PARENT_TOPCKGEN)
#define FACTOR_EXT(_id, _parent, _mult, _div)				\
	FACTOR(_id, _parent, _mult, _div, CLK_PARENT_EXT)

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR_AP(CLK_TOP_SYSPLL, CLK_APMIXED_MAINPLL, 1, 1),
	FACTOR_AP(CLK_TOP_SYSPLL_D2, CLK_APMIXED_MAINPLL, 1, 2),
	FACTOR_TOP(CLK_TOP_SYSPLL1_D2, CLK_TOP_SYSPLL_D2, 1, 2),
	FACTOR_TOP(CLK_TOP_SYSPLL1_D4, CLK_TOP_SYSPLL_D2, 1, 4),
	FACTOR_TOP(CLK_TOP_SYSPLL1_D8, CLK_TOP_SYSPLL_D2, 1, 8),
	FACTOR_TOP(CLK_TOP_SYSPLL1_D16, CLK_TOP_SYSPLL_D2, 1, 16),
	FACTOR_AP(CLK_TOP_SYSPLL_D3, CLK_APMIXED_MAINPLL, 1, 3),
	FACTOR_TOP(CLK_TOP_SYSPLL2_D2, CLK_TOP_SYSPLL_D3, 1, 2),
	FACTOR_TOP(CLK_TOP_SYSPLL2_D4, CLK_TOP_SYSPLL_D3, 1, 4),
	FACTOR_TOP(CLK_TOP_SYSPLL2_D8, CLK_TOP_SYSPLL_D3, 1, 8),
	FACTOR_AP(CLK_TOP_SYSPLL_D5, CLK_APMIXED_MAINPLL, 1, 5),
	FACTOR_TOP(CLK_TOP_SYSPLL3_D2, CLK_TOP_SYSPLL_D5, 1, 2),
	FACTOR_TOP(CLK_TOP_SYSPLL3_D4, CLK_TOP_SYSPLL_D5, 1, 4),
	FACTOR_AP(CLK_TOP_SYSPLL_D7, CLK_APMIXED_MAINPLL, 1, 7),
	FACTOR_TOP(CLK_TOP_SYSPLL4_D2, CLK_TOP_SYSPLL_D7, 1, 2),
	FACTOR_TOP(CLK_TOP_SYSPLL4_D4, CLK_TOP_SYSPLL_D7, 1, 4),
	FACTOR_AP(CLK_TOP_UNIVPLL, CLK_APMIXED_UNIV2PLL, 1, 2),
	FACTOR_TOP(CLK_TOP_USB20_192M, CLK_TOP_UNIVPLL, 2, 13),
	FACTOR_TOP(CLK_TOP_USB20_192M_D4, CLK_TOP_USB20_192M, 1, 4),
	FACTOR_TOP(CLK_TOP_USB20_192M_D8, CLK_TOP_USB20_192M, 1, 8),
	FACTOR_TOP(CLK_TOP_USB20_192M_D16, CLK_TOP_USB20_192M, 1, 16),
	FACTOR_TOP(CLK_TOP_USB20_192M_D32, CLK_TOP_USB20_192M, 1, 32),
	FACTOR_TOP(CLK_TOP_USB_PHY48M, CLK_TOP_USB20_192M, 1, 4),
	FACTOR_TOP(CLK_TOP_UNIVPLL_D2, CLK_TOP_UNIVPLL, 1, 2),
	FACTOR_TOP(CLK_TOP_UNIVPLL1_D2, CLK_TOP_UNIVPLL_D2, 1, 2),
	FACTOR_TOP(CLK_TOP_UNIVPLL1_D4, CLK_TOP_UNIVPLL_D2, 1, 4),
	FACTOR_TOP(CLK_TOP_UNIVPLL_D3, CLK_TOP_UNIVPLL, 1, 3),
	FACTOR_TOP(CLK_TOP_UNIVPLL2_D2, CLK_TOP_UNIVPLL_D3, 1, 2),
	FACTOR_TOP(CLK_TOP_UNIVPLL2_D4, CLK_TOP_UNIVPLL_D3, 1, 4),
	FACTOR_TOP(CLK_TOP_UNIVPLL2_D8, CLK_TOP_UNIVPLL_D3, 1, 8),
	FACTOR_TOP(CLK_TOP_UNIVPLL2_D32, CLK_TOP_UNIVPLL_D3, 1, 32),
	FACTOR_TOP(CLK_TOP_UNIVPLL_D5, CLK_TOP_UNIVPLL, 1, 5),
	FACTOR_TOP(CLK_TOP_UNIVPLL3_D2, CLK_TOP_UNIVPLL_D5, 1, 2),
	FACTOR_TOP(CLK_TOP_UNIVPLL3_D4, CLK_TOP_UNIVPLL_D5, 1, 4),
	FACTOR_AP(CLK_TOP_MMPLL, CLK_APMIXED_MMPLL, 1, 1),
	FACTOR_AP(CLK_TOP_MMPLL_D2, CLK_APMIXED_MMPLL, 1, 2),
	FACTOR_AP(CLK_TOP_MPLL_104M, CLK_APMIXED_MPLL, 1, 2),
	FACTOR_AP(CLK_TOP_MPLL_52M, CLK_APMIXED_MPLL, 1, 4),
	FACTOR_AP(CLK_TOP_MFGPLL, CLK_APMIXED_MFGPLL, 1, 1),
	FACTOR_AP(CLK_TOP_MSDCPLL, CLK_APMIXED_MSDCPLL, 1, 1),
	FACTOR_AP(CLK_TOP_MSDCPLL_D2, CLK_APMIXED_MSDCPLL, 1, 2),
	FACTOR_AP(CLK_TOP_APLL1, CLK_APMIXED_APLL1, 1, 1),
	FACTOR_AP(CLK_TOP_APLL1_D2, CLK_APMIXED_APLL1, 1, 2),
	FACTOR_AP(CLK_TOP_APLL1_D4, CLK_APMIXED_APLL1, 1, 4),
	FACTOR_AP(CLK_TOP_APLL1_D8, CLK_APMIXED_APLL1, 1, 8),
	FACTOR_EXT(CLK_TOP_ULPOSC1_D2, CLK_PAD_ULPOSC1, 1, 2),
	FACTOR_EXT(CLK_TOP_ULPOSC1_D4, CLK_PAD_ULPOSC1, 1, 4),
	FACTOR_EXT(CLK_TOP_ULPOSC1_D8, CLK_PAD_ULPOSC1, 1, 8),
	FACTOR_EXT(CLK_TOP_ULPOSC1_D16, CLK_PAD_ULPOSC1, 1, 16),
	FACTOR_EXT(CLK_TOP_ULPOSC1_D32, CLK_PAD_ULPOSC1, 1, 32),
	FACTOR_TOP(CLK_TOP_DA_USB20_48M_DIV, CLK_TOP_USB_PHY48M, 1, 1),
	FACTOR_TOP(CLK_TOP_DA_UNIV_48M_DIV, CLK_TOP_USB_PHY48M, 1, 1),
	FACTOR_TOP(CLK_TOP_DA_MPLL_104M_DIV, CLK_TOP_MPLL_104M, 1, 1),
	FACTOR_TOP(CLK_TOP_DA_MPLL_52M_DIV, CLK_TOP_MPLL_52M, 1, 1),
	FACTOR_TOP(CLK_TOP_CSW_FAXI, CLK_TOP_AXI_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_CSW_FMSDC30_1, CLK_TOP_MSDC30_1_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_CSW_FMSDC30_2, CLK_TOP_MSDC50_0_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_AXI, CLK_TOP_AXI_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_MEM, CLK_TOP_MEM_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_MM, CLK_TOP_MM_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_SCP, CLK_TOP_SCP_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_MFG, CLK_TOP_MFG_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_F_FUART, CLK_TOP_UART_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_SPI, CLK_TOP_SPI_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_MSDC50_0, CLK_TOP_MSDC50_0_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_AUDIO, CLK_TOP_AUDIO_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_AUD_1, CLK_TOP_AUD_1_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_AUD_ENGEN1, CLK_TOP_AUD_ENGEN1_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_F_FDISP_PWM, CLK_TOP_DISP_PWM_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_SSPM, CLK_TOP_SSPM_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_DXCC, CLK_TOP_DXCC_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_I2C, CLK_TOP_I2C_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_F_FPWM, CLK_TOP_PWM_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_F_FSENINF, CLK_TOP_SENINF_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_AES_FDE, CLK_TOP_AES_FDE_SEL, 1, 1),
	FACTOR_TOP(CLK_TOP_F_BIST2FPC, CLK_TOP_UNIVPLL2_D2, 1, 1),
	FACTOR_TOP(CLK_TOP_ARMPLL_DIVIDER_PLL0, CLK_TOP_SYSPLL_D2, 1, 1),
	FACTOR_TOP(CLK_TOP_ARMPLL_DIVIDER_PLL1, CLK_TOP_SYSPLL, 1, 1),
	FACTOR_TOP(CLK_TOP_ARMPLL_DIVIDER_PLL2, CLK_TOP_UNIVPLL_D2, 1, 1),
	FACTOR_EXT(CLK_TOP_UFS_TICK1US, CLK_PAD_CLK26M, 1, 1),
	FACTOR_EXT(CLK_TOP_CLK13M, CLK_PAD_CLK26M, 1, 2),
};

static const struct mtk_parent axi_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_SYSPLL_D7),
	TOP_PARENT(CLK_TOP_SYSPLL1_D4),
	TOP_PARENT(CLK_TOP_SYSPLL3_D2),
};

static const struct mtk_parent mem_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_DMPLL),
	TOP_PARENT(CLK_TOP_APLL1),
};

static const struct mtk_parent mm_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_MMPLL),
	TOP_PARENT(CLK_TOP_SYSPLL1_D2),
	TOP_PARENT(CLK_TOP_SYSPLL_D5),
	TOP_PARENT(CLK_TOP_SYSPLL1_D4),
	TOP_PARENT(CLK_TOP_UNIVPLL_D5),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D2),
	TOP_PARENT(CLK_TOP_MMPLL_D2),
};

static const struct mtk_parent scp_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_SYSPLL4_D2),
	TOP_PARENT(CLK_TOP_UNIVPLL2_D2),
	TOP_PARENT(CLK_TOP_SYSPLL1_D2),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D2),
	TOP_PARENT(CLK_TOP_SYSPLL_D3),
	TOP_PARENT(CLK_TOP_UNIVPLL_D3),
};

static const struct mtk_parent mfg_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_MFGPLL),
	TOP_PARENT(CLK_TOP_SYSPLL_D3),
	TOP_PARENT(CLK_TOP_UNIVPLL_D3),
};

static const struct mtk_parent atb_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_SYSPLL1_D4),
	TOP_PARENT(CLK_TOP_SYSPLL1_D2),
};

static const struct mtk_parent camtg_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_USB20_192M_D8),
	TOP_PARENT(CLK_TOP_UNIVPLL2_D8),
	TOP_PARENT(CLK_TOP_USB20_192M_D4),
	TOP_PARENT(CLK_TOP_UNIVPLL2_D32),
	TOP_PARENT(CLK_TOP_USB20_192M_D16),
	TOP_PARENT(CLK_TOP_USB20_192M_D32),
};

static const struct mtk_parent uart_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_UNIVPLL2_D8),
};

static const struct mtk_parent spi_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_SYSPLL3_D2),
	TOP_PARENT(CLK_TOP_SYSPLL4_D2),
	TOP_PARENT(CLK_TOP_SYSPLL2_D4),
};

static const struct mtk_parent msdc5hclk_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_SYSPLL1_D2),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D4),
	TOP_PARENT(CLK_TOP_SYSPLL2_D2),
};

static const struct mtk_parent msdc50_0_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_MSDCPLL),
	TOP_PARENT(CLK_TOP_SYSPLL2_D2),
	TOP_PARENT(CLK_TOP_SYSPLL4_D2),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D2),
	TOP_PARENT(CLK_TOP_SYSPLL1_D2),
	TOP_PARENT(CLK_TOP_UNIVPLL_D5),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D4),
};

static const struct mtk_parent msdc30_1_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_MSDCPLL_D2),
	TOP_PARENT(CLK_TOP_UNIVPLL2_D2),
	TOP_PARENT(CLK_TOP_SYSPLL2_D2),
	TOP_PARENT(CLK_TOP_SYSPLL1_D4),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D4),
	TOP_PARENT(CLK_TOP_USB20_192M_D4),
	TOP_PARENT(CLK_TOP_SYSPLL2_D4),
};

static const struct mtk_parent audio_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_SYSPLL3_D4),
	TOP_PARENT(CLK_TOP_SYSPLL4_D4),
	TOP_PARENT(CLK_TOP_SYSPLL1_D16),
};

static const struct mtk_parent aud_intbus_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_SYSPLL1_D4),
	TOP_PARENT(CLK_TOP_SYSPLL4_D2),
};

static const struct mtk_parent aud_1_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_APLL1),
};

static const struct mtk_parent aud_engen1_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_APLL1_D2),
	TOP_PARENT(CLK_TOP_APLL1_D4),
	TOP_PARENT(CLK_TOP_APLL1_D8),
};

static const struct mtk_parent disp_pwm_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_UNIVPLL2_D4),
	TOP_PARENT(CLK_TOP_ULPOSC1_D2),
	TOP_PARENT(CLK_TOP_ULPOSC1_D8),
};

static const struct mtk_parent sspm_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_SYSPLL1_D2),
	TOP_PARENT(CLK_TOP_SYSPLL_D3),
};

static const struct mtk_parent dxcc_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_SYSPLL1_D2),
	TOP_PARENT(CLK_TOP_SYSPLL1_D4),
	TOP_PARENT(CLK_TOP_SYSPLL1_D8),
};

static const struct mtk_parent usb_top_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_UNIVPLL3_D4),
};

static const struct mtk_parent spm_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_SYSPLL1_D8),
};

static const struct mtk_parent i2c_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_UNIVPLL3_D4),
	TOP_PARENT(CLK_TOP_UNIVPLL3_D2),
	TOP_PARENT(CLK_TOP_SYSPLL1_D8),
	TOP_PARENT(CLK_TOP_SYSPLL2_D8),
};

static const struct mtk_parent pwm_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_UNIVPLL3_D4),
	TOP_PARENT(CLK_TOP_SYSPLL1_D8),
};

static const struct mtk_parent seninf_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D4),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D2),
	TOP_PARENT(CLK_TOP_UNIVPLL2_D2),
};

static const struct mtk_parent aes_fde_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_MSDCPLL),
	TOP_PARENT(CLK_TOP_UNIVPLL_D3),
	TOP_PARENT(CLK_TOP_UNIVPLL2_D2),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D2),
	TOP_PARENT(CLK_TOP_SYSPLL1_D2),
};

static const struct mtk_parent ulposc_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_ULPOSC1_D4),
	TOP_PARENT(CLK_TOP_ULPOSC1_D8),
	TOP_PARENT(CLK_TOP_ULPOSC1_D16),
	TOP_PARENT(CLK_TOP_ULPOSC1_D32),
};

static const struct mtk_parent camtm_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D4),
	TOP_PARENT(CLK_TOP_UNIVPLL1_D2),
	TOP_PARENT(CLK_TOP_UNIVPLL2_D2),
};

#define MUX_GATE_CLR_SET_UPD(_id, _parents, _mux, _set, _clr, _shift,	\
			     _width, _gate, _upd_ofs, _upd)		\
	MUX_CLR_SET_UPD_FLAGS(_id, _parents, _mux, _set, _clr, _shift,	\
			      _width, _gate, _upd_ofs, _upd,		\
			      CLK_MUX_SETCLR_UPD)

static const struct mtk_composite top_muxes[] = {
	/* CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AXI_SEL, axi_parents, CLK_CFG_0,
			     CLK_CFG_0_SET, CLK_CFG_0_CLR, 0, 2, 7,
			     CLK_CFG_UPDATE, 0),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MEM_SEL, mem_parents, CLK_CFG_0,
			     CLK_CFG_0_SET, CLK_CFG_0_CLR, 8, 2, 15,
			     CLK_CFG_UPDATE, 1),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MM_SEL, mm_parents, CLK_CFG_0,
			     CLK_CFG_0_SET, CLK_CFG_0_CLR, 16, 3, 23,
			     CLK_CFG_UPDATE, 2),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SCP_SEL, scp_parents, CLK_CFG_0,
			     CLK_CFG_0_SET, CLK_CFG_0_CLR, 24, 3, 31,
			     CLK_CFG_UPDATE, 3),
	/* CLK_CFG_1 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_SEL, mfg_parents, CLK_CFG_1,
			     CLK_CFG_1_SET, CLK_CFG_1_CLR, 0, 2, 7,
			     CLK_CFG_UPDATE, 4),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ATB_SEL, atb_parents, CLK_CFG_1,
			     CLK_CFG_1_SET, CLK_CFG_1_CLR, 8, 2, 15,
			     CLK_CFG_UPDATE, 5),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG_SEL, camtg_parents, CLK_CFG_1,
			     CLK_CFG_1_SET, CLK_CFG_1_CLR, 16, 3, 23,
			     CLK_CFG_UPDATE, 6),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG1_SEL, camtg_parents, CLK_CFG_1,
			     CLK_CFG_1_SET, CLK_CFG_1_CLR, 24, 3, 31,
			     CLK_CFG_UPDATE, 7),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL, camtg_parents, CLK_CFG_2,
			     CLK_CFG_2_SET, CLK_CFG_2_CLR, 0, 3, 7,
			     CLK_CFG_UPDATE, 8),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL, camtg_parents, CLK_CFG_2,
			     CLK_CFG_2_SET, CLK_CFG_2_CLR, 8, 3, 15,
			     CLK_CFG_UPDATE, 9),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL, uart_parents, CLK_CFG_2,
			     CLK_CFG_2_SET, CLK_CFG_2_CLR, 16, 1, 23,
			     CLK_CFG_UPDATE, 10),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI_SEL, spi_parents, CLK_CFG_2,
			     CLK_CFG_2_SET, CLK_CFG_2_CLR, 24, 2, 31,
			     CLK_CFG_UPDATE, 11),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL, msdc5hclk_parents,
			     CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
			     0, 2, 7, CLK_CFG_UPDATE, 12),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL, msdc50_0_parents,
			     CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
			     8, 3, 15, CLK_CFG_UPDATE, 13),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL, msdc30_1_parents,
			     CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
			     16, 3, 23, CLK_CFG_UPDATE, 14),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_SEL, audio_parents, CLK_CFG_3,
			     CLK_CFG_3_SET, CLK_CFG_3_CLR, 24, 2, 31,
			     CLK_CFG_UPDATE, 15),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL, aud_intbus_parents,
			     CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
			     0, 2, 7, CLK_CFG_UPDATE, 16),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL, aud_1_parents, CLK_CFG_4,
			     CLK_CFG_4_SET, CLK_CFG_4_CLR, 8, 1, 15,
			     CLK_CFG_UPDATE, 17),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL, aud_engen1_parents,
			     CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
			     16, 2, 23, CLK_CFG_UPDATE, 18),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL, disp_pwm_parents,
			     CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
			     24, 2, 31, CLK_CFG_UPDATE, 19),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSPM_SEL, sspm_parents, CLK_CFG_5,
			     CLK_CFG_5_SET, CLK_CFG_5_CLR, 0, 2, 7,
			     CLK_CFG_UPDATE, 20),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DXCC_SEL, dxcc_parents, CLK_CFG_5,
			     CLK_CFG_5_SET, CLK_CFG_5_CLR, 8, 2, 15,
			     CLK_CFG_UPDATE, 21),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL, usb_top_parents,
			     CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
			     16, 1, 23, CLK_CFG_UPDATE, 22),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPM_SEL, spm_parents, CLK_CFG_5,
			     CLK_CFG_5_SET, CLK_CFG_5_CLR, 24, 1, 31,
			     CLK_CFG_UPDATE, 23),
	/* CLK_CFG_6 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL, i2c_parents, CLK_CFG_6,
			     CLK_CFG_6_SET, CLK_CFG_6_CLR, 0, 3, 7,
			     CLK_CFG_UPDATE, 24),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL, pwm_parents, CLK_CFG_6,
			     CLK_CFG_6_SET, CLK_CFG_6_CLR, 8, 2, 15,
			     CLK_CFG_UPDATE, 25),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF_SEL, seninf_parents, CLK_CFG_6,
			     CLK_CFG_6_SET, CLK_CFG_6_CLR, 16, 2, 23,
			     CLK_CFG_UPDATE, 26),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_FDE_SEL, aes_fde_parents,
			     CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
			     24, 3, 31, CLK_CFG_UPDATE, 27),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWRAP_ULPOSC_SEL, ulposc_parents,
			     CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
			     0, 3, 7, CLK_CFG_UPDATE, 28),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTM_SEL, camtm_parents, CLK_CFG_7,
			     CLK_CFG_7_SET, CLK_CFG_7_CLR, 8, 2, 15,
			     CLK_CFG_UPDATE, 29),
};

static const struct mtk_gate_regs top0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs top1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x104,
	.sta_ofs = 0x104,
};

static const struct mtk_gate_regs top2_cg_regs = {
	.set_ofs = 0x320,
	.clr_ofs = 0x320,
	.sta_ofs = 0x320,
};

/* TOP0 */
#define GATE_TOP0(_id, _parent, _shift)					\
	GATE_FLAGS(_id, _parent, &top0_cg_regs, _shift,			\
		   CLK_GATE_NO_SETCLR | CLK_PARENT_TOPCKGEN)
#define GATE_TOP0_EXT(_id, _parent, _shift)				\
	GATE_FLAGS(_id, _parent, &top0_cg_regs, _shift,			\
		   CLK_GATE_NO_SETCLR | CLK_PARENT_EXT)
/* TOP1 */
#define GATE_TOP1(_id, _parent, _shift)					\
	GATE_FLAGS(_id, _parent, &top1_cg_regs, _shift,			\
		   CLK_GATE_NO_SETCLR_INV | CLK_PARENT_TOPCKGEN)
#define GATE_TOP1_EXT(_id, _parent, _shift)				\
	GATE_FLAGS(_id, _parent, &top1_cg_regs, _shift,			\
		   CLK_GATE_NO_SETCLR_INV | CLK_PARENT_EXT)
#define GATE_TOP1_AP(_id, _parent, _shift)				\
	GATE_FLAGS(_id, _parent, &top1_cg_regs, _shift,			\
		   CLK_GATE_NO_SETCLR_INV | CLK_PARENT_APMIXED)
/* TOP2 */
#define GATE_TOP2(_id, _parent, _shift)					\
	GATE_FLAGS(_id, _parent, &top2_cg_regs, _shift,			\
		   CLK_GATE_NO_SETCLR | CLK_PARENT_TOPCKGEN)
#define GATE_TOP2_EXT(_id, _parent, _shift)				\
	GATE_FLAGS(_id, _parent, &top2_cg_regs, _shift,			\
		   CLK_GATE_NO_SETCLR | CLK_PARENT_EXT)

static const struct mtk_gate top_clks[] = {
	/* TOP0 */
	GATE_TOP0_EXT(CLK_TOP_MD_32K, CLK_PAD_CLK32K, 8),
	GATE_TOP0_EXT(CLK_TOP_MD_26M, CLK_PAD_CLK26M, 9),
	GATE_TOP0_EXT(CLK_TOP_CONN_32K, CLK_PAD_CLK32K, 10),
	GATE_TOP0_EXT(CLK_TOP_CONN_26M, CLK_PAD_CLK26M, 11),
	/* TOP1 */
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL0_EN,
		  CLK_TOP_ARMPLL_DIVIDER_PLL0, 3),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL1_EN,
		  CLK_TOP_ARMPLL_DIVIDER_PLL1, 4),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL2_EN,
		  CLK_TOP_ARMPLL_DIVIDER_PLL2, 5),
	GATE_TOP1(CLK_TOP_FMEM_OCC_DRC_EN, CLK_TOP_UNIVPLL2_D2, 6),
	GATE_TOP1(CLK_TOP_USB20_48M_EN, CLK_TOP_DA_USB20_48M_DIV, 8),
	GATE_TOP1(CLK_TOP_UNIVPLL_48M_EN, CLK_TOP_DA_UNIV_48M_DIV, 9),
	GATE_TOP1_EXT(CLK_TOP_F_UFS_MP_SAP_CFG_EN, CLK_PAD_CLK26M, 12),
	GATE_TOP1(CLK_TOP_F_BIST2FPC_EN, CLK_TOP_F_BIST2FPC, 16),
	GATE_TOP1_EXT(CLK_TOP_QS_CG, CLK_PAD_CLK26M, 17),
	GATE_TOP1_AP(CLK_TOP_ARMPLL_DIV_PLL, CLK_APMIXED_MAINPLL, 4),
	/* TOP2 */
	GATE_TOP2(CLK_TOP_APLL12_DIV0, CLK_TOP_AUD_1, 2),
	GATE_TOP2(CLK_TOP_APLL12_DIV1, CLK_TOP_AUD_1, 3),
	GATE_TOP2(CLK_TOP_APLL12_DIV2, CLK_TOP_AUD_1, 4),
	GATE_TOP2(CLK_TOP_APLL12_DIV3, CLK_TOP_AUD_1, 5),
	GATE_TOP2_EXT(CLK_TOP_APLL12_DIV4, CLK_PAD_CLK26M, 6),
	GATE_TOP2_EXT(CLK_TOP_APLL12_DIVB, CLK_PAD_CLK26M, 7),
};

static const int top_id_offs_map[CLK_TOP_NR_CLK] = {
	[0 ... CLK_TOP_NR_CLK - 1] = -1,
	/* fclks */
	[CLK_TOP_CLK32K] = 0,
	[CLK_TOP_CLK26M] = 1,
	[CLK_TOP_DMPLL] = 2,
	/* fdivs */
	[CLK_TOP_SYSPLL] = 3,
	[CLK_TOP_SYSPLL_D2] = 4,
	[CLK_TOP_SYSPLL1_D2] = 5,
	[CLK_TOP_SYSPLL1_D4] = 6,
	[CLK_TOP_SYSPLL1_D8] = 7,
	[CLK_TOP_SYSPLL1_D16] = 8,
	[CLK_TOP_SYSPLL_D3] = 9,
	[CLK_TOP_SYSPLL2_D2] = 10,
	[CLK_TOP_SYSPLL2_D4] = 11,
	[CLK_TOP_SYSPLL2_D8] = 12,
	[CLK_TOP_SYSPLL_D5] = 13,
	[CLK_TOP_SYSPLL3_D2] = 14,
	[CLK_TOP_SYSPLL3_D4] = 15,
	[CLK_TOP_SYSPLL_D7] = 16,
	[CLK_TOP_SYSPLL4_D2] = 17,
	[CLK_TOP_SYSPLL4_D4] = 18,
	[CLK_TOP_UNIVPLL] = 19,
	[CLK_TOP_USB20_192M] = 20,
	[CLK_TOP_USB20_192M_D4] = 21,
	[CLK_TOP_USB20_192M_D8] = 22,
	[CLK_TOP_USB20_192M_D16] = 23,
	[CLK_TOP_USB20_192M_D32] = 24,
	[CLK_TOP_USB_PHY48M] = 25,
	[CLK_TOP_UNIVPLL_D2] = 26,
	[CLK_TOP_UNIVPLL1_D2] = 27,
	[CLK_TOP_UNIVPLL1_D4] = 28,
	[CLK_TOP_UNIVPLL_D3] = 29,
	[CLK_TOP_UNIVPLL2_D2] = 30,
	[CLK_TOP_UNIVPLL2_D4] = 31,
	[CLK_TOP_UNIVPLL2_D8] = 32,
	[CLK_TOP_UNIVPLL2_D32] = 33,
	[CLK_TOP_UNIVPLL_D5] = 34,
	[CLK_TOP_UNIVPLL3_D2] = 35,
	[CLK_TOP_UNIVPLL3_D4] = 36,
	[CLK_TOP_MMPLL] = 37,
	[CLK_TOP_MMPLL_D2] = 38,
	[CLK_TOP_MPLL_104M] = 39,
	[CLK_TOP_MPLL_52M] = 40,
	[CLK_TOP_MFGPLL] = 41,
	[CLK_TOP_MSDCPLL] = 42,
	[CLK_TOP_MSDCPLL_D2] = 43,
	[CLK_TOP_APLL1] = 44,
	[CLK_TOP_APLL1_D2] = 45,
	[CLK_TOP_APLL1_D4] = 46,
	[CLK_TOP_APLL1_D8] = 47,
	[CLK_TOP_ULPOSC1_D2] = 48,
	[CLK_TOP_ULPOSC1_D4] = 49,
	[CLK_TOP_ULPOSC1_D8] = 50,
	[CLK_TOP_ULPOSC1_D16] = 51,
	[CLK_TOP_ULPOSC1_D32] = 52,
	[CLK_TOP_DA_USB20_48M_DIV] = 53,
	[CLK_TOP_DA_UNIV_48M_DIV] = 54,
	[CLK_TOP_DA_MPLL_104M_DIV] = 55,
	[CLK_TOP_DA_MPLL_52M_DIV] = 56,
	[CLK_TOP_CSW_FAXI] = 57,
	[CLK_TOP_CSW_FMSDC30_1] = 58,
	[CLK_TOP_CSW_FMSDC30_2] = 59,
	[CLK_TOP_AXI] = 60,
	[CLK_TOP_MEM] = 61,
	[CLK_TOP_MM] = 62,
	[CLK_TOP_SCP] = 63,
	[CLK_TOP_MFG] = 64,
	[CLK_TOP_F_FUART] = 65,
	[CLK_TOP_SPI] = 66,
	[CLK_TOP_MSDC50_0] = 67,
	[CLK_TOP_AUDIO] = 68,
	[CLK_TOP_AUD_1] = 69,
	[CLK_TOP_AUD_ENGEN1] = 70,
	[CLK_TOP_F_FDISP_PWM] = 71,
	[CLK_TOP_SSPM] = 72,
	[CLK_TOP_DXCC] = 73,
	[CLK_TOP_I2C] = 74,
	[CLK_TOP_F_FPWM] = 75,
	[CLK_TOP_F_FSENINF] = 76,
	[CLK_TOP_AES_FDE] = 77,
	[CLK_TOP_F_BIST2FPC] = 78,
	[CLK_TOP_ARMPLL_DIVIDER_PLL0] = 79,
	[CLK_TOP_ARMPLL_DIVIDER_PLL1] = 80,
	[CLK_TOP_ARMPLL_DIVIDER_PLL2] = 81,
	[CLK_TOP_UFS_TICK1US] = 82,
	[CLK_TOP_CLK13M] = 83,
	/* muxes */
	[CLK_TOP_AXI_SEL] = 84,
	[CLK_TOP_MEM_SEL] = 85,
	[CLK_TOP_MM_SEL] = 86,
	[CLK_TOP_SCP_SEL] = 87,
	[CLK_TOP_MFG_SEL] = 88,
	[CLK_TOP_ATB_SEL] = 89,
	[CLK_TOP_CAMTG_SEL] = 90,
	[CLK_TOP_CAMTG1_SEL] = 91,
	[CLK_TOP_CAMTG2_SEL] = 92,
	[CLK_TOP_CAMTG3_SEL] = 93,
	[CLK_TOP_UART_SEL] = 94,
	[CLK_TOP_SPI_SEL] = 95,
	[CLK_TOP_MSDC50_0_HCLK_SEL] = 96,
	[CLK_TOP_MSDC50_0_SEL] = 97,
	[CLK_TOP_MSDC30_1_SEL] = 98,
	[CLK_TOP_AUDIO_SEL] = 99,
	[CLK_TOP_AUD_INTBUS_SEL] = 100,
	[CLK_TOP_AUD_1_SEL] = 101,
	[CLK_TOP_AUD_ENGEN1_SEL] = 102,
	[CLK_TOP_DISP_PWM_SEL] = 103,
	[CLK_TOP_SSPM_SEL] = 104,
	[CLK_TOP_DXCC_SEL] = 105,
	[CLK_TOP_USB_TOP_SEL] = 106,
	[CLK_TOP_SPM_SEL] = 107,
	[CLK_TOP_I2C_SEL] = 108,
	[CLK_TOP_PWM_SEL] = 109,
	[CLK_TOP_SENINF_SEL] = 110,
	[CLK_TOP_AES_FDE_SEL] = 111,
	[CLK_TOP_PWRAP_ULPOSC_SEL] = 112,
	[CLK_TOP_CAMTM_SEL] = 113,
	/* gates */
	[CLK_TOP_MD_32K] = 114,
	[CLK_TOP_MD_26M] = 115,
	[CLK_TOP_CONN_32K] = 116,
	[CLK_TOP_CONN_26M] = 117,
	[CLK_TOP_ARMPLL_DIVIDER_PLL0_EN] = 118,
	[CLK_TOP_ARMPLL_DIVIDER_PLL1_EN] = 119,
	[CLK_TOP_ARMPLL_DIVIDER_PLL2_EN] = 120,
	[CLK_TOP_FMEM_OCC_DRC_EN] = 121,
	[CLK_TOP_USB20_48M_EN] = 122,
	[CLK_TOP_UNIVPLL_48M_EN] = 123,
	[CLK_TOP_F_UFS_MP_SAP_CFG_EN] = 124,
	[CLK_TOP_F_BIST2FPC_EN] = 125,
	[CLK_TOP_QS_CG] = 126,
	[CLK_TOP_ARMPLL_DIV_PLL] = 127,
	[CLK_TOP_APLL12_DIV0] = 128,
	[CLK_TOP_APLL12_DIV1] = 129,
	[CLK_TOP_APLL12_DIV2] = 130,
	[CLK_TOP_APLL12_DIV3] = 131,
	[CLK_TOP_APLL12_DIV4] = 132,
	[CLK_TOP_APLL12_DIVB] = 133,
};

static const struct mtk_clk_tree mt6761_topckgen_tree = {
	.ext_clk_rates = ext_clock_rates,
	.num_ext_clks = ARRAY_SIZE(ext_clock_rates),
	.id_offs_map = top_id_offs_map,
	.id_offs_map_size = ARRAY_SIZE(top_id_offs_map),
	.fclks_offs = 0,
	.fdivs_offs = top_id_offs_map[CLK_TOP_SYSPLL],
	.muxes_offs = top_id_offs_map[CLK_TOP_AXI_SEL],
	.gates_offs = top_id_offs_map[CLK_TOP_MD_32K],
	.fclks = top_fixed_clks,
	.fdivs = top_divs,
	.muxes = top_muxes,
	.gates = top_clks,
	.num_fclks = ARRAY_SIZE(top_fixed_clks),
	.num_fdivs = ARRAY_SIZE(top_divs),
	.num_muxes = ARRAY_SIZE(top_muxes),
	.num_gates = ARRAY_SIZE(top_clks),
	.flags = CLK_PARENT_TOPCKGEN,
	.type = MTK_CLK_TREE_TOPCKGEN,
};

static const struct mtk_gate_regs ifr2_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifr3_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs ifr4_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs ifr5_cg_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

#define GATE_IFRX(_id, _parent, _shift, _regs)				\
	GATE_FLAGS(_id, _parent, _regs, _shift,				\
		   CLK_GATE_SETCLR | CLK_PARENT_TOPCKGEN)
#define GATE_IFRX_EXT(_id, _parent, _shift, _regs)			\
	GATE_FLAGS(_id, _parent, _regs, _shift,				\
		   CLK_GATE_SETCLR | CLK_PARENT_EXT)

#define GATE_IFR2(_id, _parent, _shift)					\
	GATE_IFRX(_id, _parent, _shift, &ifr2_cg_regs)
#define GATE_IFR2_EXT(_id, _parent, _shift)				\
	GATE_IFRX_EXT(_id, _parent, _shift, &ifr2_cg_regs)
#define GATE_IFR3(_id, _parent, _shift)					\
	GATE_IFRX(_id, _parent, _shift, &ifr3_cg_regs)
#define GATE_IFR3_EXT(_id, _parent, _shift)				\
	GATE_IFRX_EXT(_id, _parent, _shift, &ifr3_cg_regs)
#define GATE_IFR4(_id, _parent, _shift)					\
	GATE_IFRX(_id, _parent, _shift, &ifr4_cg_regs)
#define GATE_IFR4_EXT(_id, _parent, _shift)				\
	GATE_IFRX_EXT(_id, _parent, _shift, &ifr4_cg_regs)
#define GATE_IFR5(_id, _parent, _shift)					\
	GATE_IFRX(_id, _parent, _shift, &ifr5_cg_regs)
#define GATE_IFR5_EXT(_id, _parent, _shift)				\
	GATE_IFRX_EXT(_id, _parent, _shift, &ifr5_cg_regs)

static const struct mtk_gate ifr_clks[] = {
	/* INFRA mode 0 */
	GATE_IFR2(CLK_IFR_PMIC_AP, CLK_TOP_AXI, 1),
	GATE_IFR2(CLK_IFR_ICUSB, CLK_TOP_AXI, 8),
	GATE_IFR2(CLK_IFR_GCE, CLK_TOP_AXI, 9),
	GATE_IFR2(CLK_IFR_THERM, CLK_TOP_AXI, 10),
	GATE_IFR2(CLK_IFR_I2C_AP, CLK_TOP_I2C, 11),
	GATE_IFR2(CLK_IFR_I2C_CCU, CLK_TOP_I2C, 12),
	GATE_IFR2(CLK_IFR_I2C_SSPM, CLK_TOP_I2C, 13),
	GATE_IFR2(CLK_IFR_I2C_RSV, CLK_TOP_I2C, 14),
	GATE_IFR2(CLK_IFR_PWM_HCLK, CLK_TOP_AXI, 15),
	GATE_IFR2(CLK_IFR_PWM1, CLK_TOP_F_FPWM, 16),
	GATE_IFR2(CLK_IFR_PWM2, CLK_TOP_F_FPWM, 17),
	GATE_IFR2(CLK_IFR_PWM3, CLK_TOP_F_FPWM, 18),
	GATE_IFR2(CLK_IFR_PWM4, CLK_TOP_F_FPWM, 19),
	GATE_IFR2(CLK_IFR_PWM5, CLK_TOP_F_FPWM, 20),
	GATE_IFR2(CLK_IFR_PWM, CLK_TOP_F_FPWM, 21),
	GATE_IFR2(CLK_IFR_UART0, CLK_TOP_F_FUART, 22),
	GATE_IFR2(CLK_IFR_UART1, CLK_TOP_F_FUART, 23),
	GATE_IFR2_EXT(CLK_IFR_GCE_26M, CLK_PAD_CLK26M, 27),
	GATE_IFR2(CLK_IFR_CQ_DMA_FPC, CLK_TOP_AXI, 28),
	GATE_IFR2(CLK_IFR_BTIF, CLK_TOP_AXI, 31),
	/* INFRA mode 1 */
	GATE_IFR3(CLK_IFR_SPI0, CLK_TOP_SPI, 1),
	GATE_IFR3(CLK_IFR_MSDC0, CLK_TOP_MSDC50_0_HCLK_SEL, 2),
	GATE_IFR3(CLK_IFR_MSDC1, CLK_TOP_AXI, 4),
	GATE_IFR3(CLK_IFR_TRNG, CLK_TOP_AXI, 9),
	GATE_IFR3_EXT(CLK_IFR_AUXADC, CLK_PAD_CLK26M, 10),
	GATE_IFR3(CLK_IFR_CCIF1_AP, CLK_TOP_AXI, 12),
	GATE_IFR3(CLK_IFR_CCIF1_MD, CLK_TOP_AXI, 13),
	GATE_IFR3_EXT(CLK_IFR_AUXADC_MD, CLK_PAD_CLK26M, 14),
	GATE_IFR3(CLK_IFR_AP_DMA, CLK_TOP_AXI, 18),
	GATE_IFR3(CLK_IFR_DEVICE_APC, CLK_TOP_AXI, 20),
	GATE_IFR3(CLK_IFR_CCIF_AP, CLK_TOP_AXI, 23),
	GATE_IFR3(CLK_IFR_AUDIO, CLK_TOP_AXI, 25),
	GATE_IFR3(CLK_IFR_CCIF_MD, CLK_TOP_AXI, 26),
	/* INFRA mode 2 */
	GATE_IFR4_EXT(CLK_IFR_RG_PWM_FBCLK6, CLK_PAD_CLK26M, 0),
	GATE_IFR4(CLK_IFR_DISP_PWM, CLK_TOP_F_FDISP_PWM, 2),
	GATE_IFR4(CLK_IFR_CLDMA_BCLK, CLK_TOP_AXI, 3),
	GATE_IFR4_EXT(CLK_IFR_AUDIO_26M_BCLK, CLK_PAD_CLK26M, 4),
	GATE_IFR4(CLK_IFR_SPI1, CLK_TOP_SPI, 6),
	GATE_IFR4(CLK_IFR_I2C4, CLK_TOP_I2C, 7),
	GATE_IFR4(CLK_IFR_SPI2, CLK_TOP_SPI, 9),
	GATE_IFR4(CLK_IFR_SPI3, CLK_TOP_SPI, 10),
	GATE_IFR4(CLK_IFR_I2C5, CLK_TOP_I2C, 18),
	GATE_IFR4(CLK_IFR_I2C5_ARBITER, CLK_TOP_I2C, 19),
	GATE_IFR4(CLK_IFR_I2C5_IMM, CLK_TOP_I2C, 20),
	GATE_IFR4(CLK_IFR_I2C1_ARBITER, CLK_TOP_I2C, 21),
	GATE_IFR4(CLK_IFR_I2C1_IMM, CLK_TOP_I2C, 22),
	GATE_IFR4(CLK_IFR_I2C2_ARBITER, CLK_TOP_I2C, 23),
	GATE_IFR4(CLK_IFR_I2C2_IMM, CLK_TOP_I2C, 24),
	GATE_IFR4(CLK_IFR_SPI4, CLK_TOP_SPI, 25),
	GATE_IFR4(CLK_IFR_SPI5, CLK_TOP_SPI, 26),
	GATE_IFR4(CLK_IFR_CQ_DMA, CLK_TOP_AXI, 27),
	GATE_IFR4(CLK_IFR_BIST2FPC, CLK_TOP_F_BIST2FPC, 28),
	GATE_IFR4(CLK_IFR_FAES_FDE, CLK_TOP_AES_FDE, 29),
	/* INFRA mode 3 */
	GATE_IFR5(CLK_IFR_MSDC0_SELF, CLK_TOP_MSDC50_0, 0),
	GATE_IFR5(CLK_IFR_MSDC1_SELF, CLK_TOP_MSDC50_0, 1),
	GATE_IFR5(CLK_IFR_MSDC2_SELF, CLK_TOP_MSDC50_0, 2),
	GATE_IFR5(CLK_IFR_UFS_AXI, CLK_TOP_CSW_FAXI, 5),
	GATE_IFR5(CLK_IFR_I2C6, CLK_TOP_I2C, 6),
	GATE_IFR5(CLK_IFR_AP_MSDC0, CLK_TOP_MSDC50_0, 7),
	GATE_IFR5(CLK_IFR_MD_MSDC0, CLK_TOP_MSDC50_0, 8),
	GATE_IFR5(CLK_IFR_MSDC0_SRC, CLK_TOP_MSDC50_0, 9),
	GATE_IFR5(CLK_IFR_MSDC1_SRC, CLK_TOP_CSW_FMSDC30_1, 10),
	GATE_IFR5_EXT(CLK_IFR_MSDC2_SRC, CLK_PAD_CLK26M, 11),
	GATE_IFR5(CLK_IFR_MCU_PM_BCLK, CLK_TOP_AXI, 17),
	GATE_IFR5(CLK_IFR_CCIF2_AP, CLK_TOP_AXI, 18),
	GATE_IFR5(CLK_IFR_CCIF2_MD, CLK_TOP_AXI, 19),
	GATE_IFR5(CLK_IFR_CCIF3_AP, CLK_TOP_AXI, 20),
	GATE_IFR5(CLK_IFR_CCIF3_MD, CLK_TOP_AXI, 21),
};

static const int ifr_id_offs_map[CLK_IFR_NR_CLK] = {
	[0 ... CLK_IFR_NR_CLK - 1] = -1,
	[CLK_IFR_PMIC_AP] = 0,
	[CLK_IFR_ICUSB] = 1,
	[CLK_IFR_GCE] = 2,
	[CLK_IFR_THERM] = 3,
	[CLK_IFR_I2C_AP] = 4,
	[CLK_IFR_I2C_CCU] = 5,
	[CLK_IFR_I2C_SSPM] = 6,
	[CLK_IFR_I2C_RSV] = 7,
	[CLK_IFR_PWM_HCLK] = 8,
	[CLK_IFR_PWM1] = 9,
	[CLK_IFR_PWM2] = 10,
	[CLK_IFR_PWM3] = 11,
	[CLK_IFR_PWM4] = 12,
	[CLK_IFR_PWM5] = 13,
	[CLK_IFR_PWM] = 14,
	[CLK_IFR_UART0] = 15,
	[CLK_IFR_UART1] = 16,
	[CLK_IFR_GCE_26M] = 17,
	[CLK_IFR_CQ_DMA_FPC] = 18,
	[CLK_IFR_BTIF] = 19,
	[CLK_IFR_SPI0] = 20,
	[CLK_IFR_MSDC0] = 21,
	[CLK_IFR_MSDC1] = 22,
	[CLK_IFR_TRNG] = 23,
	[CLK_IFR_AUXADC] = 24,
	[CLK_IFR_CCIF1_AP] = 25,
	[CLK_IFR_CCIF1_MD] = 26,
	[CLK_IFR_AUXADC_MD] = 27,
	[CLK_IFR_AP_DMA] = 28,
	[CLK_IFR_DEVICE_APC] = 29,
	[CLK_IFR_CCIF_AP] = 30,
	[CLK_IFR_AUDIO] = 31,
	[CLK_IFR_CCIF_MD] = 32,
	[CLK_IFR_RG_PWM_FBCLK6] = 33,
	[CLK_IFR_DISP_PWM] = 34,
	[CLK_IFR_CLDMA_BCLK] = 35,
	[CLK_IFR_AUDIO_26M_BCLK] = 36,
	[CLK_IFR_SPI1] = 37,
	[CLK_IFR_I2C4] = 38,
	[CLK_IFR_SPI2] = 39,
	[CLK_IFR_SPI3] = 40,
	[CLK_IFR_I2C5] = 41,
	[CLK_IFR_I2C5_ARBITER] = 42,
	[CLK_IFR_I2C5_IMM] = 43,
	[CLK_IFR_I2C1_ARBITER] = 44,
	[CLK_IFR_I2C1_IMM] = 45,
	[CLK_IFR_I2C2_ARBITER] = 46,
	[CLK_IFR_I2C2_IMM] = 47,
	[CLK_IFR_SPI4] = 48,
	[CLK_IFR_SPI5] = 49,
	[CLK_IFR_CQ_DMA] = 50,
	[CLK_IFR_BIST2FPC] = 51,
	[CLK_IFR_FAES_FDE] = 52,
	[CLK_IFR_MSDC0_SELF] = 53,
	[CLK_IFR_MSDC1_SELF] = 54,
	[CLK_IFR_MSDC2_SELF] = 55,
	[CLK_IFR_UFS_AXI] = 56,
	[CLK_IFR_I2C6] = 57,
	[CLK_IFR_AP_MSDC0] = 58,
	[CLK_IFR_MD_MSDC0] = 59,
	[CLK_IFR_MSDC0_SRC] = 60,
	[CLK_IFR_MSDC1_SRC] = 61,
	[CLK_IFR_MSDC2_SRC] = 62,
	[CLK_IFR_MCU_PM_BCLK] = 63,
	[CLK_IFR_CCIF2_AP] = 64,
	[CLK_IFR_CCIF2_MD] = 65,
	[CLK_IFR_CCIF3_AP] = 66,
	[CLK_IFR_CCIF3_MD] = 67,
};

static const struct mtk_clk_tree mt6761_infracfg_tree = {
	.ext_clk_rates = ext_clock_rates,
	.num_ext_clks = ARRAY_SIZE(ext_clock_rates),
	.id_offs_map = ifr_id_offs_map,
	.id_offs_map_size = ARRAY_SIZE(ifr_id_offs_map),
	.gates_offs = ifr_id_offs_map[CLK_IFR_PMIC_AP],
	.gates = ifr_clks,
	.num_gates = ARRAY_SIZE(ifr_clks),
};

static const struct mtk_parent mcu_bus_parents[] = {
	EXT_PARENT(CLK_PAD_CLK26M),
	APMIXED_PARENT(CLK_APMIXED_ARMPLL),
	TOP_PARENT(CLK_TOP_ARMPLL_DIV_PLL),
};

static const struct mtk_composite mcu_muxes[] = {
	/* bus_pll_divider_cfg */
	MUX_GATE(CLK_MCU_BUS_SEL, mcu_bus_parents, 0x7C0, 9, 2, -1),
};

static const struct mtk_clk_tree mt6761_mcucfg_tree = {
	.ext_clk_rates = ext_clock_rates,
	.num_ext_clks = ARRAY_SIZE(ext_clock_rates),
	.muxes_offs = CLK_MCU_BUS_SEL,
	.muxes = mcu_muxes,
	.num_muxes = ARRAY_SIZE(mcu_muxes),
};

static int mt6761_clk_probe(struct udevice *dev)
{
	struct mtk_clk_priv *priv = dev_get_priv(dev);
	int ret;

	ret = mtk_clk_probe(dev);
	if (ret)
		return ret;

	if (priv->tree == &mt6761_apmixed_tree) {
		/* MPLL, CCIPLL, MAINPLL set HW mode, TDCLKSQ, CLKSQ1 */
		writel(readl(priv->base + AP_PLL_CON3) & 0xFFFFFFE1,
		       priv->base + AP_PLL_CON3);
		writel(readl(priv->base + PLLON_CON0) & 0x11111111,
		       priv->base + PLLON_CON0);
		writel(readl(priv->base + PLLON_CON1) & 0x11,
		       priv->base + PLLON_CON1);
	} else if (priv->tree == &mt6761_topckgen_tree) {
		/* [4]: no need */
		writel(readl(priv->base + CLK_SCP_CFG_0) | 0x3EF,
		       priv->base + CLK_SCP_CFG_0);
		/* [1,2,3,8]: no need */
		writel(readl(priv->base + CLK_SCP_CFG_1) | 0x1,
		       priv->base + CLK_SCP_CFG_1);
	}

	return 0;
}

static const struct udevice_id mt6761_clk_compat[] = {
	{
		.compatible = "mediatek,mt6761-apmixedsys",
		.data = (ulong)&mt6761_apmixed_tree,
	},
	{
		.compatible = "mediatek,mt6761-topckgen",
		.data = (ulong)&mt6761_topckgen_tree,
	},
	{
		.compatible = "mediatek,mt6761-infracfg",
		.data = (ulong)&mt6761_infracfg_tree,
	},
	{
		.compatible = "mediatek,mt6761-mcucfg",
		.data = (ulong)&mt6761_mcucfg_tree,
	},
	{ }
};

U_BOOT_DRIVER(mt6761_clk) = {
	.name = "mt6761-clk",
	.id = UCLASS_CLK,
	.of_match = mt6761_clk_compat,
	.probe = mt6761_clk_probe,
	.priv_auto = sizeof(struct mtk_clk_priv),
	.ops = &mtk_clk_ops,
	.flags = DM_FLAG_PRE_RELOC,
};