/*
 * Copyright (c) 2018 PHYTEC Messtechnik GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/* ssd1673_regs.h - Registers definition for SSD1673 controller */

#ifndef __SSD1673_REGS_H__
#define __SSD1673_REGS_H__

#define SSD1673_CMD_GDO_CTRL			0x01
#define SSD1673_CMD_GDV_CTRL			0x03
#define SSD1673_CMD_SDV_CTRL			0x04
#define SSD1673_CMD_GSCAN_START			0x0f
#define SSD1673_CMD_SLEEP_MODE			0x10
#define SSD1673_CMD_ENTRY_MODE			0x11
#define SSD1673_CMD_SW_RESET			0x12
#define SSD1673_CMD_TSENS_CTRL			0x1a
#define SSD1673_CMD_MASTER_ACTIVATION		0x20
#define SSD1673_CMD_UPDATE_CTRL1		0x21
#define SSD1673_CMD_UPDATE_CTRL2		0x22
#define SSD1673_CMD_WRITE_RAM			0x24
#define SSD1673_CMD_VCOM_SENSE			0x28
#define SSD1673_CMD_VCOM_SENSE_DURATON		0x29
#define SSD1673_CMD_PRGM_VCOM_OTP		0x2a
#define SSD1673_CMD_VCOM_VOLTAGE		0x2c
#define SSD1673_CMD_PRGM_WS_OTP			0x30
#define SSD1673_CMD_UPDATE_LUT			0x32
#define SSD1673_CMD_PRGM_OTP_SELECTION		0x36
#define SSD1673_CMD_OTP_SELECTION_CTRL		0x37
#define SSD1673_CMD_DUMMY_LINE			0x3a
#define SSD1673_CMD_GATE_LINE_WIDTH		0x3b
#define SSD1673_CMD_BWF_CTRL			0x3c
#define SSD1673_CMD_RAM_XPOS_CTRL		0x44
#define SSD1673_CMD_RAM_YPOS_CTRL		0x45
#define SSD1673_CMD_RAM_XPOS_CNTR		0x4e
#define SSD1673_CMD_RAM_YPOS_CNTR		0x4f

/* Data entry sequence modes */
#define SSD1673_DATA_ENTRY_MASK			0x07
#define SSD1673_DATA_ENTRY_XDYDX		0x00
#define SSD1673_DATA_ENTRY_XIYDX		0x01
#define SSD1673_DATA_ENTRY_XDYIX		0x02
#define SSD1673_DATA_ENTRY_XIYIX		0x03
#define SSD1673_DATA_ENTRY_XDYDY		0x04
#define SSD1673_DATA_ENTRY_XIYDY		0x05
#define SSD1673_DATA_ENTRY_XDYIY		0x06
#define SSD1673_DATA_ENTRY_XIYIY		0x07

/* Options for display update */
#define SSD1673_CTRL1_INITIAL_UPDATE_LL		0x00
#define SSD1673_CTRL1_INITIAL_UPDATE_LH		0x01
#define SSD1673_CTRL1_INITIAL_UPDATE_HL		0x02
#define SSD1673_CTRL1_INITIAL_UPDATE_HH		0x03

/* Options for display update sequence */
#define SSD1673_CTRL2_ENABLE_CLK		0x80
#define SSD1673_CTRL2_ENABLE_ANALOG		0x40
#define SSD1673_CTRL2_TO_INITIAL		0x08
#define SSD1673_CTRL2_TO_PATTERN		0x04
#define SSD1673_CTRL2_DISABLE_ANALOG		0x02
#define SSD1673_CTRL2_DISABLE_CLK		0x01

#define SSD1673_SLEEP_MODE_DSM			0x01
#define SSD1673_SLEEP_MODE_PON			0x00

/* Default values */
#define SSD1673_VAL_GDV_CTRL_A			16
#define SSD1673_VAL_GDV_CTRL_B			10
#define SSD1673_VAL_SDV_CTRL			0x19
#define SSD1673_VAL_VCOM_VOLTAGE		0xa8
#define SSD1673_VAL_DUMMY_LINE			0x1a
#define SSD1673_VAL_GATE_LWIDTH			0x08

/** Maximum resolution in the X direction */
#define SSD1673_RAM_XRES			152
/** Maximum resolution in the Y direction */
#define SSD1673_RAM_YRES			250

/* time constants in ms */
#define SSD1673_RESET_DELAY			1
#define SSD1673_BUSY_DELAY			1

/** Size of each RAM in octets */
#define SSD1673_RAM_SIZE	(SSD1673_RAM_XRES * SSD1673_RAM_YRES / 8)

#endif /* __SSD1673_REGS_H__ */
