#include "bsp_sdram.h"


HAL_StatusTypeDef SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram)
{
    FMC_SDRAM_CommandTypeDef cmd;

    // 步骤1: 时钟使能
    cmd.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
    cmd.AutoRefreshNumber = 1;
    cmd.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(hsdram, &cmd, 0xFFFF);
    HAL_Delay(1); // 等待时钟稳定

    // 步骤2: 预充电所有Bank
    cmd.CommandMode = FMC_SDRAM_CMD_PALL;
    HAL_SDRAM_SendCommand(hsdram, &cmd, 0xFFFF);

    // 步骤3: 自动刷新 6次
    cmd.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
    cmd.AutoRefreshNumber = 2; // 修改点 1: 4 改为 8
    cmd.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(hsdram, &cmd, 0xFFFF);

    // 步骤4: 设置模式寄存器(W9825G6KH: CAS=3, Burst=4, WB=1)
    // 注意: WB位只影响"突发读后写入"的行为,WB=1表示单次写入
    // FMC的独立写入操作不受此限制,仍可进行32位传输
    cmd.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    cmd.ModeRegisterDefinition = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_8 |
                                  SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |
                                  SDRAM_MODEREG_CAS_LATENCY_3           |
                                  SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                                  SDRAM_MODEREG_WRITEBURST_MODE_SINGLE; // WB=1 (single write after burst read)
    HAL_SDRAM_SendCommand(hsdram, &cmd, 0xFFFF);

    /// 步骤5: 自动刷新 2次 
    cmd.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
    cmd.AutoRefreshNumber = 2; 
    cmd.ModeRegisterDefinition = 0;
    HAL_SDRAM_SendCommand(hsdram, &cmd, 0xFFFF);

    // 步骤6: 设置自动刷新速率
    HAL_SDRAM_SetAutoRefreshNumber(hsdram, SDRAM_REFRESH_COUNT);
    
    // 验证初始化是否成功
    uint32_t testAddr = SDRAM_BASE_ADDR;
    uint32_t testValue = 0x12345678;
    HAL_SDRAM_Write_32b(hsdram, &testAddr, &testValue, 1);
    uint32_t readValue = 0;
    HAL_SDRAM_Read_32b(hsdram, &testAddr, &readValue, 1);
    
    if (readValue != testValue) {
        return HAL_ERROR;
    }
    return HAL_OK;
}


