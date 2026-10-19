# 403 第一个控件：单行文本框

NsUI 框架的第一个控件 —— 单行文本框（SingleLineText）。
在 OLED 指定坐标显示一行文本，支持 6x12 / 8x16 / 12x24 三种字体。

## 本节内容

- 控件数据结构 `stSingleLineTxetTdf`：坐标（x, y）、字符串指针、字体大小
- 通过控件指针数组统一管理多个文本控件
- 在 Ui_Task 中创建 UI 任务，驱动 OLED 刷新显示

## 工程结构

```
NsUiExample/
├── Core/            # CubeMX 生成的工程核心（main、中断、HAL 配置）
├── Drivers/         # STM32 HAL 库 + CMSIS
├── Middlewares/     # FreeRTOS 内核源码
├── User/
│   ├── Device/      # OLED 驱动（软件模拟 I2C，分层架构）与字库
│   └── Task/        # UI 任务（本节核心：单行文本框控件）
├── MDK-ARM/         # Keil 工程文件
└── notes/           # 学习笔记
```

## 编译与下载

1. Keil MDK 打开 `MDK-ARM/NsUiExample.uvprojx`
2. 编译（F7）→ 下载（F8）
