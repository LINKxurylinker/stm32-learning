#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "adc.h"
#include "cmd.h"
#include "ui.h"
#include <stdio.h>
	        
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    delay_init(168);

    uart2_init(460800);

    ADC1_Init();
    CMD_Init();
    UI_Init();

    while (1)
    {
        CMD_Task();
        UI_Task();
    }
}
