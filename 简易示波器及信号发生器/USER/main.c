#include "stm32f4xx.h"
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "app_config.h"
#include "tftlcd.h"
#include "app_main.h"

/*==============================================================================
 * main.c
 *-----------------------------------------------------------------------------
 * Õý³£: APP_Init() + APP_Loop()
 *============================================================================*/
int main(void)
{
    APP_Init();

    while (1)
    {
        APP_Loop();
    }
}
