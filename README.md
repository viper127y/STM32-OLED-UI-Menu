# STM32-OLED-UI-Menu

基于 STM32 + FreeRTOS + OLED 的 UI 菜单学习课程仓库（Ns单片机实验室）。

从零实现一套可扩展的 OLED UI 框架，每节课对应一个独立文件夹，工程可直接用 Keil MDK 打开编译。

## 课程目录

| 编号 | 课题 | 工程 |
| ---- | ---- | ---- |
| 403 | 第一个控件：单行文本框 | [403-第一个控件-单行文本框](./403-第一个控件-单行文本框) |

## 硬件与开发环境

- MCU：STM32F103C8T6
- 显示：OLED（软件模拟 I2C 分层驱动）
- RTOS：FreeRTOS（CMSIS-RTOS V2 封装）
- 工程：STM32CubeMX 生成 + Keil MDK-ARM

## 使用方式

1. 用 Keil 打开对应课程文件夹下的 `MDK-ARM/NsUiExample.uvprojx`
2. 编译（F7）→ 下载（F8）即可运行
