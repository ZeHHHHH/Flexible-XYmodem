
# Flexible-XYmodem 应用说明

用户通过阅读本文, 会对 Flexible-XYmodem 组件有一定了解, 并可以快速地部署至目标设备上运行.

## 介绍

Flexible-XYmodem 是一个轻量、灵活的 Xmodem/Ymodem 协议库, 采用 OOPC 构建, 实现了标准的 Xmodem/Ymodem 收发, 源码遵循 ANSI C 标准.

- **本组件特点**: 
  - 灵活的内存分配选择, 可选静态或动态内存分配, 同时支持多个实例.
  - 非阻塞实现, 有效数据处理对用户开放透明.
  - 仅实现标准的 X/Ymodem 收发, 允许用户拓展, 可移植性和效率更高.
  - 轻量的 RAM/ROM 资源占用, API 简单易上手, 提供裸机应用下的参考示例.

## 目录一览

- **./xymodem**
  - xymodem.c
  - xymodem.h
  - xymodem_example.h

- **./xymodem/port**
  - Synwit : SWM 全系列芯片移植示例

## 编译构建

> 将 **./xymodem** 路径添加至编译器头文件索引下;

> 将 **xymodem.c / xymodem_example.c** 加入编译;

> 在 **./xymodem/port** 目录下选择对应厂商的 **xymodem_port_xxx.c** 加入编译, 如无对应厂商的芯片支持, 可参考 **Synwit** 目录下的示例, 在用户当前平台上重新实现对应接口;

> 调整 **xymodem_port_xxx.c** 文件中的 **get_ticks()** 函数实现, 为其提供一个在用户平台上的心跳时基.

## 运行测试

在 **xymodem_example.c** 配置 **EXAMPLE_CONFIG** 枚举宏以选择运行相应的示例, 并在用户任务中调用 **xymodem_example()** 函数执行, 编译下载至目标设备进行测试, 如无异常, 将输出打印 “X / Y modem example test!”.
> 支持设备双向自测(需要有两组以上的串口)
> 支持设备与串口终端双向测试

## 注意事项

- 对 Stack 占用较大, 请保证栈大小至少为 2KB 以上.
- 在使用串口终端工具如：**SecureCRT、XShell、sscom** 时, 关闭或禁用 **RTS/CTR** 硬件流控选项.
- 个别串口终端工具实现的 Ymodem 协议与标准协议有所差异, 目前可能需要调整 Ymodem 文件信息包与传输流程以适配(通常是首包和尾包的处理有所不同), 将来应有额外的拓展处理流程.

- ***拉取链接：***
> <https://github.com/ZeHHHHH/Flexible-XYmodem.git>

## 维护信息

-  邮箱：<lzhoran@163.com>
