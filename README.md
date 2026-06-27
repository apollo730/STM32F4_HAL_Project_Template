# STM32F429\_HAL\_Project\_Template 工程模板

## 一、项目简介
本项目为 **STM32F429IGT6 HAL 库通用开发模板**，基于 STM32CubeMX 生成，适配 VSCode+ 插件STM32CUBEIDE for VScode + OpenOCD 全套开源开发环境，
需要：
    需要安装的vscode插件：插件STM32CUBEIDE for VScode会自动安装编译工具链,安装cortex-debug插件用于调试。
	下载openocd，使用cortex-debug插件中的设置，设置openocd.exe路径。
    此工程模板先从cubeMX打开.ioc文件，配置生成代码后，再进行调试开发。
    UserProfile文件夹中的STM32.code-profile文件可以直接导入，自动下载安装所需插件
## 二、硬件平台
- **主控芯片**：STM32F429IGT6
- **开发板**：野火 STM32F429IGT6\_V2 开发板
- **开发框架**：STM32 HAL 库
## 三、开发环境
- 代码生成工具：STM32CubeMX
- 编译工具：CMake \+ Ninja
- 调试烧录：OpenOCD \+ cortex-debug
- 编辑器：VSCode（含完整工程配置）
- 代码格式化：clang\-format
- 注释插件：Doxygen
## 四、工程整体结构说明
### 1\. 工程目录核心文件
- **STM32F429\BSP**： 板级支持文件夹,适配野火 F429 开发板的板级驱动包，包含板载 LED、按键、串口、屏幕等底层驱动，可直接复用。
- **\_HAL\_Project\_Template\.ioc**：CubeMX 工程配置文件，保存所有外设、时钟、引脚配置，可二次修改重生成代码。
- **STM32F429\.svd**：外设寄存器描述文件，调试时可直接在 VSCode 查看 STM32 所有外设寄存器数值。
- **\.clang\-format**：全局代码格式化规则，统一代码风格，一键格式化整个工程代码。
- **STM32F429\.vscode**： 工程配置文件夹:
    - **settings\.json**
        - 配置armToolchainPath路径、openocd.exe路径
        - 配置Doxygen注释风格
    - **tasks\.json**
        - 内置一键任务：CMake 配置、工程构建、代码编译、程序烧录、工程清理（clean）
        - 可安装Task Buttons用于快捷任务按钮，配置以在settings\.json中设置
    - **launch\.json**
        - OpenOCD 调试配置
        - 支持断点调试、寄存器查看、变量监控
### 2\. 关键资源文件夹
- **STM32.code-profile**:vscode开发环境的配置文件
## 五、已配置的核心功能
- ✅ 完整 VSCode 开发环境预配置
- ✅ CMake \+ Ninja 编译体系
- ✅ OpenOCD 一键下载 \+ 在线调试
- ✅ SVD 外设寄存器可视化调试
- ✅ 统一 clang\-format 代码格式化规范
- ✅ 野火 F429 全套板级驱动支持（待更新）
- ✅ 内置 VSCode 快捷任务按钮：编译、构建、烧录、清理
## 六、使用说明
1. 使用 VSCode 打开工程根目录（必须打开文件夹，不能单开文件）
2. 等待 STM32CUBEIDE for VScode 自动加载配置
3. 点击 VSCode 快捷任务按钮：Build 编译工程
4. 连接下载器，一键 Flash 下载程序
5. 支持Debug 在线断点调试
## 七、更新记录
2026\.05\.30 — 初始化完整 STM32F429 HAL 工程模板，配置全套编译调试环境、BSP 驱动、代码格式化、寄存器调试功能。
2026\.05\.31 — 更新setting.json文件，增加Doxygen注释风格，增加STM32.code-profile文件，可直接导入到vscode中。
2026\.06\.27 — 更新setting.json文件，在launch.json、tasks.json 文件中使用${command:cmake.launchTargetFilename}变量，确保调试中不会文件找不到的错误。
> 
