/*
 * (C) Copyright 2013-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */
/*
**********************************************************************************************************************
*
*						           the Embedded Secure Bootloader System
*
*
*						       Copyright(C), 2006-2014, Allwinnertech Co., Ltd.
*                                           All Rights Reserved
*
* File    :
*
* By      :
*
* Version : V2.00
*
* Date	  :
*
* Descript:
**********************************************************************************************************************
*/
#include "common.h"
#include "asm/io.h"
#include "asm/arch/ccmu.h"
#include "asm/arch/timer.h"
#include "asm/arch/archdef.h"


void enable_pll_lock_bit(__u32 lock_bit)
{
	__u32 reg_val;
	reg_val = readl(CCMU_PLL_LOCK_CTRL_REG);
	reg_val |= lock_bit;
	writel(reg_val, CCMU_PLL_LOCK_CTRL_REG);

}

void disable_pll_lock_bit(__u32 lock_bit)
{
	__u32 reg_val;
	reg_val = readl(CCMU_PLL_LOCK_CTRL_REG);
	reg_val &= (~lock_bit);
	writel(reg_val, CCMU_PLL_LOCK_CTRL_REG);
}

void set_pll_cpux_axi(void)
{
	__u32 reg_val;
	//select CPUX  clock src: OSC24M,AXI divide ratio is 3, system apb clk ratio is 4
	//cpu/axi /sys apb  clock ratio
	writel((1<<16) | (2<<0), CCMU_CPUX_AXI_CFG_REG);
	__usdelay(20);

	//set PLL_CPUX, the  default  clk is 1008M  ,PLL_OUTPUT= 24M*N*K/( M*P)
	disable_pll_lock_bit(LOCK_EN_PLL_CPUX);
	writel((0x1000), CCMU_PLL_CPUX_CTRL_REG);
	enable_pll_lock_bit(LOCK_EN_PLL_CPUX);
	writel((1<<31) | readl(CCMU_PLL_CPUX_CTRL_REG), CCMU_PLL_CPUX_CTRL_REG);
	//wait PLL_CPUX stable
#ifndef FPGA_PLATFORM
	while(!(readl(CCMU_PLL_CPUX_CTRL_REG) & (0x1<<28)));
	__usdelay(20);
#endif
	disable_pll_lock_bit(LOCK_EN_PLL_CPUX);

	//set and change cpu clk src to PLL_CPUX,  PLL_CPUX:AXI0 = 408M:136M
	reg_val = readl(CCMU_CPUX_AXI_CFG_REG);
	reg_val &=  ~(3 << 16);
	reg_val |=  (2 << 16);
	writel(reg_val, CCMU_CPUX_AXI_CFG_REG);
	__usdelay(1000);
}


void set_pll_periph0_ahb_apb(void)
{
	//change ahb src before set pll6
	writel((0x01 << 12) | (readl(CCMU_AHB1_APB1_CFG_REG)&(~(0x3<<12))), CCMU_AHB1_APB1_CFG_REG);

	//enable PLL6:  600M(1X)  1200M(2x)
	disable_pll_lock_bit(LOCK_EN_PLL_PERIPH0);
	writel( 0x41811, CCMU_PLL_PERIPH0_CTRL_REG);
	enable_pll_lock_bit(LOCK_EN_PLL_PERIPH0);
	writel( (1U << 31)|readl(CCMU_PLL_PERIPH0_CTRL_REG), CCMU_PLL_PERIPH0_CTRL_REG);
#ifndef FPGA_PLATFORM
	while(!(readl(CCMU_PLL_PERIPH0_CTRL_REG) & (0x1<<28)));
	__usdelay(20);
#endif
	disable_pll_lock_bit(LOCK_EN_PLL_PERIPH0);

	//set AHB1/APB1 clock  divide ratio
	//ahb1 clock src is PLL6,                           (0x03<< 12)
	//apb1 clk src is ahb1 clk src, divide  ratio is 4  (2<<8)
	//ahb1 pre divide  ratio is 2:    0:1  , 1:2,  2:3,   3:4 (2<<6)
	//ahb1  divide  ratio is 1:    0:1  , 1:2,  2:4,   3:8 (1<<4)
	//PLL6:AHB1:APB1 = 600M:100M:50M , sram C limited(100M).
	writel((1<<8) | (2<<6) | (1<<4), CCMU_AHB1_APB1_CFG_REG);
	writel((0x03 << 12)|readl(CCMU_AHB1_APB1_CFG_REG), CCMU_AHB1_APB1_CFG_REG);
}
void set_pll_dma(void)
{
	//----DMA function--------
	//dma reset
	writel(readl(CCMU_BUS_SOFT_RST_REG0)  | (1 << 6), CCMU_BUS_SOFT_RST_REG0);
	__usdelay(20);
	//gating clock for dma pass
	writel(readl(CCMU_BUS_CLK_GATING_REG0) | (1 << 6), CCMU_BUS_CLK_GATING_REG0);
}

void set_pll_mbus(void)
{
	//reset mbus domain
	writel(0x80000000, CCMU_MBUS_RST_REG);
	//open MBUS,clk src is pll6(2x) , pll6/(m+1) = 400M
    writel(0x00000002, CCMU_MBUS_CLK_REG);
	__usdelay(1);//设置MBUS的分频因子
	writel(0x01000002, CCMU_MBUS_CLK_REG);
	__usdelay(1);//选择MBUS的源头
	writel(0x81000002, CCMU_MBUS_CLK_REG);
	__usdelay(1);//开MBUS时钟
}

void set_pll( void )
{
	//use new mode
	//printf("set pll start\n");
	enable_pll_lock_bit(LOCK_EN_NEW_MODE);

	set_pll_cpux_axi();
	set_pll_periph0_ahb_apb();
	set_pll_dma();
	set_pll_mbus();

	disable_pll_lock_bit(LOCK_EN_NEW_MODE);
	//printf("set pll end\n");
	return ;
}


void reset_pll( void )
{
	writel(0x10000, CCMU_CPUX_AXI_CFG_REG);
	writel(0x01010, CCMU_AHB1_APB1_CFG_REG);
	writel(0x01000000, CCMU_AHB2_CFG_GREG);

	__usdelay(10);
	writel(0x00001000, CCMU_PLL_CPUX_CTRL_REG);
	writel(0x00041811, CCMU_PLL_PERIPH0_CTRL_REG);

	return ;
}

void set_gpio_gate(void)
{
	uint reg_val;

	reg_val = readl(CCMU_BUS_CLK_GATING_REG2);
	reg_val |= (1<<5);
	writel(reg_val, CCMU_BUS_CLK_GATING_REG2);
}

int sunxi_key_clock_open(void)
{
	uint reg_val = 0,i=0;

	//reset
	reg_val = readl(CCMU_BUS_SOFT_RST_REG2);
	reg_val &= ~(1<<9);
	writel(reg_val, CCMU_BUS_SOFT_RST_REG2);
	for( i = 0; i < 100; i++ );
	reg_val |=  (1<<9);
	writel(reg_val, CCMU_BUS_SOFT_RST_REG2);

	//enable KEYADC gating
	reg_val = readl(CCMU_BUS_CLK_GATING_REG2);
	reg_val |= (1<<9);
	writel(reg_val, CCMU_BUS_CLK_GATING_REG2);

	return 0;
}
