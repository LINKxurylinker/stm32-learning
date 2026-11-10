#include "tftlcd.h"
#include "stdlib.h"
#include "font.h" 
#include "usart.h"	 
#include "delay.h"	   


//LCD的画笔颜色和背景色	   
u16 FRONT_COLOR=BLACK;	//画笔颜色
u16 BACK_COLOR=WHITE;  //背景色 

_tftlcd_data tftlcd_data; 
  

//写寄存器函数
//cmd:寄存器值
void LCD_WriteCmd(u16 cmd)
{
#ifdef TFTLCD_HX8357D	
	TFTLCD->LCD_CMD=cmd;//写入要写的寄存器序号
#endif
	
#ifdef TFTLCD_HX8357DN	
	TFTLCD->LCD_CMD=cmd;
#endif

#ifdef TFTLCD_R61509V	
	TFTLCD->LCD_CMD=cmd;//写入要写的寄存器序号
#endif
	
#ifdef TFTLCD_R61509VN	
	TFTLCD->LCD_CMD=(cmd>>8)<<1;
	TFTLCD->LCD_CMD=(cmd&0xff)<<1;
#endif

#ifdef TFTLCD_R61509V3	
	TFTLCD->LCD_CMD=cmd;//写入要写的寄存器序号
#endif
	
#ifdef TFTLCD_HX8352C
	TFTLCD->LCD_CMD=cmd<<8;
#endif

#ifdef TFTLCD_ILI9341
	TFTLCD->LCD_CMD=cmd;
#endif
	
#ifdef TFTLCD_ILI9327
	TFTLCD->LCD_CMD=cmd;
#endif

#ifdef TFTLCD_ILI9325
	TFTLCD->LCD_CMD=cmd;
#endif

#ifdef TFTLCD_ILI9486
	TFTLCD->LCD_CMD=cmd;
#endif

#ifdef TFTLCD_ST7793		
	TFTLCD->LCD_CMD=cmd>>8;
	TFTLCD->LCD_CMD=cmd&0xff;
#endif

#ifdef TFTLCD_SSD1963		
	TFTLCD->LCD_CMD=cmd;
#endif
	
#ifdef TFTLCD_NT35510		
	TFTLCD->LCD_CMD=cmd;	
#endif

#ifdef TFTLCD_ILI9481
	TFTLCD->LCD_CMD=cmd;
#endif

#ifdef TFTLCD_R61509VE
	TFTLCD->LCD_CMD=cmd;
#endif

#ifdef TFTLCD_SSD1963N		
	TFTLCD->LCD_CMD=cmd;
#endif

#ifdef TFTLCD_ILI9488
	TFTLCD->LCD_CMD=cmd;
#endif

#ifdef TFTLCD_ILI9806
	TFTLCD->LCD_CMD=cmd;
#endif
	
}

//写数据
//data:要写入的值
void LCD_WriteData(u16 data)
{
#ifdef TFTLCD_HX8357D	
	TFTLCD->LCD_DATA=data;//写入要写的寄存器序号
#endif

#ifdef TFTLCD_HX8357DN	
	TFTLCD->LCD_DATA=data;
#endif
	
#ifdef TFTLCD_R61509V	
	TFTLCD->LCD_DATA=data;//写入要写的寄存器序号
#endif
	
#ifdef TFTLCD_R61509VN	
	TFTLCD->LCD_DATA=(data>>8)<<1;
	TFTLCD->LCD_DATA=(data&0xff)<<1;	
#endif

#ifdef TFTLCD_R61509V3	
	TFTLCD->LCD_DATA=data;//写入要写的寄存器序号
#endif
	
#ifdef TFTLCD_HX8352C
	TFTLCD->LCD_DATA=data<<8;
#endif

#ifdef TFTLCD_ILI9341
	TFTLCD->LCD_DATA=data;
#endif

#ifdef TFTLCD_ILI9327
	TFTLCD->LCD_DATA=data;
#endif

#ifdef TFTLCD_ILI9325
	TFTLCD->LCD_DATA=data;
#endif
	
#ifdef TFTLCD_ILI9486
	TFTLCD->LCD_DATA=data;
#endif
	
#ifdef TFTLCD_ST7793	
	TFTLCD->LCD_DATA=data>>8;	
	TFTLCD->LCD_DATA=data&0xff;	
#endif

#ifdef TFTLCD_SSD1963
	TFTLCD->LCD_DATA=data;
#endif
	
#ifdef TFTLCD_NT35510		
	TFTLCD->LCD_DATA=data;	
#endif

#ifdef TFTLCD_ILI9481
	TFTLCD->LCD_DATA=data;
#endif

#ifdef TFTLCD_R61509VE
	TFTLCD->LCD_DATA=data;
#endif

#ifdef TFTLCD_SSD1963N
	TFTLCD->LCD_DATA=data;
#endif

#ifdef TFTLCD_ILI9488
	TFTLCD->LCD_DATA=data;
#endif

#ifdef TFTLCD_ILI9806
	TFTLCD->LCD_DATA=data;
#endif
}

void LCD_WriteCmdData(u16 cmd,u16 data)
{
	LCD_WriteCmd(cmd);
	LCD_WriteData(data);
}


u32 LCD_RGBColor_Change(u16 color)
{
	u8 r,g,b=0;
	
	r=(color>>11)&0x1f;
	g=(color>>5)&0x3f;
	b=color&0x1f;
	
	return ((r<<13)|(g<<6)|(b<<1));
}
void LCD_WriteData_Color(u16 color)
{
#ifdef TFTLCD_HX8357D
	TFTLCD->LCD_DATA=color;
#endif

#ifdef TFTLCD_HX8357DN
	TFTLCD->LCD_DATA=color>>8;
	TFTLCD->LCD_DATA=color&0xff;
#endif

#ifdef TFTLCD_R61509V
	TFTLCD->LCD_DATA=color;
#endif
	
#ifdef TFTLCD_R61509VN
	u32 recolor=0;
	recolor=LCD_RGBColor_Change(color);
	TFTLCD->LCD_DATA=(recolor>>9);
	TFTLCD->LCD_DATA=recolor;
#endif
	
#ifdef TFTLCD_R61509V3
	TFTLCD->LCD_DATA=color;
#endif

#ifdef TFTLCD_HX8352C
	TFTLCD->LCD_DATA=color&0xff00;
	TFTLCD->LCD_DATA=color<<8;
#endif	

#ifdef TFTLCD_ILI9341
	TFTLCD->LCD_DATA=color;
#endif
	
#ifdef TFTLCD_ILI9327
	TFTLCD->LCD_DATA=color>>8;
	TFTLCD->LCD_DATA=color&0xff;
#endif

#ifdef TFTLCD_ILI9325
	TFTLCD->LCD_DATA=color;
#endif

#ifdef TFTLCD_ILI9486
	TFTLCD->LCD_DATA=color;
#endif

#ifdef TFTLCD_ST7793
	TFTLCD->LCD_DATA=color>>8;
	TFTLCD->LCD_DATA=color&0xff;
#endif

#ifdef TFTLCD_SSD1963
	TFTLCD->LCD_DATA=color;
#endif
	
#ifdef TFTLCD_NT35510
	TFTLCD->LCD_DATA=color;	
#endif

#ifdef TFTLCD_ILI9481
	TFTLCD->LCD_DATA=color;
#endif

#ifdef TFTLCD_R61509VE
	TFTLCD->LCD_DATA=color;
#endif

#ifdef TFTLCD_SSD1963N
	TFTLCD->LCD_DATA=color;
#endif

#ifdef TFTLCD_ILI9488
	TFTLCD->LCD_DATA=color;
#endif

#ifdef TFTLCD_ILI9806
	TFTLCD->LCD_DATA=color;
#endif
}

//读数据
//返回值:读到的值
u16 LCD_ReadData(void)
{
#ifdef TFTLCD_HX8357D
	return TFTLCD->LCD_DATA;
#endif
	
#ifdef TFTLCD_HX8357DN
//	u16 ram1,ram2;
//	ram1=TFTLCD->LCD_DATA;
//	printf("ram1=%x   ",ram1);
//	ram2=TFTLCD->LCD_DATA;
//	printf("ram2=%x   \r\n",ram2);
//	ram2=ram2<<8|ram1;
//	return ram2;
	return TFTLCD->LCD_DATA;
//	return ((TFTLCD->LCD_DATA<<8)|(TFTLCD->LCD_DATA));
#endif

#ifdef TFTLCD_R61509V
	return TFTLCD->LCD_DATA;
#endif
	
#ifdef TFTLCD_R61509VN
//	return TFTLCD->LCD_DATA;
	u16 ram;		
	ram=(TFTLCD->LCD_DATA)>>1;
	ram=ram<<8;
	ram|=(TFTLCD->LCD_DATA)>>1;	
	return ram;	
#endif

#ifdef TFTLCD_R61509V3
	return TFTLCD->LCD_DATA;
#endif
	
#ifdef TFTLCD_HX8352C
	return ((TFTLCD->LCD_DATA<<8)|(TFTLCD->LCD_DATA));
#endif

#ifdef TFTLCD_ILI9341
	return TFTLCD->LCD_DATA;
#endif

#ifdef TFTLCD_ILI9327		
	u16 ram;		
	ram=TFTLCD->LCD_DATA;
	ram=ram<<8;
	ram|=TFTLCD->LCD_DATA;	
	return ram;		
#endif

#ifdef TFTLCD_ILI9325		
	return TFTLCD->LCD_DATA;	
#endif

#ifdef TFTLCD_ILI9486
	return TFTLCD->LCD_DATA;
#endif

#ifdef TFTLCD_ST7793		
	u16 ram;		
	ram=TFTLCD->LCD_DATA;
	ram=ram<<8;
	ram|=TFTLCD->LCD_DATA;	
	return ram;		
#endif

#ifdef TFTLCD_SSD1963
	return TFTLCD->LCD_DATA;
#endif
	
#ifdef TFTLCD_NT35510		
	return TFTLCD->LCD_DATA;
#endif

#ifdef TFTLCD_ILI9481
	return TFTLCD->LCD_DATA;
#endif

#ifdef TFTLCD_R61509VE
	return TFTLCD->LCD_DATA;
#endif

#ifdef TFTLCD_SSD1963N
	return TFTLCD->LCD_DATA;
#endif

#ifdef TFTLCD_ILI9488		
	u16 ram;		
	ram=TFTLCD->LCD_DATA;
	return ram;		
#endif

#ifdef TFTLCD_ILI9806
	return TFTLCD->LCD_DATA;
#endif
}


//设置LCD显示方向
//dir:0,竖屏；1,横屏
void LCD_Display_Dir(u8 dir)
{
	tftlcd_data.dir=dir;         //横屏/竖屏
	if(dir==0)  //默认竖屏方向
	{		
#ifdef TFTLCD_HX8357D		
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x4c);
		tftlcd_data.height=480;
		tftlcd_data.width=320;
#endif

#ifdef TFTLCD_HX8357DN		
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x4c);
		tftlcd_data.height=480;
		tftlcd_data.width=320;
#endif

#ifdef TFTLCD_R61509V		
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0100);
		LCD_WriteCmd(0x0003);
		LCD_WriteData(0x1030);
		tftlcd_data.height=400;
		tftlcd_data.width=240;
#endif

#ifdef TFTLCD_R61509VN		
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0100);
		LCD_WriteCmd(0x0003);
		LCD_WriteData(0x1030);
		tftlcd_data.height=400;
		tftlcd_data.width=240;
#endif
		
#ifdef TFTLCD_R61509V3		
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0100);
		LCD_WriteCmd(0x0003);
		LCD_WriteData(0x1030);
		tftlcd_data.height=400;
		tftlcd_data.width=240;
#endif

#ifdef TFTLCD_HX8352C
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x03);  //03:U-D,L-R; 06:D-U,R-L;
		tftlcd_data.height=400;
		tftlcd_data.width=240;			
#endif

#ifdef TFTLCD_ILI9341
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x08);  
		tftlcd_data.height=320;
		tftlcd_data.width=240;			
#endif

#ifdef TFTLCD_ILI9327
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x48);  
		tftlcd_data.height=400;
		tftlcd_data.width=240;			
#endif

#ifdef TFTLCD_ILI9325
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0100);
		LCD_WriteCmd(0x0003);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x1030);  
		tftlcd_data.height=320;
		tftlcd_data.width=240;			
#endif

#ifdef TFTLCD_ILI9486
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x00);  
		tftlcd_data.height=480;
		tftlcd_data.width=320;			
#endif

#ifdef TFTLCD_ST7793
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0100);
		LCD_WriteCmd(0x0003);
		LCD_WriteData(0x1030);
		tftlcd_data.height=400;
		tftlcd_data.width=240;			
#endif

#ifdef TFTLCD_SSD1963
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x21);  
		tftlcd_data.height=800;
		tftlcd_data.width=480;			
#endif
		
#ifdef TFTLCD_NT35510	
		LCD_WriteCmd(0x3600);   
		LCD_WriteData(0x0000);
		tftlcd_data.height=800;
		tftlcd_data.width=480;
#endif	

#ifdef TFTLCD_ILI9481
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x00);  
		tftlcd_data.height=480;
		tftlcd_data.width=320;			
#endif

#ifdef TFTLCD_R61509VE		
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0100);
		LCD_WriteCmd(0x0003);
		LCD_WriteData(0x1030);
		tftlcd_data.height=400;
		tftlcd_data.width=240;
#endif

#ifdef TFTLCD_SSD1963N
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x22);  
		tftlcd_data.height=480;
		tftlcd_data.width=272;			
#endif

#ifdef TFTLCD_ILI9488		
		LCD_WriteCmdData(0x36,0x08);
		tftlcd_data.height=480;
		tftlcd_data.width=320;
#endif

#ifdef TFTLCD_ILI9806		
		LCD_WriteCmdData(0x36,0x00);
		tftlcd_data.height=800;
		tftlcd_data.width=480;
#endif
	}
	else
	{	
#ifdef TFTLCD_HX8357D
		LCD_WriteCmd(0x36);   
		LCD_WriteData(0x2c);
		tftlcd_data.height=320;
		tftlcd_data.width=480;
#endif
		
#ifdef TFTLCD_HX8357DN
		LCD_WriteCmd(0x36);   
		LCD_WriteData(0x2c);
		tftlcd_data.height=320;
		tftlcd_data.width=480;
#endif

#ifdef TFTLCD_R61509V		
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0000);
		LCD_WriteCmd(0x0003);
		LCD_WriteData(0x1038);
		tftlcd_data.height=240;
		tftlcd_data.width=400;
#endif

#ifdef TFTLCD_R61509VN		
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0000);
		LCD_WriteCmd(0x0003);
		LCD_WriteData(0x1038);	
		tftlcd_data.height=240;
		tftlcd_data.width=400;
#endif

#ifdef TFTLCD_R61509V3		
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0000);
		LCD_WriteCmd(0x0003);
		LCD_WriteData(0x1038);
		tftlcd_data.height=240;
		tftlcd_data.width=400;
#endif
		
#ifdef TFTLCD_HX8352C
		LCD_WriteCmd(0x16);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x60);  //60：横屏  
		tftlcd_data.height=240;
		tftlcd_data.width=400;			
#endif

#ifdef TFTLCD_ILI9341
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x68);  
		tftlcd_data.height=240;
		tftlcd_data.width=320;			
#endif

#ifdef TFTLCD_ILI9327
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x28);  
		tftlcd_data.height=240;
		tftlcd_data.width=400;			
#endif

#ifdef TFTLCD_ILI9325
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0000);
		LCD_WriteCmd(0x0003);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x1038);  
		tftlcd_data.height=240;
		tftlcd_data.width=320;			
#endif

#ifdef TFTLCD_ILI9486
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x60);  
		tftlcd_data.height=320;
		tftlcd_data.width=480;			
#endif

#ifdef TFTLCD_ST7793
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0000);
		LCD_WriteCmd(0x0003);
		LCD_WriteData(0x1038);
		tftlcd_data.height=240;
		tftlcd_data.width=400;			
#endif

#ifdef TFTLCD_SSD1963
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x00);  
		tftlcd_data.height=480;
		tftlcd_data.width=800;			
#endif

#ifdef TFTLCD_NT35510	
		LCD_WriteCmd(0x3600);   
		LCD_WriteData(0x0060);
		tftlcd_data.height=480;
		tftlcd_data.width=800;
#endif

#ifdef TFTLCD_ILI9481
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x60);  
		tftlcd_data.height=320;
		tftlcd_data.width=480;			
#endif

#ifdef TFTLCD_R61509VE		
		LCD_WriteCmd(0x0001);   
		LCD_WriteData(0x0000);
		LCD_WriteCmd(0x0003);
		LCD_WriteData(0x1038);
		tftlcd_data.height=240;
		tftlcd_data.width=400;
#endif

#ifdef TFTLCD_SSD1963N
		LCD_WriteCmd(0x36);   //设置彩屏显示方向的寄存器
		LCD_WriteData(0x00);  
		tftlcd_data.height=272;
		tftlcd_data.width=480;			
#endif

#ifdef TFTLCD_ILI9488		
		LCD_WriteCmdData(0x36,0x28);
		LCD_WriteCmd(0XB6);
		LCD_WriteData(0x00);
		LCD_WriteData(0x02);
		LCD_WriteData(0x3B);
		tftlcd_data.height=320;
		tftlcd_data.width=480;
#endif

#ifdef TFTLCD_ILI9806		
		LCD_WriteCmdData(0x36,0x68);
		tftlcd_data.height=480;
		tftlcd_data.width=800;
#endif
	}	
}


//设置窗口,并自动设置画点坐标到窗口左上角(sx,sy).
//sx,sy:窗口起始坐标(左上角)
//width,height:窗口宽度和高度,必须大于0!!
//窗体大小:width*height. 
void LCD_Set_Window(u16 sx,u16 sy,u16 width,u16 height)
{    
#ifdef TFTLCD_HX8357D	
	LCD_WriteCmd(0x2A);
    LCD_WriteData(sx>>8);
    LCD_WriteData(sx&0XFF);
    LCD_WriteData(width>>8);
    LCD_WriteData(width&0XFF);

    LCD_WriteCmd(0x2b);
    LCD_WriteData(sy>>8);
    LCD_WriteData(sy&0XFF);
    LCD_WriteData(height>>8);
    LCD_WriteData(height&0XFF);
    LCD_WriteCmd(0x2c); 
#endif

#ifdef TFTLCD_HX8357DN	
	LCD_WriteCmd(0x2A);
    LCD_WriteData(sx>>8);
    LCD_WriteData(sx&0XFF);
    LCD_WriteData(width>>8);
    LCD_WriteData(width&0XFF);

    LCD_WriteCmd(0x2b);
    LCD_WriteData(sy>>8);
    LCD_WriteData(sy&0XFF);
    LCD_WriteData(height>>8);
    LCD_WriteData(height&0XFF);
    LCD_WriteCmd(0x2c); 
#endif

#ifdef TFTLCD_R61509V	
	if(tftlcd_data.dir==0)
	{
		LCD_WriteCmd(0x0210);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0211);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0212);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0213);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0200);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0201);   
	    LCD_WriteData(sy);	
	}   	
	else
	{
		LCD_WriteCmd(0x0212);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0213);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0210);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0211);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0201);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0200);   
	    LCD_WriteData(sy);	
	}
	LCD_WriteCmd(0x0202);
#endif

#ifdef TFTLCD_R61509VN	
	if(tftlcd_data.dir==0)
	{
		LCD_WriteCmd(0x0210);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0211);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0212);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0213);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0200);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0201);   
	    LCD_WriteData(sy);	
	}   	
	else
	{
		LCD_WriteCmd(0x0212);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0213);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0210);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0211);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0201);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0200);   
	    LCD_WriteData(sy);	
	}
	LCD_WriteCmd(0x0202);
#endif	

#ifdef TFTLCD_R61509V3	
	if(tftlcd_data.dir==0)
	{
		LCD_WriteCmd(0x0210);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0211);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0212);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0213);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0200);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0201);   
	    LCD_WriteData(sy);	
	}   	
	else
	{
		LCD_WriteCmd(0x0212);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0213);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0210);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0211);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0201);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0200);   
	    LCD_WriteData(sy);	
	}
	LCD_WriteCmd(0x0202);
#endif
	
#ifdef TFTLCD_HX8352C
	LCD_WriteCmd(0x02);LCD_WriteData(sx/256);   
	LCD_WriteCmd(0x03);LCD_WriteData(sx%256); 	 
	LCD_WriteCmd(0x04);LCD_WriteData(width/256); 
	LCD_WriteCmd(0x05);LCD_WriteData(width%256);
	
	LCD_WriteCmd(0x06);LCD_WriteData(sy/256);  
	LCD_WriteCmd(0x07);LCD_WriteData(sy%256);
	LCD_WriteCmd(0x08);LCD_WriteData(height/256); 
	LCD_WriteCmd(0x09);LCD_WriteData(height%256); 	

	LCD_WriteCmd(0x22);

#endif

#ifdef TFTLCD_ILI9341
	LCD_WriteCmd(0x2A);
	LCD_WriteData(sx/256);   
	LCD_WriteData(sx%256); 	 
	LCD_WriteData(width/256); 
	LCD_WriteData(width%256);
	
	LCD_WriteCmd(0x2B);
	LCD_WriteData(sy/256);  
	LCD_WriteData(sy%256);
	LCD_WriteData(height/256); 
	LCD_WriteData(height%256); 	

	LCD_WriteCmd(0x2C);

#endif

#ifdef TFTLCD_ILI9327
	LCD_WriteCmd(0x2A);
	LCD_WriteData(sx/256);   
	LCD_WriteData(sx%256); 	 
	LCD_WriteData(width/256); 
	LCD_WriteData(width%256);
	
	LCD_WriteCmd(0x2B);
	LCD_WriteData(sy/256);  
	LCD_WriteData(sy%256);
	LCD_WriteData(height/256); 
	LCD_WriteData(height%256); 	

	LCD_WriteCmd(0x2C);

#endif

#ifdef TFTLCD_ILI9325
	if(tftlcd_data.dir==0)
	{
		LCD_WriteCmd(0x0050);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0051);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0052);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0053);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0020);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0021);   
	    LCD_WriteData(sy);	
	}   	
	else
	{
		LCD_WriteCmd(0x0052);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0053);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0050);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0051);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0021);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0020);   
	    LCD_WriteData(sy);	
	}
	LCD_WriteCmd(0x0022);
#endif

#ifdef TFTLCD_ILI9486
	LCD_WriteCmd(0x2A);
	LCD_WriteData(sx/256);   
	LCD_WriteData(sx%256); 	 
	LCD_WriteData(width/256); 
	LCD_WriteData(width%256);
	
	LCD_WriteCmd(0x2B);
	LCD_WriteData(sy/256);  
	LCD_WriteData(sy%256);
	LCD_WriteData(height/256); 
	LCD_WriteData(height%256); 	

	LCD_WriteCmd(0x2C);

#endif

#ifdef TFTLCD_ST7793	
	if(tftlcd_data.dir==0)
	{
		LCD_WriteCmd(0x0210);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0211);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0212);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0213);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0200);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0201);   
	    LCD_WriteData(sy);	
	}   	
	else
	{
		LCD_WriteCmd(0x0212);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0213);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0210);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0211);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0201);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0200);   
	    LCD_WriteData(sy);	
	}
	LCD_WriteCmd(0x0202);		
#endif

#ifdef TFTLCD_SSD1963
	if(tftlcd_data.dir==0)
	{
		LCD_WriteCmd(0x2B);
		LCD_WriteData(sx/256);   
		LCD_WriteData(sx%256); 	 
		LCD_WriteData(width/256); 
		LCD_WriteData(width%256);
		
		LCD_WriteCmd(0x2A);
		LCD_WriteData(sy/256);  
		LCD_WriteData(sy%256);
		LCD_WriteData(height/256); 
		LCD_WriteData(height%256);
	}
	else
	{
		LCD_WriteCmd(0x2A);
		LCD_WriteData(sx/256);   
		LCD_WriteData(sx%256); 	 
		LCD_WriteData(width/256); 
		LCD_WriteData(width%256);
		
		LCD_WriteCmd(0x2B);
		LCD_WriteData(sy/256);  
		LCD_WriteData(sy%256);
		LCD_WriteData(height/256); 
		LCD_WriteData(height%256);
	}
	LCD_WriteCmd(0x2C);

#endif

#ifdef TFTLCD_NT35510	
	LCD_WriteCmd(0X2A00);LCD_WriteData(sx>>8);  
	LCD_WriteCmd(0X2A01);LCD_WriteData(sx&0XFF);	  
	LCD_WriteCmd(0X2A02);LCD_WriteData(width>>8);   
	LCD_WriteCmd(0X2A03);LCD_WriteData(width&0XFF);   
	LCD_WriteCmd(0X2B00);LCD_WriteData(sy>>8);   
	LCD_WriteCmd(0X2B01);LCD_WriteData(sy&0XFF);  
	LCD_WriteCmd(0X2B02);LCD_WriteData(height>>8);   
	LCD_WriteCmd(0X2B03);LCD_WriteData(height&0XFF);
	LCD_WriteCmd(0x2C00);
#endif

#ifdef TFTLCD_ILI9481
	LCD_WriteCmd(0x2A);
	LCD_WriteData(sx/256);   
	LCD_WriteData(sx%256); 	 
	LCD_WriteData(width/256); 
	LCD_WriteData(width%256);
	
	LCD_WriteCmd(0x2B);
	LCD_WriteData(sy/256);  
	LCD_WriteData(sy%256);
	LCD_WriteData(height/256); 
	LCD_WriteData(height%256); 	

	LCD_WriteCmd(0x2C);
#endif

#ifdef TFTLCD_R61509VE	
	if(tftlcd_data.dir==0)
	{
		LCD_WriteCmd(0x0210);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0211);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0212);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0213);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0200);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0201);   
	    LCD_WriteData(sy);	
	}   	
	else
	{
		LCD_WriteCmd(0x0212);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0213);  
	    LCD_WriteData(width);
		LCD_WriteCmd(0x0210);   
	    LCD_WriteData(sy);
		LCD_WriteCmd(0x0211);   
	    LCD_WriteData(height);
	
		LCD_WriteCmd(0x0201);   
	    LCD_WriteData(sx);
		LCD_WriteCmd(0x0200);   
	    LCD_WriteData(sy);	
	}
	LCD_WriteCmd(0x0202);
#endif

#ifdef TFTLCD_SSD1963N
	if(tftlcd_data.dir==0)
	{
		LCD_WriteCmd(0x2B);
		LCD_WriteData(sx/256);   
		LCD_WriteData(sx%256); 	 
		LCD_WriteData(width/256); 
		LCD_WriteData(width%256);
		
		LCD_WriteCmd(0x2A);
		LCD_WriteData(sy/256);  
		LCD_WriteData(sy%256);
		LCD_WriteData(height/256); 
		LCD_WriteData(height%256);
	}
	else
	{
		LCD_WriteCmd(0x2A);
		LCD_WriteData(sx/256);   
		LCD_WriteData(sx%256); 	 
		LCD_WriteData(width/256); 
		LCD_WriteData(width%256);
		
		LCD_WriteCmd(0x2B);
		LCD_WriteData(sy/256);  
		LCD_WriteData(sy%256);
		LCD_WriteData(height/256); 
		LCD_WriteData(height%256);
	}
	LCD_WriteCmd(0x2C);
#endif

#ifdef TFTLCD_ILI9488
	LCD_WriteCmd(0x2A);
	LCD_WriteData(sx/256);   
	LCD_WriteData(sx%256); 	 
	LCD_WriteData(width/256); 
	LCD_WriteData(width%256);
	
	LCD_WriteCmd(0x2B);
	LCD_WriteData(sy/256);  
	LCD_WriteData(sy%256);
	LCD_WriteData(height/256); 
	LCD_WriteData(height%256); 	

	LCD_WriteCmd(0x2C);

#endif

#ifdef TFTLCD_ILI9806
	LCD_WriteCmd(0x2A);
	LCD_WriteData(sx/256);   
	LCD_WriteData(sx%256); 	 
	LCD_WriteData(width/256); 
	LCD_WriteData(width%256);
	
	LCD_WriteCmd(0x2B);
	LCD_WriteData(sy/256);  
	LCD_WriteData(sy%256);
	LCD_WriteData(height/256); 
	LCD_WriteData(height%256); 	

	LCD_WriteCmd(0x2C);

#endif
}

//读取个某点的颜色值	 
//x,y:坐标
//返回值:此点的颜色
u16 LCD_ReadPoint(u16 x,u16 y)
{
 	u16 r=0,g=0,b=0;
	u16 r1,r2,r3;
	u32 value;
	
	if(x>=tftlcd_data.width||y>=tftlcd_data.height)return 0;	//超过了范围,直接返回		     
	LCD_Set_Window(x, y, x, y);
	
#ifdef TFTLCD_HX8357D	
	LCD_WriteCmd(0X2E);     		 				    
 	r=LCD_ReadData();								//dummy Read	   
 	r=LCD_ReadData();  		  						//实际坐标颜色
#endif
	
#ifdef TFTLCD_HX8357DN	
//	LCD_WriteCmd(0X2E);
//	r=TFTLCD->LCD_DATA;
//	r=TFTLCD->LCD_DATA<<8;
//	r|=TFTLCD->LCD_DATA;
	LCD_WriteCmd(0X2E);	
	r=TFTLCD->LCD_DATA;
	r=TFTLCD->LCD_DATA;
	g=TFTLCD->LCD_DATA;
	b=TFTLCD->LCD_DATA;
	r=r<<8|(g&0xff);
#endif

#ifdef TFTLCD_R61509V	
	LCD_WriteCmd(0X0202);     		 				    
 	r=LCD_ReadData();								//dummy Read	   
 	r=LCD_ReadData(); 
	r=LCD_ReadData();	  						
#endif

#ifdef TFTLCD_R61509VN	
	LCD_WriteCmd(0X0202);
//	r=LCD_ReadData();								//dummy Read	   
// 	r=LCD_ReadData();
//	printf("r=%x\r\n",r);
	
			

	value=TFTLCD->LCD_DATA;
	value=value<<9;
	value|=TFTLCD->LCD_DATA;
	
	value=TFTLCD->LCD_DATA;
	value=value<<9;
	value|=TFTLCD->LCD_DATA;
	printf("value=%X\r\n",value);
	r1=value>>13;
	r2=(value>>6)&0x3f;
	r3=(value>>1)&0x1f;	
	r=(r1<<11)|(r2<<5)|r3;
	printf("r=%X\r\n",r);
	
#endif

#ifdef TFTLCD_R61509V3	
	LCD_WriteCmd(0X0202);     		 				    
 	r=LCD_ReadData();								//dummy Read	   
 	r=LCD_ReadData(); 
	r=LCD_ReadData();	  						
#endif

#ifdef TFTLCD_HX8352C	
	LCD_WriteCmd(0X22);     		 				    
 	r=TFTLCD->LCD_DATA;	
	r=LCD_ReadData();
#endif	

#ifdef TFTLCD_ILI9341	
	LCD_WriteCmd(0X2e);
	r=LCD_ReadData();								//dummy Read     		 				    
	r=LCD_ReadData();								//实际坐标颜色	
	//printf("r=%X\r\n",r);
	b=LCD_ReadData(); 
	g=r&0xff;
	g<<=8;
	return (((r>>11)<<11)|((g>>10)<<5)|(b>>11));
#endif

#ifdef TFTLCD_ILI9327	
	LCD_WriteCmd(0X2e);
	r=LCD_ReadData();								//dummy Read     		 				    
	r=LCD_ReadData();								//实际坐标颜色	
	r=(r<<8)|(r>>8);
	return r;
#endif

#ifdef TFTLCD_ILI9325
	LCD_WriteCmd(0x0022);
	r=LCD_ReadData();//dummy Read     		 				    
	r=LCD_ReadData();//实际坐标颜色
	r1=r&0xf800;
	r2=r&0x07e0;
	r3=r&0x001f;
	return (r3<<11)|((r2>>5)<<5)|(r1>>11);
#endif

#ifdef TFTLCD_ILI9486	
	LCD_WriteCmd(0X2e);
	r=LCD_ReadData();								//dummy Read     		 				    
	r=LCD_ReadData();								//实际坐标颜色	
	//printf("r=%X\r\n",r);
	b=LCD_ReadData();
	g=LCD_ReadData(); 
	//printf("g=%X\r\n",g);
	//printf("b=%X\r\n",b);
	return (((r>>11)<<11)|((g>>10)<<5)|(b>>11));
#endif

#ifdef TFTLCD_ST7793	
	if(x>=tftlcd_data.width||y>=tftlcd_data.height)return 0;	//超过了范围,直接返回		     
	LCD_Set_Window(x, y, x, y);
	LCD_WriteCmd(0x0202);     		 				    
 	r=LCD_ReadData();  		  						//实际坐标颜色
#endif

#ifdef TFTLCD_SSD1963	
	LCD_WriteCmd(0X2E);     		 				    
	r=LCD_ReadData();
#endif	
	
#ifdef TFTLCD_NT35510	
	LCD_WriteCmd(0X2E00);
	r=LCD_ReadData();								//dummy Read     		 				    
	r=LCD_ReadData();								//实际坐标颜色
	
	b=LCD_ReadData(); 
	g=r&0XFF;		//对于9341/5310/5510,第一次读取的是RG的值,R在前,G在后,各占8位
	g<<=8;
	
	return (((r>>11)<<11)|((g>>10)<<5)|(b>>11));
#endif

#ifdef TFTLCD_ILI9481	
	LCD_WriteCmd(0X2e);
	r=LCD_ReadData();								//dummy Read     		 				    
	r=LCD_ReadData();								//实际坐标颜色	
	//printf("r=%X\r\n",r);
	b=LCD_ReadData();
	g=LCD_ReadData(); 
	//printf("g=%X\r\n",g);
	//printf("b=%X\r\n",b);
	return (((r>>11)<<11)|((g>>10)<<5)|(b>>11));
#endif

#ifdef TFTLCD_R61509VE	
	LCD_WriteCmd(0X0202);     		 				    
 	r=LCD_ReadData();								//dummy Read	   
 	r=LCD_ReadData(); 
	r=LCD_ReadData();	  						
#endif

#ifdef TFTLCD_SSD1963N	
	LCD_WriteCmd(0X2E);     		 				    
	r=LCD_ReadData();
#endif

#ifdef TFTLCD_ILI9488	
	LCD_WriteCmd(0X2e);
	r=LCD_ReadData();								//dummy Read     		 				    
	r=LCD_ReadData();								//实际坐标颜色	
	b=LCD_ReadData();
	g=LCD_ReadData(); 
	return (((r>>11)<<11)|((g>>10)<<5)|(b>>11));
#endif


#ifdef TFTLCD_ILI9806	
	LCD_WriteCmd(0X2e);
	r=LCD_ReadData();								//dummy Read     		 				    
	r=LCD_ReadData();								//实际坐标颜色	
	//printf("r=%X\r\n",r);
	b=LCD_ReadData();
	g=LCD_ReadData(); 
	//printf("g=%X\r\n",g);
	//printf("b=%X\r\n",b);
	return (((r>>11)<<11)|((g>>10)<<5)|(b>>11));
#endif
 	return r;						
}

void TFTLCD_GPIO_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD|RCC_AHB1Periph_GPIOE|
							RCC_AHB1Periph_GPIOF|RCC_AHB1Periph_GPIOG, ENABLE);//使能PD,PE,PF,PG时钟  

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource14,GPIO_AF_FSMC);  //FSMC_D0-FSMC_D15
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource15,GPIO_AF_FSMC);  
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource0,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource1,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource7,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource8,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource9,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource10,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource11,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource12,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource13,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource14,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource15,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource8,GPIO_AF_FSMC); 
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource9,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource10,GPIO_AF_FSMC);
	
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource4,GPIO_AF_FSMC);  //FSMC_NOE
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource5,GPIO_AF_FSMC);  //FSMC_NWE
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource12,GPIO_AF_FSMC);//FSMC_A6
	GPIO_PinAFConfig(GPIOG,GPIO_PinSource12,GPIO_AF_FSMC);//FSMC_NE4
	
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 
	                              | GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_9
								  | GPIO_Pin_10 | GPIO_Pin_14| GPIO_Pin_15;//PD0,1,4,5,8,9,10,14,15 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用输出
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
	GPIO_Init(GPIOD, &GPIO_InitStructure);//初始化  
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 
	                              | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12
								  | GPIO_Pin_13 | GPIO_Pin_14| GPIO_Pin_15;//PE7~15
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用输出
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
	GPIO_Init(GPIOE, &GPIO_InitStructure);//初始化  

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;//PF12,FSMC_A6
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用输出
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
	GPIO_Init(GPIOF, &GPIO_InitStructure);//初始化  

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;//PG12,FSMC_NE4
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用输出
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
	GPIO_Init(GPIOG, &GPIO_InitStructure);//初始化 
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;//PB15 推挽输出,控制背光
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//普通输出模式
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//100MHz
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
	GPIO_Init(GPIOB, &GPIO_InitStructure);//初始化 //PB15 推挽输出,控制背光
	LCD_LED=1;				//点亮背光
	
}

void TFTLCD_FSMC_Init(void)
{
	FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
  	FSMC_NORSRAMTimingInitTypeDef  FSMC_ReadTimingInitStructure; 
	FSMC_NORSRAMTimingInitTypeDef  FSMC_WriteTimingInitStructure;
	
  	RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC,ENABLE);//使能FSMC时钟
	
	FSMC_ReadTimingInitStructure.FSMC_AddressSetupTime = 0x0F;	 //地址建立时间（ADDSET）为16个HCLK 1/168M=6ns*16=96ns
  	FSMC_ReadTimingInitStructure.FSMC_AddressHoldTime = 0x00;	 //地址保持时间（ADDHLD）模式A未用到	
  	FSMC_ReadTimingInitStructure.FSMC_DataSetupTime = 0x60;		 // 数据保存时间为60个HCLK	=6*60=360ns
  	FSMC_ReadTimingInitStructure.FSMC_BusTurnAroundDuration = 0x00;
  	FSMC_ReadTimingInitStructure.FSMC_CLKDivision = 0x00;
  	FSMC_ReadTimingInitStructure.FSMC_DataLatency = 0x00;
  	FSMC_ReadTimingInitStructure.FSMC_AccessMode = FSMC_AccessMode_A;	 //模式A 

	FSMC_WriteTimingInitStructure.FSMC_AddressSetupTime = 0x15;	 //地址建立时间（ADDSET）为16个HCLK  
  	FSMC_WriteTimingInitStructure.FSMC_AddressHoldTime = 0x15;	 //地址保持时间		
  	FSMC_WriteTimingInitStructure.FSMC_DataSetupTime = 0x05;		 //数据保存时间为6个HCLK	
  	FSMC_WriteTimingInitStructure.FSMC_BusTurnAroundDuration = 0x00;
  	FSMC_WriteTimingInitStructure.FSMC_CLKDivision = 0x00;
  	FSMC_WriteTimingInitStructure.FSMC_DataLatency = 0x00;
  	FSMC_WriteTimingInitStructure.FSMC_AccessMode = FSMC_AccessMode_A;	 //模式A  

 
  	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM4;//  这里我们使用NE4 ，也就对应BTCR[6],[7]。
  	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable; // 不复用数据地址
  	FSMC_NORSRAMInitStructure.FSMC_MemoryType =FSMC_MemoryType_SRAM;// FSMC_MemoryType_SRAM;  //SRAM   
  	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;//存储器数据宽度为16bit   
  	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode =FSMC_BurstAccessMode_Disable;// FSMC_BurstAccessMode_Disable; 
  	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait=FSMC_AsynchronousWait_Disable; 
  	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;   
  	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;  
  	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;	//  存储器写使能
  	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;   
  	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Enable; // 读写使用不同的时序
  	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable; 
  	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &FSMC_ReadTimingInitStructure; //读写时序
  	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &FSMC_WriteTimingInitStructure;  //写时序

  	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);  //初始化FSMC配置

 	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);  // 使能BANK1 
}
	 
//SSD1963 背光设置
//pwm:背光等级,0~100.越大越亮.
void LCD_SSD_BackLightSet(u8 pwm)
{	
	LCD_WriteCmd(0xBE);	//配置PWM输出
	LCD_WriteData(0x05);	//1设置PWM频率
	LCD_WriteData(pwm*2.55);//2设置PWM占空比
	LCD_WriteData(0x01);	//3设置C
	LCD_WriteData(0xFF);	//4设置D
	LCD_WriteData(0x00);	//5设置E
	LCD_WriteData(0x00);	//6设置F
}

void TFTLCD_Init(void)
{
	u16 i;

	TFTLCD_GPIO_Init();
	TFTLCD_FSMC_Init();
	
	delay_ms(50); 
	
#ifdef TFTLCD_HX8357D	
	LCD_WriteCmd(0Xd0);				   
	tftlcd_data.id=LCD_ReadData();	//dummy read 	
	tftlcd_data.id=LCD_ReadData();    
#endif 

#ifdef TFTLCD_HX8357DN	
	LCD_WriteCmd(0Xd0);				   
	tftlcd_data.id=LCD_ReadData();	//dummy read 	
	tftlcd_data.id=LCD_ReadData();    
#endif 

#ifdef TFTLCD_R61509V	
	LCD_WriteCmd(0X0000);				   	
	tftlcd_data.id=LCD_ReadData();    
#endif

#ifdef TFTLCD_R61509VN	
	LCD_WriteCmd(0X0000);				   	
	tftlcd_data.id=LCD_ReadData();    
#endif

#ifdef TFTLCD_R61509V3	
	LCD_WriteCmd(0X0000);				   	
	tftlcd_data.id=LCD_ReadData();    
#endif
	
#ifdef TFTLCD_HX8352C
	LCD_WriteCmd(0X00);	
	tftlcd_data.id=TFTLCD->LCD_DATA>>8; 
#endif

#ifdef TFTLCD_ILI9341
	LCD_WriteCmd(0Xd3);	
	tftlcd_data.id=TFTLCD->LCD_DATA;	//dummy read 
	tftlcd_data.id=TFTLCD->LCD_DATA;  //00
	tftlcd_data.id=TFTLCD->LCD_DATA;  //93
	tftlcd_data.id|=(tftlcd_data.id<<8)|TFTLCD->LCD_DATA; //41
#endif

#ifdef TFTLCD_ILI9327
	LCD_WriteCmd(0XEF);	
	tftlcd_data.id=TFTLCD->LCD_DATA;	//dummy read 
	tftlcd_data.id=TFTLCD->LCD_DATA;  //02
	tftlcd_data.id=TFTLCD->LCD_DATA;  //04
	tftlcd_data.id=TFTLCD->LCD_DATA;  //93
	tftlcd_data.id=(tftlcd_data.id<<8)|TFTLCD->LCD_DATA;  //27
#endif

#ifdef TFTLCD_ILI9325
	LCD_WriteCmd(0x00);	
	tftlcd_data.id=TFTLCD->LCD_DATA;			
#endif

#ifdef TFTLCD_ILI9486
	LCD_WriteCmd(0Xd3);	
	tftlcd_data.id=TFTLCD->LCD_DATA;	//dummy read 
	tftlcd_data.id=TFTLCD->LCD_DATA;  //00
	tftlcd_data.id=TFTLCD->LCD_DATA;  //94
	tftlcd_data.id<<=8;
	tftlcd_data.id|=TFTLCD->LCD_DATA;  //86
#endif

#ifdef TFTLCD_ST7793	
	LCD_WriteCmd(0X0000);				   	
	tftlcd_data.id=LCD_ReadData();	
#endif

#ifdef TFTLCD_SSD1963
	LCD_WriteCmd(0XA1);
	tftlcd_data.id=LCD_ReadData();
	tftlcd_data.id=LCD_ReadData();	//读回0X57
	tftlcd_data.id<<=8;	 
	tftlcd_data.id|=LCD_ReadData();	//读回0X61			
#endif
	
#ifdef TFTLCD_NT35510	
	LCD_WriteCmd(0XDA00);	
	tftlcd_data.id=LCD_ReadData();		//读回0X00	 
	LCD_WriteCmd(0XDB00);	
	tftlcd_data.id=LCD_ReadData();		//读回0X80
	tftlcd_data.id<<=8;	
	LCD_WriteCmd(0XDC00);	
	tftlcd_data.id|=LCD_ReadData();		//读回0X00
#endif

#ifdef TFTLCD_ILI9481
	LCD_WriteCmd(0Xd3);	
	tftlcd_data.id=TFTLCD->LCD_DATA;	 
	tftlcd_data.id=TFTLCD->LCD_DATA;  
	tftlcd_data.id=TFTLCD->LCD_DATA;  
	tftlcd_data.id<<=8;
	tftlcd_data.id|=TFTLCD->LCD_DATA;  
#endif

#ifdef TFTLCD_R61509VE	
	LCD_WriteCmd(0X0000);				   	
	tftlcd_data.id=LCD_ReadData();    
#endif

#ifdef TFTLCD_ILI9488
LCD_WriteCmd(0XD3);				   
		tftlcd_data.id=TFTLCD->LCD_DATA;	//dummy read 	
 		tftlcd_data.id=TFTLCD->LCD_DATA;	//读到0X00
  		tftlcd_data.id=TFTLCD->LCD_DATA;   	//读取94							   
 		tftlcd_data.id<<=8;
		tftlcd_data.id|=TFTLCD->LCD_DATA;  	//读取88
	
#endif

#ifdef TFTLCD_ILI9806
	LCD_WriteCmd(0Xd3);	
	tftlcd_data.id=TFTLCD->LCD_DATA;	 
	tftlcd_data.id=TFTLCD->LCD_DATA;  
	tftlcd_data.id=TFTLCD->LCD_DATA;  
	tftlcd_data.id<<=8;
	tftlcd_data.id|=TFTLCD->LCD_DATA;  
#endif
	
 	printf(" LCD ID:%x\r\n",tftlcd_data.id); //打印LCD ID
	
#ifdef TFTLCD_HX8357D	
	LCD_WriteCmd(0xE9); 
	LCD_WriteData(0x20); 
	
	LCD_WriteCmd(0x11); //Exit Sleep 
	for(i=500; i>0; i--);
	
	LCD_WriteCmd(0x3A);
	LCD_WriteData(0x55);  //16Bit colors
	
	LCD_WriteCmd(0xD1); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x65); //调试此值改善水纹
	LCD_WriteData(0x1F); 
	
	LCD_WriteCmd(0xD0); 
	LCD_WriteData(0x07); 
	LCD_WriteData(0x07); 
	LCD_WriteData(0x80); 
	
	LCD_WriteCmd(0x36); 	 //Set_address_mode
	LCD_WriteData(0x4c);   	//4c
	
	LCD_WriteCmd(0xC1); 
	LCD_WriteData(0x10); 
	LCD_WriteData(0x10); 
	LCD_WriteData(0x02); 
	LCD_WriteData(0x02); 
	
	LCD_WriteCmd(0xC0); //Set Default Gamma 
	LCD_WriteData(0x00);  
	LCD_WriteData(0x35); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x02); 

	LCD_WriteCmd(0xC4);			
	LCD_WriteData(0x03);

	LCD_WriteCmd(0xC5); //Set frame rate 
	LCD_WriteData(0x01); 
	
	LCD_WriteCmd(0xD2); //power setting 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x22); 

	LCD_WriteCmd(0xE7);			
	LCD_WriteData(0x38);
	
	LCD_WriteCmd(0xF3);			
    LCD_WriteData(0x08); 
	LCD_WriteData(0x12);
	LCD_WriteData(0x12);
	LCD_WriteData(0x08);

	LCD_WriteCmd(0xC8); //Set Gamma 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x52); 
	LCD_WriteData(0x37); 
	LCD_WriteData(0x10); 
	LCD_WriteData(0x0d); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x04); 
	LCD_WriteData(0x51); 
	LCD_WriteData(0x77); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x0d); 
	LCD_WriteData(0x08); 
	LCD_WriteData(0x80); 
	LCD_WriteData(0x00);

    LCD_WriteCmd(0x29);
#endif

#ifdef TFTLCD_HX8357DN
	LCD_WriteCmd(0xE9); 
	LCD_WriteData(0x20); 
	
	LCD_WriteCmd(0x11); //Exit Sleep 
	for(i=500; i>0; i--);
	
	LCD_WriteCmd(0x3A);
	LCD_WriteData(0x55);  //16Bit colors
	
	LCD_WriteCmd(0xD1); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x65); //调试此值改善水纹
	LCD_WriteData(0x1F); 
	
	LCD_WriteCmd(0xD0); 
	LCD_WriteData(0x07); 
	LCD_WriteData(0x07); 
	LCD_WriteData(0x80); 
	
	LCD_WriteCmd(0x36); 	 //Set_address_mode
	LCD_WriteData(0x4c);   	//4c
	
	LCD_WriteCmd(0xC1); 
	LCD_WriteData(0x10); 
	LCD_WriteData(0x10); 
	LCD_WriteData(0x02); 
	LCD_WriteData(0x02); 
	
	LCD_WriteCmd(0xC0); //Set Default Gamma 
	LCD_WriteData(0x00);  
	LCD_WriteData(0x35); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x02); 

	LCD_WriteCmd(0xC4);			
	LCD_WriteData(0x03);

	LCD_WriteCmd(0xC5); //Set frame rate 
	LCD_WriteData(0x01); 
	
	LCD_WriteCmd(0xD2); //power setting 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x22); 

	LCD_WriteCmd(0xE7);			
	LCD_WriteData(0x38);
	
	LCD_WriteCmd(0xF3);			
    LCD_WriteData(0x08); 
	LCD_WriteData(0x12);
	LCD_WriteData(0x12);
	LCD_WriteData(0x08);

	LCD_WriteCmd(0xC8); //Set Gamma 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x52); 
	LCD_WriteData(0x37); 
	LCD_WriteData(0x10); 
	LCD_WriteData(0x0d); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x04); 
	LCD_WriteData(0x51); 
	LCD_WriteData(0x77); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x0d); 
	LCD_WriteData(0x08); 
	LCD_WriteData(0x80); 
	LCD_WriteData(0x00);

    LCD_WriteCmd(0x29);
#endif

#ifdef TFTLCD_R61509V
	LCD_WriteCmd(0x0000);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0000);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0000);LCD_WriteData(0x0000);
	
	LCD_WriteCmd(0x0400);LCD_WriteData(0x6200); 
	LCD_WriteCmd(0x0008);LCD_WriteData(0x0808); 


	LCD_WriteCmd(0x0001);LCD_WriteData(0x0100);
 	LCD_WriteCmd(0x0002);LCD_WriteData(0x0100);
	LCD_WriteCmd(0x0003);LCD_WriteData(0x1030);
	LCD_WriteCmd(0x0008);LCD_WriteData(0x0808);
	/*gamma set 20090423 BY LYJ*/

	 LCD_WriteCmd(0x0300);LCD_WriteData(0x000c); 
	 LCD_WriteCmd(0x0301);LCD_WriteData(0x5A0B); 
	 LCD_WriteCmd(0x0302);LCD_WriteData(0x0906); 
	 LCD_WriteCmd(0x0303);LCD_WriteData(0x1017); 
	 LCD_WriteCmd(0x0304);LCD_WriteData(0x2300); 
	 LCD_WriteCmd(0x0305);LCD_WriteData(0x1700); 
	 LCD_WriteCmd(0x0306);LCD_WriteData(0x6309); 
	 LCD_WriteCmd(0x0307);LCD_WriteData(0x0c09); 
	 LCD_WriteCmd(0x0308);LCD_WriteData(0x100c); 
	 LCD_WriteCmd(0x0309);LCD_WriteData(0x2232); 
	 /*power set*/
	  LCD_WriteCmd(0x0010);LCD_WriteData(0x0014);
	  LCD_WriteCmd(0x0011);LCD_WriteData(0x0101);  
	  LCD_WriteCmd(0x0100);LCD_WriteData(0x0230);//
		//for(i=500; i>0; i--);
		delay_ms(50);
	  LCD_WriteCmd(0x0101);LCD_WriteData(0x0247);//
		for(i=500; i>0; i--);
		delay_ms(50);
	  LCD_WriteCmd(0x0103);LCD_WriteData(0x0a00);//Starts VLOUT3,Sets the VREG1OUT.
		for(i=500; i>0; i--);
		delay_ms(10);
	  LCD_WriteCmd(0x0280);LCD_WriteData(0xf0ff);//VCOMH voltage   //0xcEff
		delay_ms(50);
		for(i=500; i>0; i--);
	  LCD_WriteCmd(0x0102);LCD_WriteData(0xB1b0);//Starts VLOUT3,Sets the VREG1OUT.
		for(i=500; i>0; i--);
		delay_ms(50);
	 /*window set*/
	  LCD_WriteCmd(0x0210);LCD_WriteData(0x0000);//Window Horizontal RAM Address Start
	  LCD_WriteCmd(0x0211);LCD_WriteData(0x00ef);//Window Horizontal RAM Address End
	  LCD_WriteCmd(0x0212);LCD_WriteData(0x0000);//Window Vertical RAM Address Start
	  LCD_WriteCmd(0x0213);LCD_WriteData(0x018f);//Window Vertical RAM Address End
	  LCD_WriteCmd(0x0200);LCD_WriteData(0x0000);//RAM Address Set (Horizontal Address)
	  LCD_WriteCmd(0x0201);LCD_WriteData(0x0000);//RAM Address Set (Vertical Address)
	  LCD_WriteCmd(0x0401);LCD_WriteData(0x0000);//Base Image Display
	  LCD_WriteCmd(0x0007);LCD_WriteData(0x0100);//Display Control 1
		for(i=500; i>0; i--);
		delay_ms(50);
	  LCD_WriteCmd(0x0202);
#endif

#ifdef TFTLCD_R61509VN
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	delay_ms(10);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x00A4);LCD_WriteData(0x0001);
	delay_ms(10);

	LCD_WriteCmd(0x0060);LCD_WriteData(0x2700);
	LCD_WriteCmd(0x0008);LCD_WriteData(0x0808);

	//----------- Adjust the Gamma Curve ----------/
	LCD_WriteCmd(0x0300);LCD_WriteData(0x0109);  // Gamma Control
	LCD_WriteCmd(0x0301);LCD_WriteData(0x7E0A); 
	LCD_WriteCmd(0x0302);LCD_WriteData(0x0704); 
	LCD_WriteCmd(0x0303);LCD_WriteData(0x0911); 
	LCD_WriteCmd(0x0304);LCD_WriteData(0x2100); 
	LCD_WriteCmd(0x0305);LCD_WriteData(0x1109); 
	LCD_WriteCmd(0x0306);LCD_WriteData(0x7407); 
	LCD_WriteCmd(0x0307);LCD_WriteData(0x0A0E); 
	LCD_WriteCmd(0x0308);LCD_WriteData(0x0901); 
	LCD_WriteCmd(0x0309);LCD_WriteData(0x0021);

	LCD_WriteCmd(0x0010);LCD_WriteData(0x0010);  // Frame frequence
	LCD_WriteCmd(0x0011);LCD_WriteData(0x0202);  // 
	LCD_WriteCmd(0x0012);LCD_WriteData(0x0300); 
	LCD_WriteCmd(0x0013);LCD_WriteData(0x0007);
	delay_ms(10);
	// -------------- Power On sequence -------------//
	LCD_WriteCmd(0x0100);LCD_WriteData(0x0000);  // VGH/VGL 6/-3
	LCD_WriteCmd(0x0101);LCD_WriteData(0x0007);  // VCI1
	LCD_WriteCmd(0x0102);LCD_WriteData(0x0000);  // VREG1
	LCD_WriteCmd(0x0103);LCD_WriteData(0x0000);  // VDV 
	LCD_WriteCmd(0x0280);LCD_WriteData(0x0000);  // VCM
	delay_ms(200);//delay_ms 200ms
	LCD_WriteCmd(0x0100);LCD_WriteData(0x0330);  // VGH/VGL 6/-3
	LCD_WriteCmd(0x0101);LCD_WriteData(0x0247);  // VCI1
	delay_ms(50);//delay_ms 50ms
	LCD_WriteCmd(0x0102);LCD_WriteData(0xD1B0);  // VREG1
	delay_ms(50);//delay_ms 50ms
	LCD_WriteCmd(0x0103);LCD_WriteData(0x1000);  // VDV          //0900
	LCD_WriteCmd(0x0280);LCD_WriteData(0xC600);  // VCM          //chenyu 0xc600

	delay_ms(200);

	LCD_WriteCmd(0x0001);LCD_WriteData(0x0100);
	LCD_WriteCmd(0x0002);LCD_WriteData(0x0200);
	LCD_WriteCmd(0x0003);LCD_WriteData(0x1030);
	LCD_WriteCmd(0x0009);LCD_WriteData(0x0001);
	LCD_WriteCmd(0x000A);LCD_WriteData(0x0008);
	LCD_WriteCmd(0x000C);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x000D);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x000E);LCD_WriteData(0x0030);
	LCD_WriteCmd(0x0020);LCD_WriteData(0x0000);//H Start
	LCD_WriteCmd(0x0021);LCD_WriteData(0x0000);//V Start
	LCD_WriteCmd(0x0029);LCD_WriteData(0x0052);
	LCD_WriteCmd(0x0050);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0051);LCD_WriteData(0x00EF);
	LCD_WriteCmd(0x0052);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0053);LCD_WriteData(0x013F);
	LCD_WriteCmd(0x0061);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x006A);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0080);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0081);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0082);LCD_WriteData(0x005F);
	LCD_WriteCmd(0x0093);LCD_WriteData(0x0507);

	LCD_WriteCmd(0x0007);LCD_WriteData(0x0100);//DISPLAY ON
	delay_ms(50);
	LCD_WriteCmd(0x0022);//GRAM Data Write
#endif

#ifdef TFTLCD_R61509V3
	/* --R61509V_CPT3.0 --- */

	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	delay_ms(10);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	LCD_WriteCmd(0x0000);
	delay_ms(10);
	
	LCD_WriteCmd(0x0400); LCD_WriteData(0x6200);
	LCD_WriteCmd(0x0008); LCD_WriteData(0x0808);
	
	LCD_WriteCmd(0x0300); LCD_WriteData(0x0005);//gamma
	LCD_WriteCmd(0x0301); LCD_WriteData(0x4C06);
	LCD_WriteCmd(0x0302); LCD_WriteData(0x0602);
	LCD_WriteCmd(0x0303); LCD_WriteData(0x050C);
	LCD_WriteCmd(0x0304); LCD_WriteData(0x3300);
	LCD_WriteCmd(0x0305); LCD_WriteData(0x0C05);
	LCD_WriteCmd(0x0306); LCD_WriteData(0x4206);
	LCD_WriteCmd(0x0307); LCD_WriteData(0x060C);
	LCD_WriteCmd(0x0308); LCD_WriteData(0x0500);
	LCD_WriteCmd(0x0309); LCD_WriteData(0x0033);
	
	LCD_WriteCmd(0x0010); LCD_WriteData(0x0014);//
	LCD_WriteCmd(0x0011); LCD_WriteData(0x0101);//
	LCD_WriteCmd(0x0012); LCD_WriteData(0x0000);//
	LCD_WriteCmd(0x0013); LCD_WriteData(0x0001);//
	
	LCD_WriteCmd(0x0100); LCD_WriteData(0x0330);//BT AP
	LCD_WriteCmd(0x0101); LCD_WriteData(0x0247);//DC0,DC1,VC
	LCD_WriteCmd(0x0103); LCD_WriteData(0x1000);//VDV
	LCD_WriteCmd(0x0280); LCD_WriteData(0xDE00);//VCM
	LCD_WriteCmd(0x0102); LCD_WriteData(0xD1B0);//VR,VCMR,PSON,PON
	delay_ms(10);
	
	LCD_WriteCmd(0x0001); LCD_WriteData(0x0100);
	LCD_WriteCmd(0x0002); LCD_WriteData(0x0100);
	LCD_WriteCmd(0x0003); LCD_WriteData(0x1030);
	LCD_WriteCmd(0x0009); LCD_WriteData(0x0001);
	LCD_WriteCmd(0x000C); LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0090); LCD_WriteData(0x8000);
	LCD_WriteCmd(0x000F); LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0210); LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0211); LCD_WriteData(0x00EF);
	LCD_WriteCmd(0x0212); LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0213); LCD_WriteData(0x018F);//432=1AF, 400=18F
	LCD_WriteCmd(0x0500); LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0501); LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0502); LCD_WriteData(0x005F);
	LCD_WriteCmd(0x0401); LCD_WriteData(0x0001);
	LCD_WriteCmd(0x0404); LCD_WriteData(0x0000);
	delay_ms(10);
							 
	LCD_WriteCmd(0x0007); LCD_WriteData(0x0100);//BASEE
	delay_ms(10);
	
	LCD_WriteCmd(0x0200); LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0201); LCD_WriteData(0x0000);
	delay_ms(10);
	LCD_WriteCmd(0x0202);
#endif


#ifdef TFTLCD_HX8352C

	//************* Start Initial Sequence **********//	
	//##################################################################
	//Power Voltage Setting
	LCD_WriteCmdData(0x1A,0x02); //BT  0x02
	LCD_WriteCmdData(0x1B,0x88); //VRH  0x88
	LCD_WriteCmdData(0x1C,0x06); //AP[0:2]  //自己添加
	
	//****VCOM offset**///
	LCD_WriteCmdData(0x23,0x00); //SEL_VCM   00
	LCD_WriteCmdData(0x24,0x7f); //VCM    5f
	LCD_WriteCmdData(0x25,0x1f); //VDV   0x15
	LCD_WriteCmdData(0x2D,0x03); //NOW[2:0]=011
	//Power on Setting
	LCD_WriteCmdData(0x18,0x04); //Frame rate 72Hz
	LCD_WriteCmdData(0x19,0x01); //OSC_EN='1', start Osc
	LCD_WriteCmdData(0x01,0x00); //DP_STB='0', out deep sleep
	LCD_WriteCmdData(0x1F,0x88); //STB=0
	delay_ms(5);
	LCD_WriteCmdData(0x1F,0x80); //DK=0
	delay_ms(5);
	LCD_WriteCmdData(0x1F,0x90); //PON=1
	delay_ms(5);
	LCD_WriteCmdData(0x1F,0xD0); //VCOMG=1
	delay_ms(5);
	//262k/65k color selection
	LCD_WriteCmdData(0x17,0x05); //default 0x06 262k color // 0x05 65k color
	//SET PANEL
	LCD_WriteCmdData(0x36,0x13); //REV_P, SM_P, GS_P, BGR_P, SS_P   0x03
	LCD_WriteCmdData(0x29,0x31); //400 lines
	LCD_WriteCmdData(0x71,0x1A); //RTN
	//Gamma 2.2 Setting
	LCD_WriteCmdData(0x40,0x00);
	LCD_WriteCmdData(0x41,0x45);
	LCD_WriteCmdData(0x42,0x45);
	LCD_WriteCmdData(0x43,0x04);
	LCD_WriteCmdData(0x44,0x00);
	LCD_WriteCmdData(0x45,0x08);
	LCD_WriteCmdData(0x46,0x23);
	LCD_WriteCmdData(0x47,0x23);
	LCD_WriteCmdData(0x48,0x77);
	LCD_WriteCmdData(0x49,0x40);
	LCD_WriteCmdData(0x4A,0x04);
	LCD_WriteCmdData(0x4B,0x00);
	LCD_WriteCmdData(0x4C,0x88);
	LCD_WriteCmdData(0x4D,0x88);
	LCD_WriteCmdData(0x4E,0x88);
	//Set GRAM Area
	LCD_WriteCmdData(0x02,0x00);
	LCD_WriteCmdData(0x03,0x00); //Column Start
	LCD_WriteCmdData(0x04,0x00);
	LCD_WriteCmdData(0x05,0xEF); //Column End
	LCD_WriteCmdData(0x06,0x00);
	LCD_WriteCmdData(0x07,0x00); //Row Start
	LCD_WriteCmdData(0x08,0x01);
	LCD_WriteCmdData(0x09,0xAF); //Row End
	//Display ON Setting
	LCD_WriteCmdData(0x28,0x38); //GON=1, DTE=1, D=10
	delay_ms(40);
	LCD_WriteCmdData(0x28,0x3F); //GON=1, DTE=1, D=11  0x3C
	LCD_WriteCmd(0x22);
	 //Start GRAM write
	//##################################################################
	// Power Off Setting
	LCD_WriteCmdData(0x28,0x38); //GON=’1’ DTE=’1’ D[1:0]=’10’
	delay_ms(40);
	LCD_WriteCmdData(0x1F,0x89); // GAS=1, VOMG=0, PON=0, DK=1, XDK=0, DVDH_TRI=0, STB=1
	delay_ms(40);
	LCD_WriteCmdData(0x28,0x20); //GON=’1’ DTE=’0’ D[1:0]=’00’
	delay_ms(40);
	LCD_WriteCmdData(0x19,0x00); //OSC_EN=’0’
	delay_ms(40);
	//M51_WR_REG(U05_LCD_POWER_ON, 0x00); //VCI & IOVCC OFF
	//##################################################################
	// ENTER IDLE mode Setting
	LCD_WriteCmdData(0x18,0x07); //Frame Rate 60Hz
	LCD_WriteCmdData(0x01,0x04); //IDLE='1' , enter IDLE mode
	// EXIT IDLE mode Setting
	LCD_WriteCmdData(0x18,0x04); //Frame rate 72Hz
	LCD_WriteCmdData(0x01,0x00); //IDLE='0', EXIT IDLE mode
	//##################################################################
	// ENTER Partial mode Setting 32line
	LCD_WriteCmdData(0x18,0x04); //Frame rate 72Hz
	LCD_WriteCmdData(0x01,0x01); //PTL='1', Enter Partial mode
	LCD_WriteCmdData(0x0A,0x00); //PSL[15:8]=0x00
	LCD_WriteCmdData(0x0B,0x20); //PSL[7:0]=0x20
	LCD_WriteCmdData(0x0C,0x00); //PEL[15:8]=0x00
	LCD_WriteCmdData(0x0D,0x3F); //PEL[7:0]=0x3F
	// EXIT Partial mode Setting
	LCD_WriteCmdData(0x18,0x04); //Frame rate 72Hz
	LCD_WriteCmdData(0x01,0x00); //PTL='0', EXIT Partial mode
	//##################################################################
	// ENTER Partial + IDLE mode Setting 32line
	LCD_WriteCmdData(0x18,0x07); //Frame Rate 60Hz
	LCD_WriteCmdData(0x01,0x05); // PTL='1' IDLE=’1’, Enter Partial + IDLE mode
	LCD_WriteCmdData(0x0A,0x00); //PSL[15:8]=0x00
	LCD_WriteCmdData(0x0B,0x20); //PSL[7:0]=0x20
	LCD_WriteCmdData(0x0C,0x00); //PEL[15:8]=0x00
	LCD_WriteCmdData(0x0D,0x3F); //PEL[7:0]=0x3F
	// EXIT Partial + IDLE mode Setting
	LCD_WriteCmdData(0x18,0x04); //Frame rate 72Hz
	LCD_WriteCmdData(0x01,0x00); // PTL='0' IDLE=’0’, EXIT Partial + IDLE mode
	//##################################################################
	// Enter Sleep mode Setting
	LCD_WriteCmdData(0x28,0x38); //GON=’1’ DTE=’1’ D[1:0]=’10’
	delay_ms(40);
	LCD_WriteCmdData(0x1F,0x89); // GAS=1, VOMG=0, PON=0, DK=1, XDK=0, DVDH_TRI=0, STB=1
	delay_ms(40);
	LCD_WriteCmdData(0x28,0x20); //GON=’1’ DTE=’0’ D[1:0]=’00’
	delay_ms(40);
	LCD_WriteCmdData(0x19,0x00); //OSC_EN=’0’
	delay_ms(5);
	// Exit Sleep mode Setting
	LCD_WriteCmdData(0x19,0x01); //OSC_EN='1', start Osc
	LCD_WriteCmdData(0x1F,0x88); //STB=0
	delay_ms(5);
	LCD_WriteCmdData(0x1F,0x80); //DK=0
	delay_ms(5);
	LCD_WriteCmdData(0x1F,0x90); //PON=1
	delay_ms(5);
	LCD_WriteCmdData(0x1F,0xD0); //VCOMG=1
	delay_ms(5);
	LCD_WriteCmdData(0x28,0x38); //GON=1, DTE=1, D=10
	delay_ms(40);
	LCD_WriteCmdData(0x28,0x3C); //GON=1, DTE=1, D=11
	//##################################################################
	// Enter Deep Sleep mode Setting
	LCD_WriteCmdData(0x28,0x38); //GON=’1’ DTE=’1’ D[1:0]=’10’
	delay_ms(40);
	LCD_WriteCmdData(0x1F,0x89); // GAS=1, VOMG=0, PON=0, DK=1, XDK=0, DVDH_TRI=0, STB=1
	delay_ms(40);
	LCD_WriteCmdData(0x28,0x20); //GON=’1’ DTE=’0’ D[1:0]=’00’
	delay_ms(40);
	LCD_WriteCmdData(0x19,0x00); //OSC_EN=’0’
	delay_ms(5);
	LCD_WriteCmdData(0x01,0x80); //DP_STB=’1’
	// Exit Deep Sleep mode Setting
	LCD_WriteCmdData(0x01,0x00); //DP_STB='0', out deep sleep
	delay_ms(10);
	LCD_WriteCmdData(0x19,0x01); //OSC_EN='1', start Osc
	LCD_WriteCmdData(0x1F,0x88); //STB=0
	delay_ms(5);
	LCD_WriteCmdData(0x1F,0x80); //DK=0
	delay_ms(5);
	LCD_WriteCmdData(0x1F,0x90); //PON=1
	delay_ms(5);
	LCD_WriteCmdData(0x1F,0xD0); //VCOMG=1
	delay_ms(5);
	LCD_WriteCmdData(0x28,0x38); //GON=1, DTE=1, D=10
	delay_ms(40);
	LCD_WriteCmdData(0x28,0x3C); //GON=1, DTE=1, D=11

	LCD_WriteCmd(0x22);

#endif

#ifdef TFTLCD_ILI9341

	//************* Start Initial Sequence **********//	
	LCD_WriteCmd(0xCB);  
	LCD_WriteData(0x39); 
	LCD_WriteData(0x2C); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x34);  
	LCD_WriteData(0x02);   
	 
	LCD_WriteCmd(0xCF);  
	LCD_WriteData (0x00); 
	LCD_WriteData (0xa2); //c1
	LCD_WriteData (0Xf0);  //30
	 
	LCD_WriteCmd(0xE8);  
	LCD_WriteData (0x84); 	 //85
	LCD_WriteData (0x11); 	 //00
	LCD_WriteData (0x7a); 	 //79
	
	
	LCD_WriteCmd(0xEA);  
	LCD_WriteData (0x66);  //00
	LCD_WriteData (0x00); 
	
	LCD_WriteCmd(0xED);  
	LCD_WriteData (0x55); 	//64
	LCD_WriteData (0x01); 	//03
	LCD_WriteData (0X23); 	//12
	LCD_WriteData (0X01); 	//81
	 
	LCD_WriteCmd(0xF7);  
	LCD_WriteData (0x20); 	//20
	
	
	LCD_WriteCmd(0xC0);    //Power control 
	LCD_WriteData (0x1c);   //VRH[5:0] 	//1c
	 
	LCD_WriteCmd(0xC1);    //Power control 
	LCD_WriteData (0x13);   //SAP[2:0];BT[3:0] 	//13
	 
	LCD_WriteCmd(0xC5);    //VCM control 
	LCD_WriteData (0x23); 
	LCD_WriteData (0x3F); 
	 
	LCD_WriteCmd(0xC7);    //VCM control2 
	LCD_WriteData(0xA5); 
	
	LCD_WriteCmd(0xB1);  
	LCD_WriteData (0x00); 
	LCD_WriteData (0x17); 
	
	LCD_WriteCmd(0x3A);  
	LCD_WriteData (0x55); 
	
	LCD_WriteCmd(0xB6);  
	LCD_WriteData (0x0A);
	LCD_WriteData (0xa2);  //a2
	LCD_WriteData (0x27);
	LCD_WriteData (0x00); 
	 
	LCD_WriteCmd(0x36);    // Memory Access Control 
	LCD_WriteData (0x08); 	  //08	BGR
	 
	LCD_WriteCmd(0xF2);    // 3Gamma Function Disable 
	LCD_WriteData (0x02); 	//00
	 
	LCD_WriteCmd(0x26);    //Gamma curve selected 
	LCD_WriteData (0x01); 
	 
	LCD_WriteCmd(0xE0);    //Set Gamma 
	LCD_WriteData (0x0F); 
	LCD_WriteData (0x14); 
	LCD_WriteData (0x13); 
	LCD_WriteData (0x0C); 
	LCD_WriteData (0x0E); 
	LCD_WriteData (0x05); 
	LCD_WriteData (0x45); 
	LCD_WriteData (0x85); 
	LCD_WriteData (0x36); 
	LCD_WriteData (0x09); 
	LCD_WriteData (0x14); 
	LCD_WriteData (0x05); 
	LCD_WriteData (0x09); 
	LCD_WriteData (0x03); 
	LCD_WriteData (0x00); 
	 
	LCD_WriteCmd(0XE1);    //Set Gamma 
	LCD_WriteData (0x00); 
	LCD_WriteData (0x24); 
	LCD_WriteData (0x26); 
	LCD_WriteData (0x03); 
	LCD_WriteData (0x0F); 
	LCD_WriteData (0x04); 
	LCD_WriteData (0x3F); 
	LCD_WriteData (0x14); 
	LCD_WriteData (0x52); 
	LCD_WriteData (0x04); 
	LCD_WriteData (0x10); 
	LCD_WriteData (0x0E); 
	LCD_WriteData (0x38); 
	LCD_WriteData (0x39); 
	LCD_WriteData (0x0F); 

	LCD_WriteCmd(0x2A );
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0xEF);

	LCD_WriteCmd(0x2B );
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x01);
	LCD_WriteData(0x3F);
	 
	LCD_WriteCmd(0x11);    //Exit Sleep 
	delay_ms(120);

	LCD_WriteCmd(0x29);    //Display on

#endif

#ifdef TFTLCD_ILI9327
	//************* Start Initial Sequence **********// 

	LCD_WriteCmd(0xE9); 
	LCD_WriteData(0x20); 
	
	LCD_WriteCmd(0x11); //Exit Sleep 
	delay_ms(100); 
	
	LCD_WriteCmd(0x3A);
	LCD_WriteData(0x55);
	
	LCD_WriteCmd(0xD1); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x65); //调试此值改善水纹
	LCD_WriteData(0x1F); 
	
	LCD_WriteCmd(0xD0); 
	LCD_WriteData(0x07); 
	LCD_WriteData(0x07); 
	LCD_WriteData(0x80); 
	
	LCD_WriteCmd(0x36); 
	LCD_WriteData(0x48); 
	
	LCD_WriteCmd(0xC1); 
	LCD_WriteData(0x10); 
	LCD_WriteData(0x10); 
	LCD_WriteData(0x02); 
	LCD_WriteData(0x02); 
	
	LCD_WriteCmd(0xC0); //Set Default Gamma 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x35); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x02); 
	
	LCD_WriteCmd(0xC5); //Set frame rate 
	LCD_WriteData(0x01); 
	
	LCD_WriteCmd(0xD2); //power setting 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x22); 
	
	LCD_WriteCmd(0xC8); //Set Gamma 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x52); 
	LCD_WriteData(0x37); 
	LCD_WriteData(0x10); 
	LCD_WriteData(0x0d); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x04); 
	LCD_WriteData(0x51); 
	LCD_WriteData(0x77); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x0d); 
	LCD_WriteData(0x08); 
	LCD_WriteData(0x80); 
	LCD_WriteData(0x00); 
	
	LCD_WriteCmd(0x29); //display on

#endif


#ifdef TFTLCD_ILI9325
	LCD_WriteCmd(0x00E3);LCD_WriteData(0x3008); // Set internal timing
	LCD_WriteCmd(0x00E7);LCD_WriteData(0x0012); // Set internal timing
	LCD_WriteCmd(0x00EF);LCD_WriteData(0x1231); // Set internal timing
	LCD_WriteCmd(0x0001);LCD_WriteData(0x0100); // set SS and SM bit
	LCD_WriteCmd(0x0002);LCD_WriteData(0x0700); // set 1 line inversion
	LCD_WriteCmd(0x0003);LCD_WriteData(0x1030); // set GRAM write direction and BGR=1.
	LCD_WriteCmd(0x0004);LCD_WriteData(0x0000); // Resize register
	LCD_WriteCmd(0x0008);LCD_WriteData(0x0207); // set the back porch and front porch
	LCD_WriteCmd(0x0009);LCD_WriteData(0x0000); // set non-display area refresh cycle ISC[3:0]
	LCD_WriteCmd(0x000A);LCD_WriteData(0x0000); // FMARK function
	LCD_WriteCmd(0x000C);LCD_WriteData(0x0000); // RGB interface setting
	LCD_WriteCmd(0x000D);LCD_WriteData(0x0000); // Frame marker Position
	LCD_WriteCmd(0x000F);LCD_WriteData(0x0000); // RGB interface polarity
	//*************Power On sequence ****************//
	LCD_WriteCmd(0x0010);LCD_WriteData(0x0000); // SAP);LCD_WriteData(BT[3:0]);LCD_WriteData(AP);LCD_WriteData(DSTB);LCD_WriteData(SLP);LCD_WriteData(STB
	LCD_WriteCmd(0x0011);LCD_WriteData(0x0007); // DC1[2:0]);LCD_WriteData(DC0[2:0]);LCD_WriteData(VC[2:0]
	LCD_WriteCmd(0x0012);LCD_WriteData(0x0000); // VREG1OUT voltage
	LCD_WriteCmd(0x0013);LCD_WriteData(0x0000); // VDV[4:0] for VCOM amplitude
	delay_ms(20); // Dis-charge capacitor power voltage
	LCD_WriteCmd(0x0010);LCD_WriteData(0x1490); // SAP);LCD_WriteData(BT[3:0]);LCD_WriteData(AP);LCD_WriteData(DSTB);LCD_WriteData(SLP);LCD_WriteData(STB
	LCD_WriteCmd(0x0011);LCD_WriteData(0x0227); // DC1[2:0]);LCD_WriteData(DC0[2:0]);LCD_WriteData(VC[2:0]
	delay_ms(50); // delay_ms 50ms
	LCD_WriteCmd(0x0012);LCD_WriteData(0x001C); // Internal reference voltage= Vci;
	delay_ms(50); // delay_ms 50ms
	LCD_WriteCmd(0x0013);LCD_WriteData(0x1A00); // Set VDV[4:0] for VCOM amplitude
	LCD_WriteCmd(0x0029);LCD_WriteData(0x0025); // Set VCM[5:0] for VCOMH
	LCD_WriteCmd(0x002B);LCD_WriteData(0x000C); // Set Frame Rate
	delay_ms(50); // delay_ms 50ms
	LCD_WriteCmd(0x0020);LCD_WriteData(0x0000); // GRAM horizontal Address
	LCD_WriteCmd(0x0021);LCD_WriteData(0x0000); // GRAM Vertical Address
	// ----------- Adjust the Gamma Curve ----------//
	LCD_WriteCmd(0x0030);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0031);LCD_WriteData(0x0506);
	LCD_WriteCmd(0x0032);LCD_WriteData(0x0104);
	LCD_WriteCmd(0x0035);LCD_WriteData(0x0207);
	LCD_WriteCmd(0x0036);LCD_WriteData(0x000F);
	LCD_WriteCmd(0x0037);LCD_WriteData(0x0306);
	LCD_WriteCmd(0x0038);LCD_WriteData(0x0102);
	LCD_WriteCmd(0x0039);LCD_WriteData(0x0707);
	LCD_WriteCmd(0x003C);LCD_WriteData(0x0702);
	LCD_WriteCmd(0x003D);LCD_WriteData(0x1604);
	//------------------ Set GRAM area ---------------//
	LCD_WriteCmd(0x0050);LCD_WriteData(0x0000); // Horizontal GRAM Start Address
	LCD_WriteCmd(0x0051);LCD_WriteData(0x00EF); // Horizontal GRAM End Address
	LCD_WriteCmd(0x0052);LCD_WriteData(0x0000); // Vertical GRAM Start Address
	LCD_WriteCmd(0x0053);LCD_WriteData(0x013F); // Vertical GRAM Start Address
	LCD_WriteCmd(0x0060);LCD_WriteData(0x2700); // Gate Scan Line
	LCD_WriteCmd(0x0061);LCD_WriteData(0x0001); // NDL,VLE);LCD_WriteData(REV
	LCD_WriteCmd(0x006A);LCD_WriteData(0x0000); // set scrolling line
	//-------------- Partial Display Control ---------//
	LCD_WriteCmd(0x0080);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0081);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0082);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0083);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0084);LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0085);LCD_WriteData(0x0000);
	//------------ Panel Control -------------------//
	LCD_WriteCmd(0x0090);LCD_WriteData(0x0010);
	LCD_WriteCmd(0x0092);LCD_WriteData(0x0600);
	LCD_WriteCmd(0x0007);LCD_WriteData(0x0133); // 262K color and display ON

	LCD_WriteCmd(0x0010);LCD_WriteData(0x12B0); // SAP);LCD_WriteData(BT[3:0]);LCD_WriteData(AP);LCD_WriteData(DSTB);LCD_WriteData(SLP
	LCD_WriteCmd(0x0011);LCD_WriteData(0x0007);
	LCD_WriteCmd(0x0017);LCD_WriteData(0x0001); // DC1[2:0]);LCD_WriteData(DC0[2:0]);LCD_WriteData(VC[2:0]
	LCD_WriteCmd(0x0012);LCD_WriteData(0x01BD); // VREG1OUT voltage
	LCD_WriteCmd(0x0013);LCD_WriteData(0x1700); // VDV[4:0] for VCOM amplitude
	delay_ms(20); // Dis-charge capacitor power voltage
	LCD_WriteCmd(0x0010);LCD_WriteData(0x1490); // SAP);LCD_WriteData(BT[3:0]);LCD_WriteData(AP);LCD_WriteData(DSTB);LCD_WriteData(SLP);LCD_WriteData(STB
	LCD_WriteCmd(0x0011);LCD_WriteData(0x0227); // DC1[2:0]);LCD_WriteData(DC0[2:0]);LCD_WriteData(VC[2:0]
	delay_ms(50); // delay_ms 50ms
	LCD_WriteCmd(0x0012);LCD_WriteData(0x001C); //Inernal reference voltage =Vci;
	delay_ms(50); // delay_ms 50ms
	LCD_WriteCmd(0x0013);LCD_WriteData(0x1A00); // VDV[4:0] for VCOM amplitude
	LCD_WriteCmd(0x0029);LCD_WriteData(0x0025); // VCM[5:0] for VCOMH
	delay_ms(50); // delay_ms 50ms
	//LCD_WriteCmd(0x0007);LCD_WriteData(0x0133); // 262K color and display ON

	LCD_WriteCmd(0x0007);LCD_WriteData(0x0133);
	delay_ms(120);
	LCD_WriteCmd(0x0022);

#endif

#ifdef TFTLCD_ILI9486
/*
	LCD_WriteCmd (0xC0); //电源控制1
    LCD_WriteData(0x13);
    LCD_WriteData(0x10);
delay_ms(50);
	LCD_WriteCmd (0xC1); //电源控制2
    LCD_WriteData(0x43);
    LCD_WriteData(0x00);	   
delay_ms(50);	
	LCD_WriteCmd (0xC2); //电源控制3
    LCD_WriteData(0x22);
delay_ms(50);	
	LCD_WriteCmd (0xC5); //VCOM Control
    LCD_WriteData(0x00);
    LCD_WriteData(0x4D);
    LCD_WriteData(0x80); 
delay_ms(50);	
	LCD_WriteCmd (0xB1); //Frame Rate Control
    LCD_WriteData(0xC0);
    LCD_WriteData(0x11);
delay_ms(50);
	LCD_WriteCmd (0xB4); //Display Inversion Control
    LCD_WriteData(0x01);
delay_ms(50);	
	LCD_WriteCmd (0xB6); //Display Function Control 
    LCD_WriteData(0x00);
    LCD_WriteData(0x42);
    LCD_WriteData(0x3B); 
delay_ms(50);	
	LCD_WriteCmd(0x36);  
	LCD_WriteData(0x00);
	LCD_WriteCmd(0x3A);
	LCD_WriteData(0x55);
delay_ms(50);
	LCD_WriteCmd (0x11); //关闭睡眠模式
delay_ms(50);
	LCD_WriteCmd (0x29); //Display ON
delay_ms(50);
*/
/*
	LCD_WriteCmd(0x11); 
	delay_ms(20); 
	LCD_WriteCmd(0xD0); 
	LCD_WriteData(0x07); 
	LCD_WriteData(0x42); 
	LCD_WriteData(0x18); 

	LCD_WriteCmd(0xD1); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x07);//07 
	LCD_WriteData(0x10); 

	LCD_WriteCmd(0xD2); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x02); 

	LCD_WriteCmd(0xC0); 
	LCD_WriteData(0x10); 
	LCD_WriteData(0x3B); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x02); 
	LCD_WriteData(0x11); 

	LCD_WriteCmd(0xC5); 
	LCD_WriteData(0x03); 

	LCD_WriteCmd(0xC8); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x32); 
	LCD_WriteData(0x36); 
	LCD_WriteData(0x45); 
	LCD_WriteData(0x06); 
	LCD_WriteData(0x16); 
	LCD_WriteData(0x37); 
	LCD_WriteData(0x75); 
	LCD_WriteData(0x77); 
	LCD_WriteData(0x54); 
	LCD_WriteData(0x0C); 
	LCD_WriteData(0x00); 

	LCD_WriteCmd(0x36); 
	LCD_WriteData(0x00); 

	LCD_WriteCmd(0x3A); 
	LCD_WriteData(0x55); 

	LCD_WriteCmd(0x2A); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0x3F); 

	LCD_WriteCmd(0x2B); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x00); 
	LCD_WriteData(0x01); 
	LCD_WriteData(0xdf); 
	delay_ms(120); 
	LCD_WriteCmd(0x29);
*/

	LCD_WriteCmd(0xB0);
	LCD_WriteData(0x00);
	LCD_WriteCmd(0x11);
	delay_ms(20);

	LCD_WriteCmd(0xB3);
	LCD_WriteData(0x02);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);

	LCD_WriteCmd(0xC0);
	LCD_WriteData(0x10);//13
	LCD_WriteData(0x3B);//480
	LCD_WriteData(0x00);
	LCD_WriteData(0x02);
	LCD_WriteData(0x00);
	LCD_WriteData(0x01);
	LCD_WriteData(0x00);//NW
	LCD_WriteData(0x43);

	LCD_WriteCmd(0xC1);
	LCD_WriteData(0x10);//LCD_WriteData(0x08);
	LCD_WriteData(0x10);//LCD_WriteData(0x16);//CLOCK
	LCD_WriteData(0x08);
	LCD_WriteData(0x08);

	LCD_WriteCmd(0xC4);
	LCD_WriteData(0x11);
	LCD_WriteData(0x07);
	LCD_WriteData(0x03);
	LCD_WriteData(0x03);

	LCD_WriteCmd(0xC6);
	LCD_WriteData(0x00);

	LCD_WriteCmd(0xC8);//GAMMA
	LCD_WriteData(0x03);
	LCD_WriteData(0x03);
	LCD_WriteData(0x13);
	LCD_WriteData(0x5C);
	LCD_WriteData(0x03);
	LCD_WriteData(0x07);
	LCD_WriteData(0x14);
	LCD_WriteData(0x08);
	LCD_WriteData(0x00);
	LCD_WriteData(0x21);
	LCD_WriteData(0x08);
	LCD_WriteData(0x14);
	LCD_WriteData(0x07);
	LCD_WriteData(0x53);
	LCD_WriteData(0x0C);
	LCD_WriteData(0x13);
	LCD_WriteData(0x03);
	LCD_WriteData(0x03);
	LCD_WriteData(0x21);
	LCD_WriteData(0x00);

	LCD_WriteCmd(0x35);
	LCD_WriteData(0x00);

	LCD_WriteCmd(0x36);  
	LCD_WriteData(0x00);

	LCD_WriteCmd(0x3A);
	LCD_WriteData(0x55);

	LCD_WriteCmd(0x44);
	LCD_WriteData(0x00);
	LCD_WriteData(0x01);

	LCD_WriteCmd(0xB6);
	LCD_WriteData(0x00);
	LCD_WriteData(0x22);//0 GS SS SM ISC[3:0];其中GS SS控制显示方向，同时修改R36
	LCD_WriteData(0x3B);

	LCD_WriteCmd(0xD0);
	LCD_WriteData(0x07);
	LCD_WriteData(0x07);//VCI1
	LCD_WriteData(0x1D);//VRH

	LCD_WriteCmd(0xD1);
	LCD_WriteData(0x00);
	LCD_WriteData(0x03);//VCM
	LCD_WriteData(0x00);//VDV

	LCD_WriteCmd(0xD2);
	LCD_WriteData(0x03);
	LCD_WriteData(0x14);
	LCD_WriteData(0x04);

	LCD_WriteCmd(0x29);
	delay_ms(20);

	LCD_WriteCmd(0xB4);
	LCD_WriteData(0x00);
	delay_ms(20);
	LCD_WriteCmd(0x2C);

#endif

#ifdef TFTLCD_ST7793	
	for(i=500; i>0; i--);
	LCD_WriteCmd(0x0001);
	LCD_WriteData(0x0100);
	LCD_WriteCmd(0x0003);
	LCD_WriteData(0x1030); //16bit 565 
	
	LCD_WriteCmd(0x0008);
	LCD_WriteData(0x0808);
	LCD_WriteCmd(0x0090);
	LCD_WriteData(0x8000);
	LCD_WriteCmd(0x0400);
	LCD_WriteData(0x6200);
	LCD_WriteCmd(0x0401);
	LCD_WriteData(0x0001);

	LCD_WriteCmd(0x00ff);
	LCD_WriteData(0x0001);
	LCD_WriteCmd(0x0102);
	LCD_WriteData(0x01b0);
	LCD_WriteCmd(0x0710);
	LCD_WriteData(0x0016);
	LCD_WriteCmd(0x0712);
	LCD_WriteData(0x000f);
	LCD_WriteCmd(0x0752);
	LCD_WriteData(0x002f);
	LCD_WriteCmd(0x0724);
	LCD_WriteData(0x001a);
	LCD_WriteCmd(0x0754);	
	LCD_WriteData(0x0018);
	for(i=500; i>0; i--);
    
    /* Gamma */
	LCD_WriteCmd(0x0380);
	LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0381);
	LCD_WriteData(0x5F10);//5f10
	LCD_WriteCmd(0x0382);
	LCD_WriteData(0x0B02);//0b02
	LCD_WriteCmd(0x0383);
	LCD_WriteData(0x0614);
	LCD_WriteCmd(0x0384);
	LCD_WriteData(0x0111);
	LCD_WriteCmd(0x0385);
	LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0386);
	LCD_WriteData(0xA90B);//a90b
	LCD_WriteCmd(0x0387);
	LCD_WriteData(0x0606);//0606
	LCD_WriteCmd(0x0388);
	LCD_WriteData(0x0612);
	LCD_WriteCmd(0x0389);
	LCD_WriteData(0x0111);	  


    /* Gamma Setting */
    /* Setting */
	LCD_WriteCmd(0x0702);
	LCD_WriteData(0x003b);//003b
	LCD_WriteCmd(0x00ff);
	LCD_WriteData(0x0000);


    /* Vcom Setting */
	LCD_WriteCmd(0x0007);
	LCD_WriteData(0x0100);
	for(i=500; i>0; i--); 
	LCD_WriteCmd(0x0200);
	LCD_WriteData(0x0000);
	LCD_WriteCmd(0x0201);                  
	LCD_WriteData(0x0000);	
#endif

#ifdef TFTLCD_SSD1963
#if 0
	LCD_WriteCmd(0xE2);		//Set PLL with OSC = 10MHz (hardware),	Multiplier N = 35, 250MHz < VCO < 800MHz = OSC*(N+1), VCO = 360MHz
	LCD_WriteData(0x23);		//参数1 
	LCD_WriteData(0x02);		//参数2 Divider M = 2, PLL = 360/(M+1) = 120MHz
	LCD_WriteData(0x04);		//参数3 Validate M and N values   
	delay_us(100);
	LCD_WriteCmd(0xE0);		// Start PLL command
	LCD_WriteData(0x01);		// enable PLL
	delay_ms(10);
	LCD_WriteCmd(0xE0);		// Start PLL command again
	LCD_WriteData(0x03);		// now, use PLL output as system clock	
	delay_ms(12);  
	LCD_WriteCmd(0x01);		//软复位
	delay_ms(10);
	
	LCD_WriteCmd(0x21);		//设置颜色取反	
	
	LCD_WriteCmd(0xE6);		//设置像素频率
	LCD_WriteData(0x03);
	LCD_WriteData(0xFF);
	LCD_WriteData(0xFF);
	
	LCD_WriteCmd(0xB0);		//设置LCD模式
	LCD_WriteData(0x20);		//24位模式
	LCD_WriteData(0x00);		//TFT 模式 

	LCD_WriteData((SSD_HOR_RESOLUTION-1)>>8);//设置LCD水平像素
	LCD_WriteData(SSD_HOR_RESOLUTION-1);		 
	LCD_WriteData((SSD_VER_RESOLUTION-1)>>8);//设置LCD垂直像素
	LCD_WriteData(SSD_VER_RESOLUTION-1);		 
	LCD_WriteData(0x00);		//RGB序列 
	
	LCD_WriteCmd(0xB4);		//Set horizontal period
	LCD_WriteData((SSD_HT-1)>>8);
	LCD_WriteData(SSD_HT-1);
	LCD_WriteData((SSD_HPS-1)>>8);
	LCD_WriteData(SSD_HPS-1);
	LCD_WriteData(SSD_HOR_PULSE_WIDTH-1);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteCmd(0xB6);		//Set vertical period
	LCD_WriteData((SSD_VT-1)>>8);
	LCD_WriteData(SSD_VT-1);
	LCD_WriteData((SSD_VSP-1)>>8);
	LCD_WriteData(SSD_VSP-1);
	LCD_WriteData(SSD_VER_FRONT_PORCH-1);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	
	LCD_WriteCmd(0xF0);	//设置SSD1963与CPU接口为16bit  
	LCD_WriteData(0x03);	//16-bit(565 format) data for 16bpp 

	LCD_WriteCmd(0x29);	//开启显示
	//设置PWM输出  背光通过占空比可调 
	LCD_WriteCmd(0xD0);	//设置自动白平衡DBC
	LCD_WriteData(0x00);	//disable

	LCD_WriteCmd(0xBE);	//配置PWM输出
	LCD_WriteData(0x05);	//1设置PWM频率
	LCD_WriteData(0xFF);	//2设置PWM占空比
	LCD_WriteData(0x01);	//3设置C
	LCD_WriteData(0xFF);	//4设置D
	LCD_WriteData(0x00);	//5设置E 
	
	LCD_WriteCmd(0xB8);	//设置GPIO配置
	LCD_WriteData(0x0F);	//4个IO口设置成输出
	LCD_WriteData(0x01);	//GPIO使用正常的IO功能 
	LCD_WriteCmd(0xBA);
	LCD_WriteData(0X01);	//GPIO[1:0]=01,控制LCD方向
#endif
#if 1
	LCD_WriteCmd(0xE2);		//Set PLL with OSC = 10MHz (hardware),	Multiplier N = 35, 250MHz < VCO < 800MHz = OSC*(N+1), VCO = 360MHz
	LCD_WriteData(0x2C);		//参数1 0x23--
	LCD_WriteData(0x02);		//参数2 Divider M = 2, PLL = 360/(M+1) = 120MHz
	LCD_WriteData(0x04);		//参数3 Validate M and N values   
	delay_us(100);
	LCD_WriteCmd(0xE0);		// Start PLL command
	LCD_WriteData(0x01);		// enable PLL
	delay_ms(10);
	LCD_WriteCmd(0xE0);		// Start PLL command again
	LCD_WriteData(0x03);		// now, use PLL output as system clock	
	delay_ms(12);  
	LCD_WriteCmd(0x01);		//软复位
	delay_ms(10);
	
//	LCD_WriteCmd(0x21);		//设置颜色取反	
	
	LCD_WriteCmd(0xE6);		//设置像素频率
	LCD_WriteData(0x03);
	LCD_WriteData(0xFF);
	LCD_WriteData(0xFF);
	
	LCD_WriteCmd(0xB0);		//设置LCD模式
	LCD_WriteData(0x20);		//24位模式
	LCD_WriteData(0x00);		//TFT 模式 

	LCD_WriteData((SSD_HOR_RESOLUTION-1)>>8);//设置LCD水平像素
	LCD_WriteData(SSD_HOR_RESOLUTION-1);		 
	LCD_WriteData((SSD_VER_RESOLUTION-1)>>8);//设置LCD垂直像素
	LCD_WriteData(SSD_VER_RESOLUTION-1);		 
	LCD_WriteData(0x00);		//RGB序列 
	
	LCD_WriteCmd(0xB4);		//Set horizontal period
	LCD_WriteData((SSD_HT-1)>>8);
	LCD_WriteData(SSD_HT-1);
	LCD_WriteData((SSD_HPS-1)>>8);
	LCD_WriteData(SSD_HPS-1);
	LCD_WriteData(SSD_HOR_PULSE_WIDTH-1);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteCmd(0xB6);		//Set vertical period
	LCD_WriteData((SSD_VT-1)>>8);
	LCD_WriteData(SSD_VT-1);
	LCD_WriteData((SSD_VSP-1)>>8);
	LCD_WriteData(SSD_VSP-1);
	LCD_WriteData(SSD_VER_FRONT_PORCH-1);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	
	LCD_WriteCmd(0xF0);	//设置SSD1963与CPU接口为16bit  
	LCD_WriteData(0x03);	//16-bit(565 format) data for 16bpp 

	LCD_WriteCmd(0x29);	//开启显示
	//设置PWM输出  背光通过占空比可调 
	LCD_WriteCmd(0xD0);	//设置自动白平衡DBC
	LCD_WriteData(0x0D);	//disable

	LCD_WriteCmd(0xBE);	//配置PWM输出
	LCD_WriteData(0x05);	//1设置PWM频率
	LCD_WriteData(0xFF);	//2设置PWM占空比
	LCD_WriteData(0x01);	//3设置C
	LCD_WriteData(0xFF);	//4设置D
	LCD_WriteData(0x00);	//5设置E 
	
	LCD_WriteCmd(0xB8);	//设置GPIO配置
	LCD_WriteData(0x0F);	//4个IO口设置成输出
	LCD_WriteData(0x01);	//GPIO使用正常的IO功能 
	LCD_WriteCmd(0xBA);
	LCD_WriteData(0X01);	//GPIO[1:0]=01,控制LCD方向
	
	LCD_WriteCmd(0xBC);	
	LCD_WriteData(0XC4);//亮度值	
	LCD_WriteData(0X95);//对比度值	
	LCD_WriteData(0XB1);//饱和度值	
	LCD_WriteData(0X01);	
	delay_ms(10);	 
#endif
	LCD_SSD_BackLightSet(100);//背光设置为最亮	
#endif

#ifdef TFTLCD_NT35510
	  LCD_WriteCmdData(0xF000, 0x55);
		LCD_WriteCmdData(0xF001, 0xAA);
		LCD_WriteCmdData(0xF002, 0x52);
		LCD_WriteCmdData(0xF003, 0x08);
		LCD_WriteCmdData(0xF004, 0x01);
		LCD_WriteCmdData(0xB000, 0x0D);
		LCD_WriteCmdData(0xB001, 0x0D);
		LCD_WriteCmdData(0xB002, 0x0D);
		LCD_WriteCmdData(0xB600, 0x34);
		LCD_WriteCmdData(0xB601, 0x34);
		LCD_WriteCmdData(0xB602, 0x34);
		LCD_WriteCmdData(0xB100, 0x0D);
		LCD_WriteCmdData(0xB101, 0x0D);
		LCD_WriteCmdData(0xB102, 0x0D);
		LCD_WriteCmdData(0xB700, 0x34);
		LCD_WriteCmdData(0xB701, 0x34);
		LCD_WriteCmdData(0xB702, 0x34);
		LCD_WriteCmdData(0xB200, 0x00);
		LCD_WriteCmdData(0xB201, 0x00);
		LCD_WriteCmdData(0xB202, 0x00);
		LCD_WriteCmdData(0xB800, 0x24);
		LCD_WriteCmdData(0xB801, 0x24);
		LCD_WriteCmdData(0xB802, 0x24);
		LCD_WriteCmdData(0xBF00, 0x01);
		LCD_WriteCmdData(0xB300, 0x0F);
		LCD_WriteCmdData(0xB301, 0x0F);
		LCD_WriteCmdData(0xB302, 0x0F);
		LCD_WriteCmdData(0xB900, 0x34);
		LCD_WriteCmdData(0xB901, 0x34);
		LCD_WriteCmdData(0xB902, 0x34);
		LCD_WriteCmdData(0xB500, 0x08);
		LCD_WriteCmdData(0xB501, 0x08);
		LCD_WriteCmdData(0xB502, 0x08);
		LCD_WriteCmdData(0xC200, 0x03);
		LCD_WriteCmdData(0xBA00, 0x24);
		LCD_WriteCmdData(0xBA01, 0x24);
		LCD_WriteCmdData(0xBA02, 0x24);
		LCD_WriteCmdData(0xBC00, 0x00);
		LCD_WriteCmdData(0xBC01, 0x78);
		LCD_WriteCmdData(0xBC02, 0x00);
		LCD_WriteCmdData(0xBD00, 0x00);
		LCD_WriteCmdData(0xBD01, 0x78);
		LCD_WriteCmdData(0xBD02, 0x00);
		LCD_WriteCmdData(0xBE00, 0x00);
		LCD_WriteCmdData(0xBE01, 0x64);
		LCD_WriteCmdData(0xD100, 0x00);
		LCD_WriteCmdData(0xD101, 0x33);
		LCD_WriteCmdData(0xD102, 0x00);
		LCD_WriteCmdData(0xD103, 0x34);
		LCD_WriteCmdData(0xD104, 0x00);
		LCD_WriteCmdData(0xD105, 0x3A);
		LCD_WriteCmdData(0xD106, 0x00);
		LCD_WriteCmdData(0xD107, 0x4A);
		LCD_WriteCmdData(0xD108, 0x00);
		LCD_WriteCmdData(0xD109, 0x5C);
		LCD_WriteCmdData(0xD10A, 0x00);
		LCD_WriteCmdData(0xD10B, 0x81);
		LCD_WriteCmdData(0xD10C, 0x00);
		LCD_WriteCmdData(0xD10D, 0xA6);
		LCD_WriteCmdData(0xD10E, 0x00);
		LCD_WriteCmdData(0xD10F, 0xE5);
		LCD_WriteCmdData(0xD110, 0x01);
		LCD_WriteCmdData(0xD111, 0x13);
		LCD_WriteCmdData(0xD112, 0x01);
		LCD_WriteCmdData(0xD113, 0x54);
		LCD_WriteCmdData(0xD114, 0x01);
		LCD_WriteCmdData(0xD115, 0x82);
		LCD_WriteCmdData(0xD116, 0x01);
		LCD_WriteCmdData(0xD117, 0xCA);
		LCD_WriteCmdData(0xD118, 0x02);
		LCD_WriteCmdData(0xD119, 0x00);
		LCD_WriteCmdData(0xD11A, 0x02);
		LCD_WriteCmdData(0xD11B, 0x01);
		LCD_WriteCmdData(0xD11C, 0x02);
		LCD_WriteCmdData(0xD11D, 0x34);
		LCD_WriteCmdData(0xD11E, 0x02);
		LCD_WriteCmdData(0xD11F, 0x67);
		LCD_WriteCmdData(0xD120, 0x02);
		LCD_WriteCmdData(0xD121, 0x84);
		LCD_WriteCmdData(0xD122, 0x02);
		LCD_WriteCmdData(0xD123, 0xA4);
		LCD_WriteCmdData(0xD124, 0x02);
		LCD_WriteCmdData(0xD125, 0xB7);
		LCD_WriteCmdData(0xD126, 0x02);
		LCD_WriteCmdData(0xD127, 0xCF);
		LCD_WriteCmdData(0xD128, 0x02);
		LCD_WriteCmdData(0xD129, 0xDE);
		LCD_WriteCmdData(0xD12A, 0x02);
		LCD_WriteCmdData(0xD12B, 0xF2);
		LCD_WriteCmdData(0xD12C, 0x02);
		LCD_WriteCmdData(0xD12D, 0xFE);
		LCD_WriteCmdData(0xD12E, 0x03);
		LCD_WriteCmdData(0xD12F, 0x10);
		LCD_WriteCmdData(0xD130, 0x03);
		LCD_WriteCmdData(0xD131, 0x33);
		LCD_WriteCmdData(0xD132, 0x03);
		LCD_WriteCmdData(0xD133, 0x6D);
		LCD_WriteCmdData(0xD200, 0x00);
		LCD_WriteCmdData(0xD201, 0x33);
		LCD_WriteCmdData(0xD202, 0x00);
		LCD_WriteCmdData(0xD203, 0x34);
		LCD_WriteCmdData(0xD204, 0x00);
		LCD_WriteCmdData(0xD205, 0x3A);
		LCD_WriteCmdData(0xD206, 0x00);
		LCD_WriteCmdData(0xD207, 0x4A);
		LCD_WriteCmdData(0xD208, 0x00);
		LCD_WriteCmdData(0xD209, 0x5C);
		LCD_WriteCmdData(0xD20A, 0x00);
		LCD_WriteCmdData(0xD20B, 0x81);
		LCD_WriteCmdData(0xD20C, 0x00);
		LCD_WriteCmdData(0xD20D, 0xA6);
		LCD_WriteCmdData(0xD20E, 0x00);
		LCD_WriteCmdData(0xD20F, 0xE5);
		LCD_WriteCmdData(0xD210, 0x01);
		LCD_WriteCmdData(0xD211, 0x13);
		LCD_WriteCmdData(0xD212, 0x01);
		LCD_WriteCmdData(0xD213, 0x54);
		LCD_WriteCmdData(0xD214, 0x01);
		LCD_WriteCmdData(0xD215, 0x82);
		LCD_WriteCmdData(0xD216, 0x01);
		LCD_WriteCmdData(0xD217, 0xCA);
		LCD_WriteCmdData(0xD218, 0x02);
		LCD_WriteCmdData(0xD219, 0x00);
		LCD_WriteCmdData(0xD21A, 0x02);
		LCD_WriteCmdData(0xD21B, 0x01);
		LCD_WriteCmdData(0xD21C, 0x02);
		LCD_WriteCmdData(0xD21D, 0x34);
		LCD_WriteCmdData(0xD21E, 0x02);
		LCD_WriteCmdData(0xD21F, 0x67);
		LCD_WriteCmdData(0xD220, 0x02);
		LCD_WriteCmdData(0xD221, 0x84);
		LCD_WriteCmdData(0xD222, 0x02);
		LCD_WriteCmdData(0xD223, 0xA4);
		LCD_WriteCmdData(0xD224, 0x02);
		LCD_WriteCmdData(0xD225, 0xB7);
		LCD_WriteCmdData(0xD226, 0x02);
		LCD_WriteCmdData(0xD227, 0xCF);
		LCD_WriteCmdData(0xD228, 0x02);
		LCD_WriteCmdData(0xD229, 0xDE);
		LCD_WriteCmdData(0xD22A, 0x02);
		LCD_WriteCmdData(0xD22B, 0xF2);
		LCD_WriteCmdData(0xD22C, 0x02);
		LCD_WriteCmdData(0xD22D, 0xFE);
		LCD_WriteCmdData(0xD22E, 0x03);
		LCD_WriteCmdData(0xD22F, 0x10);
		LCD_WriteCmdData(0xD230, 0x03);
		LCD_WriteCmdData(0xD231, 0x33);
		LCD_WriteCmdData(0xD232, 0x03);
		LCD_WriteCmdData(0xD233, 0x6D);
		LCD_WriteCmdData(0xD300, 0x00);
		LCD_WriteCmdData(0xD301, 0x33);
		LCD_WriteCmdData(0xD302, 0x00);
		LCD_WriteCmdData(0xD303, 0x34);
		LCD_WriteCmdData(0xD304, 0x00);
		LCD_WriteCmdData(0xD305, 0x3A);
		LCD_WriteCmdData(0xD306, 0x00);
		LCD_WriteCmdData(0xD307, 0x4A);
		LCD_WriteCmdData(0xD308, 0x00);
		LCD_WriteCmdData(0xD309, 0x5C);
		LCD_WriteCmdData(0xD30A, 0x00);
		LCD_WriteCmdData(0xD30B, 0x81);
		LCD_WriteCmdData(0xD30C, 0x00);
		LCD_WriteCmdData(0xD30D, 0xA6);
		LCD_WriteCmdData(0xD30E, 0x00);
		LCD_WriteCmdData(0xD30F, 0xE5);
		LCD_WriteCmdData(0xD310, 0x01);
		LCD_WriteCmdData(0xD311, 0x13);
		LCD_WriteCmdData(0xD312, 0x01);
		LCD_WriteCmdData(0xD313, 0x54);
		LCD_WriteCmdData(0xD314, 0x01);
		LCD_WriteCmdData(0xD315, 0x82);
		LCD_WriteCmdData(0xD316, 0x01);
		LCD_WriteCmdData(0xD317, 0xCA);
		LCD_WriteCmdData(0xD318, 0x02);
		LCD_WriteCmdData(0xD319, 0x00);
		LCD_WriteCmdData(0xD31A, 0x02);
		LCD_WriteCmdData(0xD31B, 0x01);
		LCD_WriteCmdData(0xD31C, 0x02);
		LCD_WriteCmdData(0xD31D, 0x34);
		LCD_WriteCmdData(0xD31E, 0x02);
		LCD_WriteCmdData(0xD31F, 0x67);
		LCD_WriteCmdData(0xD320, 0x02);
		LCD_WriteCmdData(0xD321, 0x84);
		LCD_WriteCmdData(0xD322, 0x02);
		LCD_WriteCmdData(0xD323, 0xA4);
		LCD_WriteCmdData(0xD324, 0x02);
		LCD_WriteCmdData(0xD325, 0xB7);
		LCD_WriteCmdData(0xD326, 0x02);
		LCD_WriteCmdData(0xD327, 0xCF);
		LCD_WriteCmdData(0xD328, 0x02);
		LCD_WriteCmdData(0xD329, 0xDE);
		LCD_WriteCmdData(0xD32A, 0x02);
		LCD_WriteCmdData(0xD32B, 0xF2);
		LCD_WriteCmdData(0xD32C, 0x02);
		LCD_WriteCmdData(0xD32D, 0xFE);
		LCD_WriteCmdData(0xD32E, 0x03);
		LCD_WriteCmdData(0xD32F, 0x10);
		LCD_WriteCmdData(0xD330, 0x03);
		LCD_WriteCmdData(0xD331, 0x33);
		LCD_WriteCmdData(0xD332, 0x03);
		LCD_WriteCmdData(0xD333, 0x6D);
		LCD_WriteCmdData(0xD400, 0x00);
		LCD_WriteCmdData(0xD401, 0x33);
		LCD_WriteCmdData(0xD402, 0x00);
		LCD_WriteCmdData(0xD403, 0x34);
		LCD_WriteCmdData(0xD404, 0x00);
		LCD_WriteCmdData(0xD405, 0x3A);
		LCD_WriteCmdData(0xD406, 0x00);
		LCD_WriteCmdData(0xD407, 0x4A);
		LCD_WriteCmdData(0xD408, 0x00);
		LCD_WriteCmdData(0xD409, 0x5C);
		LCD_WriteCmdData(0xD40A, 0x00);
		LCD_WriteCmdData(0xD40B, 0x81);
		LCD_WriteCmdData(0xD40C, 0x00);
		LCD_WriteCmdData(0xD40D, 0xA6);
		LCD_WriteCmdData(0xD40E, 0x00);
		LCD_WriteCmdData(0xD40F, 0xE5);
		LCD_WriteCmdData(0xD410, 0x01);
		LCD_WriteCmdData(0xD411, 0x13);
		LCD_WriteCmdData(0xD412, 0x01);
		LCD_WriteCmdData(0xD413, 0x54);
		LCD_WriteCmdData(0xD414, 0x01);
		LCD_WriteCmdData(0xD415, 0x82);
		LCD_WriteCmdData(0xD416, 0x01);
		LCD_WriteCmdData(0xD417, 0xCA);
		LCD_WriteCmdData(0xD418, 0x02);
		LCD_WriteCmdData(0xD419, 0x00);
		LCD_WriteCmdData(0xD41A, 0x02);
		LCD_WriteCmdData(0xD41B, 0x01);
		LCD_WriteCmdData(0xD41C, 0x02);
		LCD_WriteCmdData(0xD41D, 0x34);
		LCD_WriteCmdData(0xD41E, 0x02);
		LCD_WriteCmdData(0xD41F, 0x67);
		LCD_WriteCmdData(0xD420, 0x02);
		LCD_WriteCmdData(0xD421, 0x84);
		LCD_WriteCmdData(0xD422, 0x02);
		LCD_WriteCmdData(0xD423, 0xA4);
		LCD_WriteCmdData(0xD424, 0x02);
		LCD_WriteCmdData(0xD425, 0xB7);
		LCD_WriteCmdData(0xD426, 0x02);
		LCD_WriteCmdData(0xD427, 0xCF);
		LCD_WriteCmdData(0xD428, 0x02);
		LCD_WriteCmdData(0xD429, 0xDE);
		LCD_WriteCmdData(0xD42A, 0x02);
		LCD_WriteCmdData(0xD42B, 0xF2);
		LCD_WriteCmdData(0xD42C, 0x02);
		LCD_WriteCmdData(0xD42D, 0xFE);
		LCD_WriteCmdData(0xD42E, 0x03);
		LCD_WriteCmdData(0xD42F, 0x10);
		LCD_WriteCmdData(0xD430, 0x03);
		LCD_WriteCmdData(0xD431, 0x33);
		LCD_WriteCmdData(0xD432, 0x03);
		LCD_WriteCmdData(0xD433, 0x6D);
		LCD_WriteCmdData(0xD500, 0x00);
		LCD_WriteCmdData(0xD501, 0x33);
		LCD_WriteCmdData(0xD502, 0x00);
		LCD_WriteCmdData(0xD503, 0x34);
		LCD_WriteCmdData(0xD504, 0x00);
		LCD_WriteCmdData(0xD505, 0x3A);
		LCD_WriteCmdData(0xD506, 0x00);
		LCD_WriteCmdData(0xD507, 0x4A);
		LCD_WriteCmdData(0xD508, 0x00);
		LCD_WriteCmdData(0xD509, 0x5C);
		LCD_WriteCmdData(0xD50A, 0x00);
		LCD_WriteCmdData(0xD50B, 0x81);
		LCD_WriteCmdData(0xD50C, 0x00);
		LCD_WriteCmdData(0xD50D, 0xA6);
		LCD_WriteCmdData(0xD50E, 0x00);
		LCD_WriteCmdData(0xD50F, 0xE5);
		LCD_WriteCmdData(0xD510, 0x01);
		LCD_WriteCmdData(0xD511, 0x13);
		LCD_WriteCmdData(0xD512, 0x01);
		LCD_WriteCmdData(0xD513, 0x54);
		LCD_WriteCmdData(0xD514, 0x01);
		LCD_WriteCmdData(0xD515, 0x82);
		LCD_WriteCmdData(0xD516, 0x01);
		LCD_WriteCmdData(0xD517, 0xCA);
		LCD_WriteCmdData(0xD518, 0x02);
		LCD_WriteCmdData(0xD519, 0x00);
		LCD_WriteCmdData(0xD51A, 0x02);
		LCD_WriteCmdData(0xD51B, 0x01);
		LCD_WriteCmdData(0xD51C, 0x02);
		LCD_WriteCmdData(0xD51D, 0x34);
		LCD_WriteCmdData(0xD51E, 0x02);
		LCD_WriteCmdData(0xD51F, 0x67);
		LCD_WriteCmdData(0xD520, 0x02);
		LCD_WriteCmdData(0xD521, 0x84);
		LCD_WriteCmdData(0xD522, 0x02);
		LCD_WriteCmdData(0xD523, 0xA4);
		LCD_WriteCmdData(0xD524, 0x02);
		LCD_WriteCmdData(0xD525, 0xB7);
		LCD_WriteCmdData(0xD526, 0x02);
		LCD_WriteCmdData(0xD527, 0xCF);
		LCD_WriteCmdData(0xD528, 0x02);
		LCD_WriteCmdData(0xD529, 0xDE);
		LCD_WriteCmdData(0xD52A, 0x02);
		LCD_WriteCmdData(0xD52B, 0xF2);
		LCD_WriteCmdData(0xD52C, 0x02);
		LCD_WriteCmdData(0xD52D, 0xFE);
		LCD_WriteCmdData(0xD52E, 0x03);
		LCD_WriteCmdData(0xD52F, 0x10);
		LCD_WriteCmdData(0xD530, 0x03);
		LCD_WriteCmdData(0xD531, 0x33);
		LCD_WriteCmdData(0xD532, 0x03);
		LCD_WriteCmdData(0xD533, 0x6D);
		LCD_WriteCmdData(0xD600, 0x00);
		LCD_WriteCmdData(0xD601, 0x33);
		LCD_WriteCmdData(0xD602, 0x00);
		LCD_WriteCmdData(0xD603, 0x34);
		LCD_WriteCmdData(0xD604, 0x00);
		LCD_WriteCmdData(0xD605, 0x3A);
		LCD_WriteCmdData(0xD606, 0x00);
		LCD_WriteCmdData(0xD607, 0x4A);
		LCD_WriteCmdData(0xD608, 0x00);
		LCD_WriteCmdData(0xD609, 0x5C);
		LCD_WriteCmdData(0xD60A, 0x00);
		LCD_WriteCmdData(0xD60B, 0x81);
		LCD_WriteCmdData(0xD60C, 0x00);
		LCD_WriteCmdData(0xD60D, 0xA6);
		LCD_WriteCmdData(0xD60E, 0x00);
		LCD_WriteCmdData(0xD60F, 0xE5);
		LCD_WriteCmdData(0xD610, 0x01);
		LCD_WriteCmdData(0xD611, 0x13);
		LCD_WriteCmdData(0xD612, 0x01);
		LCD_WriteCmdData(0xD613, 0x54);
		LCD_WriteCmdData(0xD614, 0x01);
		LCD_WriteCmdData(0xD615, 0x82);
		LCD_WriteCmdData(0xD616, 0x01);
		LCD_WriteCmdData(0xD617, 0xCA);
		LCD_WriteCmdData(0xD618, 0x02);
		LCD_WriteCmdData(0xD619, 0x00);
		LCD_WriteCmdData(0xD61A, 0x02);
		LCD_WriteCmdData(0xD61B, 0x01);
		LCD_WriteCmdData(0xD61C, 0x02);
		LCD_WriteCmdData(0xD61D, 0x34);
		LCD_WriteCmdData(0xD61E, 0x02);
		LCD_WriteCmdData(0xD61F, 0x67);
		LCD_WriteCmdData(0xD620, 0x02);
		LCD_WriteCmdData(0xD621, 0x84);
		LCD_WriteCmdData(0xD622, 0x02);
		LCD_WriteCmdData(0xD623, 0xA4);
		LCD_WriteCmdData(0xD624, 0x02);
		LCD_WriteCmdData(0xD625, 0xB7);
		LCD_WriteCmdData(0xD626, 0x02);
		LCD_WriteCmdData(0xD627, 0xCF);
		LCD_WriteCmdData(0xD628, 0x02);
		LCD_WriteCmdData(0xD629, 0xDE);
		LCD_WriteCmdData(0xD62A, 0x02);
		LCD_WriteCmdData(0xD62B, 0xF2);
		LCD_WriteCmdData(0xD62C, 0x02);
		LCD_WriteCmdData(0xD62D, 0xFE);
		LCD_WriteCmdData(0xD62E, 0x03);
		LCD_WriteCmdData(0xD62F, 0x10);
		LCD_WriteCmdData(0xD630, 0x03);
		LCD_WriteCmdData(0xD631, 0x33);
		LCD_WriteCmdData(0xD632, 0x03);
		LCD_WriteCmdData(0xD633, 0x6D);
		LCD_WriteCmdData(0xF000, 0x55);
		LCD_WriteCmdData(0xF001, 0xAA);
		LCD_WriteCmdData(0xF002, 0x52);
		LCD_WriteCmdData(0xF003, 0x08);
		LCD_WriteCmdData(0xF004, 0x00);
		LCD_WriteCmdData(0xB100, 0xCC);
		LCD_WriteCmdData(0xB101, 0x00);
		LCD_WriteCmdData(0xB600, 0x05);
		LCD_WriteCmdData(0xB700, 0x70);
		LCD_WriteCmdData(0xB701, 0x70);
		LCD_WriteCmdData(0xB800, 0x01);
		LCD_WriteCmdData(0xB801, 0x03);
		LCD_WriteCmdData(0xB802, 0x03);
		LCD_WriteCmdData(0xB803, 0x03);
		LCD_WriteCmdData(0xBC00, 0x02);
		LCD_WriteCmdData(0xBC01, 0x00);
		LCD_WriteCmdData(0xBC02, 0x00);
		LCD_WriteCmdData(0xC900, 0xD0);
		LCD_WriteCmdData(0xC901, 0x02);
		LCD_WriteCmdData(0xC902, 0x50);
		LCD_WriteCmdData(0xC903, 0x50);
		LCD_WriteCmdData(0xC904, 0x50);
		LCD_WriteCmdData(0x3500, 0x00);
		LCD_WriteCmdData(0x3A00, 0x55);
		LCD_WriteCmd(0x1100);
		delay_us(120);
		LCD_WriteCmd(0x2900);
    
#endif

#ifdef TFTLCD_ILI9481
	LCD_WriteCmd(0xFF);
	LCD_WriteCmd(0xFF);
	delay_ms(5);

	LCD_WriteCmd(0xFF);
	LCD_WriteCmd(0xFF);
	LCD_WriteCmd(0xFF);
	LCD_WriteCmd(0xFF);
	delay_ms(10);
	
	LCD_WriteCmd(0xB0);
	LCD_WriteData(0x00);
	
	LCD_WriteCmd(0xB3);
	LCD_WriteData(0x02);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	
	LCD_WriteCmd(0xC0);
	LCD_WriteData(0x13);
	LCD_WriteData(0x3B);//480
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x01);
	LCD_WriteData(0x00);//NW
	LCD_WriteData(0x43);
	
	LCD_WriteCmd(0xC1);
	LCD_WriteData(0x08);
	LCD_WriteData(0x1B);//CLOCK
	LCD_WriteData(0x08);
	LCD_WriteData(0x08);
	
	LCD_WriteCmd(0xC4);
	LCD_WriteData(0x11);
	LCD_WriteData(0x01);
	LCD_WriteData(0x73);
	LCD_WriteData(0x01);
	
	LCD_WriteCmd(0xC6);
	LCD_WriteData(0x00);
	
	LCD_WriteCmd(0xC8);
	LCD_WriteData(0x0F);
	LCD_WriteData(0x05);
	LCD_WriteData(0x14);
	LCD_WriteData(0x5C);
	LCD_WriteData(0x03);
	LCD_WriteData(0x07);
	LCD_WriteData(0x07);
	LCD_WriteData(0x10);
	LCD_WriteData(0x00);
	LCD_WriteData(0x23);
	
	LCD_WriteData(0x10);
	LCD_WriteData(0x07);
	LCD_WriteData(0x07);
	LCD_WriteData(0x53);
	LCD_WriteData(0x0C);
	LCD_WriteData(0x14);
	LCD_WriteData(0x05);
	LCD_WriteData(0x0F);
	LCD_WriteData(0x23);
	LCD_WriteData(0x00);
	
	LCD_WriteCmd(0x35);
	LCD_WriteData(0x00);
	
	LCD_WriteCmd(0x44);
	LCD_WriteData(0x00);
	LCD_WriteData(0x01);
	
	LCD_WriteCmd(0xD0);
	LCD_WriteData(0x07);
	LCD_WriteData(0x07);//VCI1
	LCD_WriteData(0x1D);//VRH
	LCD_WriteData(0x03);//BT
	
	LCD_WriteCmd(0xD1);
	LCD_WriteData(0x03);
	LCD_WriteData(0x5B);//VCM
	LCD_WriteData(0x10);//VDV
	
	LCD_WriteCmd(0xD2);
	LCD_WriteData(0x03);
	LCD_WriteData(0x24);
	LCD_WriteData(0x04);

	LCD_WriteCmd(0x2A);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x01);
	LCD_WriteData(0x3F);//320
	
	LCD_WriteCmd(0x2B);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x01);
	LCD_WriteData(0xDF);//480

	LCD_WriteCmd(0x36);
	LCD_WriteData(0x00);

	LCD_WriteCmd(0xC0);
	LCD_WriteData(0x13);

	LCD_WriteCmd(0x3A);
	LCD_WriteData(0x55);

	LCD_WriteCmd(0x11);
	delay_ms(150);
	
	LCD_WriteCmd(0x29);
	delay_ms(30);
		
	LCD_WriteCmd(0x2C);
#endif

#ifdef TFTLCD_R61509VE
	for(i=500; i>0; i--);
	LCD_WriteCmd(0x0000);LCD_WriteData(0x0000); 
	LCD_WriteCmd(0x0000);LCD_WriteData(0x0000);  
	LCD_WriteCmd(0x0000);LCD_WriteData(0x0000);  
	for(i=500; i>0; i--);
	
	LCD_WriteCmd(0x0400);LCD_WriteData(0x6200); 
	LCD_WriteCmd(0x0008);LCD_WriteData(0x0808); 
	 
	LCD_WriteCmd(0x0300);LCD_WriteData(0x0C00);  
	LCD_WriteCmd(0x0301);LCD_WriteData(0x5A0B);  
	LCD_WriteCmd(0x0302);LCD_WriteData(0x0906); 
	LCD_WriteCmd(0x0303);LCD_WriteData(0x1017);  
	LCD_WriteCmd(0x0304);LCD_WriteData(0x2300);  
	LCD_WriteCmd(0x0305);LCD_WriteData(0x1700);  
	LCD_WriteCmd(0x0306);LCD_WriteData(0x6309); 
	LCD_WriteCmd(0x0307);LCD_WriteData(0x0C09); 
	LCD_WriteCmd(0x0308);LCD_WriteData(0x010C);  
	LCD_WriteCmd(0x0309);LCD_WriteData(0x2232); 
	
	
	
	LCD_WriteCmd(0x0010);LCD_WriteData(0x0016);  
	LCD_WriteCmd(0x0011);LCD_WriteData(0x0101);  
	LCD_WriteCmd(0x0012);LCD_WriteData(0x0000);  
	LCD_WriteCmd(0x0013);LCD_WriteData(0x0001); 
	 
	LCD_WriteCmd(0x0100);LCD_WriteData(0x0330);  
	LCD_WriteCmd(0x0101);LCD_WriteData(0x0336);  
	LCD_WriteCmd(0x0103);LCD_WriteData(0x1000); 
	 
	LCD_WriteCmd(0x0280);LCD_WriteData(0x6100); 
	LCD_WriteCmd(0x0102);LCD_WriteData(0xBBB4); 
	for(i=500; i>0; i--);
	
	LCD_WriteCmd(0x0001);LCD_WriteData(0x0100);  
	LCD_WriteCmd(0x0002);LCD_WriteData(0x0100);  
	LCD_WriteCmd(0x0003);LCD_WriteData(0x1030);  
	LCD_WriteCmd(0x0009);LCD_WriteData(0x0001);  
	LCD_WriteCmd(0x000C);LCD_WriteData(0x0000); 
	LCD_WriteCmd(0x0090);LCD_WriteData(0x0800); 
	LCD_WriteCmd(0x000F);LCD_WriteData(0x0000); 
	
	LCD_WriteCmd(0x0210);LCD_WriteData(0x0000);  
	LCD_WriteCmd(0x0211);LCD_WriteData(0x00EF); 
	LCD_WriteCmd(0x0212);LCD_WriteData(0x0000);  
	LCD_WriteCmd(0x0213);LCD_WriteData(0x018F);         //;400
	
	
	LCD_WriteCmd(0x0500);LCD_WriteData(0x0000);  
	LCD_WriteCmd(0x0501);LCD_WriteData(0x0000);  
	LCD_WriteCmd(0x0502);LCD_WriteData(0x005F);  
	
	LCD_WriteCmd(0x0401);LCD_WriteData(0x0001);  
	LCD_WriteCmd(0x0404);LCD_WriteData(0x0000); 
	for(i=500; i>0; i--);
	
	LCD_WriteCmd(0x0007);LCD_WriteData(0x0100); 
	for(i=500; i>0; i--);
	
	
	LCD_WriteCmd(0x0202);
#endif
	
#ifdef TFTLCD_SSD1963N
	LCD_WriteCmd(0xE2);  //Set PLL with OSC = 10MHz (hardware), Multiplier N = 35, 250MHz < VCO < 800MHz = OSC*(N+1), VCO = 360MHz
	// LCD_WriteData(0x23);  //OSC = 10MHz (hardware),//参数1 
	LCD_WriteData(0x2C);  //OSC = 8MHz (hardware),///参数1  
	LCD_WriteData(0x02);  //参数2 Divider M = 2, PLL = 360/(M+1) = 120MHz
	//LCD_WriteData(0x54);  //参数3 Validate M and N values
	LCD_WriteData(0x04);

	//LCD_WriteCmd(0xE3);

	LCD_WriteCmd(0xE0);  // Start PLL command
	LCD_WriteData(0x01);  // enable PLL
	delay_ms(1);

	LCD_WriteCmd(0xE0);  // Start PLL command again
	LCD_WriteData(0x03);  // now, use PLL output as system clock 
	delay_ms(5);

	//LCD_WriteCmd(0xE4);  // Start PLL command again

	LCD_WriteCmd(0x01);  //软复位
	delay_ms(10);

	//Set LSHIFT freq, i.e. the DCLK with PLL freq 120MHz set previously
	//Typical DCLK for TY700TFT800480 is 33.3MHz(datasheet), experiment shows 30MHz gives a stable result
	//30MHz = 120MHz*(LCDC_FPR+1)/2^20
	//LCDC_FPR = 262143 (0x3FFFF)
	//Time per line = (DISP_HOR_RESOLUTION+DISP_HOR_PULSE_WIDTH+DISP_HOR_BACK_PORCH+DISP_HOR_FRONT_PORCH)/30 us = 1056/30 = 35.2us
	//9MHz = 120MHz*(LCDC_FPR+1)/2^20
	//LCDC_FPR = 78642 (0x13332) 
	LCD_WriteCmd(0xE6);
	LCD_WriteData(0x01);//0x03
	LCD_WriteData(0x33);
	LCD_WriteData(0x32);

	//Set LCD mode, varies from individual manufacturer
	LCD_WriteCmd(0xB0);
	//LCD_WriteData(0x10);//0x10    // set 18-bit for 7" panel TY700TFT800480
	//LCD_WriteData(0x80);    // set TTL mode
	LCD_WriteData(0x20);//0x10    // set 18-bit for 7" panel TY700TFT800480
	LCD_WriteData(0x00);    // set TTL mode
	LCD_WriteData((SSD_HOR_RESOLUTION-1)>>8);//设置LCD水平像素
	LCD_WriteData(SSD_HOR_RESOLUTION-1);		 
	LCD_WriteData((SSD_VER_RESOLUTION-1)>>8);//设置LCD垂直像素
	LCD_WriteData(SSD_VER_RESOLUTION-1);		 
	LCD_WriteData(0x00);		//RGB序列
	
	//LCD_WriteCmd(0xB1);

	//Set horizontal period
	LCD_WriteCmd(0xB4);
	LCD_WriteData(SSD_HT>>8);  //Set HT
	LCD_WriteData(SSD_HT);
	LCD_WriteData(SSD_HPS>>8);  //Set HPS
	LCD_WriteData(SSD_HPS);
	LCD_WriteData(SSD_HPW);      //Set HPW
	LCD_WriteData(SSD_LPS>>8);  //Set HPS
	LCD_WriteData(SSD_LPS);
	LCD_WriteData(0x0000);
	
	//LCD_WriteCmd(0xB5);

	//Set vertical period
	LCD_WriteCmd(0xB6);
	LCD_WriteData(SSD_VT>>8);   //Set VT
	LCD_WriteData(SSD_VT);
	LCD_WriteData(SSD_VPS>>8);  //Set VPS
	LCD_WriteData(SSD_VPS);
	LCD_WriteData(SSD_VPW);      //Set VPW
	LCD_WriteData(SSD_FPS>>8);  //Set FPS
	LCD_WriteData(SSD_FPS);

	LCD_WriteCmd(0xB8); //设置GPIO配置
	LCD_WriteData(0x0F); //GPIO3=output, GPIO[2:0]=output//GPIO is controlled by host; 4个IO设置成输出
	LCD_WriteData(0x01); //GPIO0 使用正常的IO功能 
	//LCD_WriteCmd(0xB7);

	LCD_WriteCmd(0xBA); //设置GPIO配置
	LCD_WriteData(0x0001); //GPIO[3:0] out 1

	LCD_WriteCmd(0x0036); //rotation
	LCD_WriteData(0x0000); 
	//LCD_WriteCmd(0x3A); //Set pixel format, i.e. the bpp
	//LCD_WriteData(0x55);  //set 16bpp

	LCD_WriteCmd(0xF0); //设置SSD1963同CPU接口为16bit Set pixel data interface 
	LCD_WriteData(0x03); //16-bit(565 format) data for 16bpp PIC32MX only

	//LCD_WriteCmd(0xF1);

	LCD_WriteCmd(0x21); //进入图形颜色翻转模式

	LCD_WriteCmd(0xBC);//重要
	LCD_WriteData(0X80);//亮度值
	LCD_WriteData(0X80);//对比度值
	LCD_WriteData(0X80);//饱和度值
	LCD_WriteData(0x01);//处理机允许
	delay_ms(10);
	LCD_WriteCmd(0x29); // Turn on display; show the image on display

	//set PWM 
	LCD_WriteCmd(0xBE); //
	LCD_WriteData(0x0E); //PLL(120M)/(256*Value)/256=120Hz
	LCD_WriteData(0xFF); //duty=X/256 
	LCD_WriteData(0x09); //DBC enable PWM enable 
	LCD_WriteData(0xFF); //DBC manual brightness
	LCD_WriteData(0x00); //DBC minimum brightness 
	LCD_WriteData(0x00); //Brightness prescaler ff(system frequency / Divcode / 32768)

//	LCD_WriteCmd(0xD4); //
//	//TH1 = display width * display height * 3 * 0.1 /16 
//	//480*272 * 3 * 0.1 /16 =990H
//	LCD_WriteData(0x00); //  
//	LCD_WriteData(0x09); //  
//	LCD_WriteData(0x90); //  
//	//TH2 = display width * display height * 3 * 0.25 /16
//	//480*272 * 3 * 0.25 /16 =17E8H
//	LCD_WriteData(0x00); //  
//	LCD_WriteData(0x17); //  
//	LCD_WriteData(0xE8); // 
//	//TH3 = display width * display height * 3 * 0.6 /16 
//	//480*272 * 3 * 0.6 /16  =3960H
//	LCD_WriteData(0x00); //  
//	LCD_WriteData(0x39); //  
//	LCD_WriteData(0x60); // 

	LCD_WriteCmd(0xD0); //
	LCD_WriteData(0x0D); //PLL(120M)/(256*Value)/256=120Hz
	LCD_SSD_BackLightSet(100);//背光设置为最亮	
#endif
		
#ifdef TFTLCD_ILI9488
	LCD_WriteCmd(0xE0); //P-Gamma
	LCD_WriteData(0x00);
	LCD_WriteData(0x13);
	LCD_WriteData(0x18);
	LCD_WriteData(0x04);
	LCD_WriteData(0x0F);
	LCD_WriteData(0x06);
	LCD_WriteData(0x3A);
	LCD_WriteData(0x56);
	LCD_WriteData(0x4D);
	LCD_WriteData(0x03);
	LCD_WriteData(0x0A);
	LCD_WriteData(0x06);
	LCD_WriteData(0x30);
	LCD_WriteData(0x3E);
	LCD_WriteData(0x0F);
	
	LCD_WriteCmd(0XE1); //N-Gamma
	LCD_WriteData(0x00);
	LCD_WriteData(0x13);
	LCD_WriteData(0x18);
	LCD_WriteData(0x01);
	LCD_WriteData(0x11);
	LCD_WriteData(0x06);
	LCD_WriteData(0x38);
	LCD_WriteData(0x34);
	LCD_WriteData(0x4D);
	LCD_WriteData(0x06);
	LCD_WriteData(0x0D);
	LCD_WriteData(0x0B);
	LCD_WriteData(0x31);
	LCD_WriteData(0x37);
	LCD_WriteData(0x0F); 
	
	LCD_WriteCmd(0XC0);   //Power Control 1
	LCD_WriteData(0x18); //Vreg1out
	LCD_WriteData(0x17); //Verg2out
	
	LCD_WriteCmd(0xC1);   //Power Control 2
	LCD_WriteData(0x41); //VGH,VGL
	
	LCD_WriteCmd(0xC5);   //Power Control 3
	LCD_WriteData(0x00);
	LCD_WriteData(0x1A); //Vcom
	LCD_WriteData(0x80);
	
	LCD_WriteCmd(0x36);   //Memory Access
	LCD_WriteData(0x08);   //48
	
	LCD_WriteCmd(0x3A);   // Interface Pixel Format
	LCD_WriteData(0x55); //16bit
	
	LCD_WriteCmd(0XB0);   // Interface Mode Control
	LCD_WriteData(0x00);
	
	LCD_WriteCmd(0xB1);   //Frame rate
	LCD_WriteData(0xA0); //60Hz
	
	LCD_WriteCmd(0xB4);   //Display Inversion Control
	LCD_WriteData(0x02); //2-dot
	
	LCD_WriteCmd(0XB6);   //RGB/MCU Interface Control
//	LCD_WriteData(0x02); //MCU RGB
//	LCD_WriteData(0x02); //Source,Gate scan dieection
	LCD_WriteData(0x00);
	LCD_WriteData(0x22);//0 GS SS SM ISC[3:0];其中GS SS控制显示方向，同时修改R36
	LCD_WriteData(0x3B);
	
	LCD_WriteCmd(0XE9);    // Set Image Function
	LCD_WriteData(0x00);  //disable 24 bit data input
	
	LCD_WriteCmd(0xF7);    // Adjust Control
	LCD_WriteData(0xA9);
	LCD_WriteData(0x51);
	LCD_WriteData(0x2C);
	LCD_WriteData(0x82);  // D7 stream, loose
	
	LCD_WriteCmd(0x11);    //Sleep out
	delay_ms(120);
	LCD_WriteCmd(0x29);  
	LCD_WriteCmd(0x2C);
#endif

	
#ifdef TFTLCD_ILI9806
LCD_WriteCmd(0xFF); 
		LCD_WriteData(0xFF);
		LCD_WriteData(0x98);
		LCD_WriteData(0x06);
		LCD_WriteCmd(0xBC);
		LCD_WriteData(0x01);
		LCD_WriteData(0x0F);
		LCD_WriteData(0x61);
		LCD_WriteData(0xFF);
		LCD_WriteData(0x01);
		LCD_WriteData(0x01);
		LCD_WriteData(0x0B);
		LCD_WriteData(0x10);
		LCD_WriteData(0x37);
		LCD_WriteData(0x63);
		LCD_WriteData(0xFF);
		LCD_WriteData(0xFF);
		LCD_WriteData(0x01);
		LCD_WriteData(0x01);
		LCD_WriteData(0x00);
		LCD_WriteData(0x00);
		LCD_WriteData(0xFF);
		LCD_WriteData(0x52);
		LCD_WriteData(0x01);
		LCD_WriteData(0x00);
		LCD_WriteData(0x40);
		LCD_WriteCmd(0xBD);
		LCD_WriteData(0x01);
		LCD_WriteData(0x23);
		LCD_WriteData(0x45);
		LCD_WriteData(0x67);
		LCD_WriteData(0x01);
		LCD_WriteData(0x23);
		LCD_WriteData(0x45);
		LCD_WriteData(0x67);
		LCD_WriteCmd(0xBE);
		LCD_WriteData(0x00);
		LCD_WriteData(0x01);
		LCD_WriteData(0xAB);
		LCD_WriteData(0x60);
		LCD_WriteData(0x22);
		LCD_WriteData(0x22);
		LCD_WriteData(0x22);
		LCD_WriteData(0x22);
		LCD_WriteData(0x22);
		LCD_WriteCmd(0xC7);
		LCD_WriteData(0x36);
		LCD_WriteCmd(0xED);
		LCD_WriteData(0x7F);
		LCD_WriteData(0x0F);
		LCD_WriteCmd(0XC0);
		LCD_WriteData(0x0F);
		LCD_WriteData(0x0B);
		LCD_WriteData(0x0A);
		LCD_WriteCmd(0XFC);
		LCD_WriteData(0x08);
		LCD_WriteCmd(0XDF); 
		LCD_WriteData(0x00);
		LCD_WriteData(0x00);
		LCD_WriteData(0x00);
		LCD_WriteData(0x00);
		LCD_WriteData(0x00);
		LCD_WriteData(0x20);
		LCD_WriteCmd(0XF3);
		LCD_WriteData(0x74);
		LCD_WriteCmd(0xB4);
		LCD_WriteData(0x00);
		LCD_WriteData(0x00);
		LCD_WriteData(0x00);
		LCD_WriteCmd(0xF7);
		LCD_WriteData(0x82);
		LCD_WriteCmd(0xB1);
		LCD_WriteData(0x00);
		LCD_WriteData(0x13);
		LCD_WriteData(0x13); 
		LCD_WriteCmd(0XF2);
		LCD_WriteData(0x80);
		LCD_WriteData(0x04);
		LCD_WriteData(0x40);
		LCD_WriteData(0x28);
		LCD_WriteCmd(0XC1);
		LCD_WriteData(0x17);
		LCD_WriteData(0x88);
		LCD_WriteData(0x88);
		LCD_WriteData(0x20);
		LCD_WriteCmd(0xE0);
		LCD_WriteData(0x00);
		LCD_WriteData(0x0A);
		LCD_WriteData(0x12);
		LCD_WriteData(0x10);
		LCD_WriteData(0x0E);
		LCD_WriteData(0x20);
		LCD_WriteData(0xCC);
		LCD_WriteData(0x07);
		LCD_WriteData(0x06);
		LCD_WriteData(0x0B);
		LCD_WriteData(0x0E);
		LCD_WriteData(0x0F);
		LCD_WriteData(0x0D);
		LCD_WriteData(0x15);
		LCD_WriteData(0x10);
		LCD_WriteData(0x00);
		LCD_WriteCmd(0xE1);
		LCD_WriteData(0x00);
		LCD_WriteData(0x0B);
		LCD_WriteData(0x13);
		LCD_WriteData(0x0D);
		LCD_WriteData(0x0E);
		LCD_WriteData(0x1B);
		LCD_WriteData(0x71);
		LCD_WriteData(0x06);
		LCD_WriteData(0x06);
		LCD_WriteData(0x0A);
		LCD_WriteData(0x0F);
		LCD_WriteData(0x0E);
		LCD_WriteData(0x0F);
		LCD_WriteData(0x15);
		LCD_WriteData(0x0C);
		LCD_WriteData(0x00);
		LCD_WriteCmd(0x2a);
		LCD_WriteData(0x00);
		LCD_WriteData(0x00);
		LCD_WriteData(0x01);
		LCD_WriteData(0xdf);
		LCD_WriteCmd(0x2b);
		LCD_WriteData(0x00);
		LCD_WriteData(0x00);
		LCD_WriteData(0x03);
		LCD_WriteData(0x1f);
		LCD_WriteCmd(0x3A);
		LCD_WriteData(0x55);
		LCD_WriteCmd(0x36);
		LCD_WriteData(0x00);
		LCD_WriteCmd(0x11);
		delay_ms(120);   
		LCD_WriteCmd(0x29);  
		delay_ms(20);  
		LCD_WriteCmd(0x2C);
#endif
	LCD_Display_Dir(TFTLCD_DIR);		//0：竖屏  1：横屏  默认竖屏
	LCD_Clear(WHITE);
}

//清屏函数
//color:要清屏的填充色
void LCD_Clear(u16 color)
{
	uint16_t i, j ;

	LCD_Set_Window(0, 0, tftlcd_data.width-1, tftlcd_data.height-1);	 //作用区域
  	for(i=0; i<tftlcd_data.width; i++)
	{
		for (j=0; j<tftlcd_data.height; j++)
		{
			LCD_WriteData_Color(color);
		}
	} 
}


//在指定区域内填充单个颜色
//(sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1)   
//color:要填充的颜色
void LCD_Fill(u16 xState,u16 yState,u16 xEnd,u16 yEnd,u16 color)
{          
	uint16_t temp;

    if((xState > xEnd) || (yState > yEnd))
    {
        return;
    }   
	LCD_Set_Window(xState, yState, xEnd, yEnd); 
    xState = xEnd - xState + 1;
	yState = yEnd - yState + 1;

	while(xState--)
	{
	 	temp = yState;
		while (temp--)
	 	{			
			LCD_WriteData_Color(color);	
		}
	}	
} 

//在指定区域内填充指定颜色块			 
//(sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1)   
//color:要填充的颜色
void LCD_Color_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 *color)
{  
	u16 height,width;
	u16 i,j;
	width=ex-sx+1; 			//得到填充的宽度
	height=ey-sy+1;			//高度
	
	for(i=0;i<height;i++)
	{
		for(j=0;j<width;j++)
		{
			LCD_Set_Window(sx+j, sy+i,ex, ey);
			LCD_WriteData_Color(color[i*width+j]);
		}
	}		  
}
//画点
//x,y:坐标
//FRONT_COLOR:此点的颜色
void LCD_DrawPoint(u16 x,u16 y)
{
	LCD_Set_Window(x, y, x, y);  //设置点的位置
	LCD_WriteData_Color(FRONT_COLOR);	
}

//快速画点
//x,y:坐标
//color:颜色
void LCD_DrawFRONT_COLOR(u16 x,u16 y,u16 color)
{	   
	LCD_Set_Window(x, y, x, y);
	LCD_WriteData_Color(color);	
} 

//画线
//x1,y1:起点坐标
//x2,y2:终点坐标  
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2)
{
	u16 t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance; 
	int incx,incy,uRow,uCol; 
	delta_x=x2-x1; //计算坐标增量 
	delta_y=y2-y1; 
	uRow=x1; 
	uCol=y1; 
	if(delta_x>0)incx=1; //设置单步方向 
	else if(delta_x==0)incx=0;//垂直线 
	else {incx=-1;delta_x=-delta_x;} 
	if(delta_y>0)incy=1; 
	else if(delta_y==0)incy=0;//水平线 
	else{incy=-1;delta_y=-delta_y;} 
	if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y; 
	for(t=0;t<=distance+1;t++ )//画线输出 
	{  
		LCD_DrawPoint(uRow,uCol);//画点 
		xerr+=delta_x ; 
		yerr+=delta_y ; 
		if(xerr>distance) 
		{ 
			xerr-=distance; 
			uRow+=incx; 
		} 
		if(yerr>distance) 
		{ 
			yerr-=distance; 
			uCol+=incy; 
		} 
	}  
} 

void LCD_DrawLine_Color(u16 x1, u16 y1, u16 x2, u16 y2,u16 color)
{
	u16 t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance; 
	int incx,incy,uRow,uCol; 
	delta_x=x2-x1; //计算坐标增量 
	delta_y=y2-y1; 
	uRow=x1; 
	uCol=y1; 
	if(delta_x>0)incx=1; //设置单步方向 
	else if(delta_x==0)incx=0;//垂直线 
	else {incx=-1;delta_x=-delta_x;} 
	if(delta_y>0)incy=1; 
	else if(delta_y==0)incy=0;//水平线 
	else{incy=-1;delta_y=-delta_y;} 
	if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y; 
	for(t=0;t<=distance+1;t++ )//画线输出 
	{  
		LCD_DrawFRONT_COLOR(uRow,uCol,color);//画点 
		xerr+=delta_x ; 
		yerr+=delta_y ; 
		if(xerr>distance) 
		{ 
			xerr-=distance; 
			uRow+=incx; 
		} 
		if(yerr>distance) 
		{ 
			yerr-=distance; 
			uCol+=incy; 
		} 
	}  
} 


// 画一个十字的标记
// x：标记的X坐标
// y：标记的Y坐标
// color：标记的颜色
void LCD_DrowSign(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t i;

    /* 画点 */
    LCD_Set_Window(x-1, y-1, x+1, y+1);
    for(i=0; i<9; i++)
    {
		LCD_WriteData_Color(color);   
    }

    /* 画竖 */
    LCD_Set_Window(x-4, y, x+4, y);
    for(i=0; i<9; i++)
    {
		LCD_WriteData_Color(color); 
    }

    /* 画横 */
    LCD_Set_Window(x, y-4, x, y+4);
    for(i=0; i<9; i++)
    {
		LCD_WriteData_Color(color); 
    }
}

//画矩形	  
//(x1,y1),(x2,y2):矩形的对角坐标
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
	LCD_DrawLine(x1,y1,x2,y1);
	LCD_DrawLine(x1,y1,x1,y2);
	LCD_DrawLine(x1,y2,x2,y2);
	LCD_DrawLine(x2,y1,x2,y2);
}
//在指定位置画一个指定大小的圆
//(x,y):中心点
//r    :半径
void LCD_Draw_Circle(u16 x0,u16 y0,u8 r)
{
	int a,b;
	int di;
	a=0;b=r;	  
	di=3-(r<<1);             //判断下个点位置的标志
	while(a<=b)
	{
		LCD_DrawPoint(x0+a,y0-b);             //5
 		LCD_DrawPoint(x0+b,y0-a);             //0           
		LCD_DrawPoint(x0+b,y0+a);             //4               
		LCD_DrawPoint(x0+a,y0+b);             //6 
		LCD_DrawPoint(x0-a,y0+b);             //1       
 		LCD_DrawPoint(x0-b,y0+a);             
		LCD_DrawPoint(x0-a,y0-b);             //2             
  		LCD_DrawPoint(x0-b,y0-a);             //7     	         
		a++;
		//使用Bresenham算法画圆     
		if(di<0)di +=4*a+6;	  
		else
		{
			di+=10+4*(a-b);   
			b--;
		} 						    
	}
} 



//在指定位置显示一个字符
//x,y:起始坐标
//num:要显示的字符:" "--->"~"
//size:字体大小 12/16/24
//mode:叠加方式(1)还是非叠加方式(0)
void LCD_ShowChar(u16 x,u16 y,u8 num,u8 size,u8 mode)
{  							  
    u8 temp,t1,t;
	u16 y0=y;
	u8 csize=(size/8+((size%8)?1:0))*(size/2);		//得到字体一个字符对应点阵集所占的字节数	
 	num=num-' ';//得到偏移后的值（ASCII字库是从空格开始取模，所以-' '就是对应字符的字库）
	for(t=0;t<csize;t++)
	{   
		if(size==12)temp=ascii_1206[num][t]; 	 	//调用1206字体
		else if(size==16)temp=ascii_1608[num][t];	//调用1608字体
		else if(size==24)temp=ascii_2412[num][t];	//调用2412字体
		else return;								//没有的字库
		for(t1=0;t1<8;t1++)
		{			    
			if(temp&0x80)LCD_DrawFRONT_COLOR(x,y,FRONT_COLOR);
			else if(mode==0)LCD_DrawFRONT_COLOR(x,y,BACK_COLOR);
			temp<<=1;
			y++;
			if(y>=tftlcd_data.height)return;		//超区域了
			if((y-y0)==size)
			{
				y=y0;
				x++;
				if(x>=tftlcd_data.width)return;	//超区域了
				break;
			}
		}  	 
	}  	    	   	 	  
}   
//m^n函数
//返回值:m^n次方.
u32 LCD_Pow(u8 m,u8 n)
{
	u32 result=1;	 
	while(n--)result*=m;    
	return result;
}			 
//显示数字,高位为0,则不显示
//x,y :起点坐标	 
//len :数字的位数
//size:字体大小
//color:颜色 
//num:数值(0~4294967295);	 
void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 size)
{         	
	u8 t,temp;
	u8 enshow=0;						   
	for(t=0;t<len;t++)
	{
		temp=(num/LCD_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				LCD_ShowChar(x+(size/2)*t,y,' ',size,0);
				continue;
			}else enshow=1; 
		 	 
		}
	 	LCD_ShowChar(x+(size/2)*t,y,temp+'0',size,0); 
	}
} 

//显示数字,高位为0,还是显示
//x,y:起点坐标
//num:数值(0~999999999);	 
//len:长度(即要显示的位数)
//size:字体大小
//mode:
//[7]:0,不填充;1,填充0.
//[6:1]:保留
//[0]:0,非叠加显示;1,叠加显示.
void LCD_ShowxNum(u16 x,u16 y,u32 num,u8 len,u8 size,u8 mode)
{  
	u8 t,temp;
	u8 enshow=0;						   
	for(t=0;t<len;t++)
	{
		temp=(num/LCD_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				if(mode&0X80)LCD_ShowChar(x+(size/2)*t,y,'0',size,mode&0X01);  
				else LCD_ShowChar(x+(size/2)*t,y,' ',size,mode&0X01);  
 				continue;
			}else enshow=1; 
		 	 
		}
	 	LCD_ShowChar(x+(size/2)*t,y,temp+'0',size,mode&0X01); 
	}
} 
//显示字符串
//x,y:起点坐标
//width,height:区域大小  
//size:字体大小
//*p:字符串起始地址		  
void LCD_ShowString(u16 x,u16 y,u16 width,u16 height,u8 size,u8 *p)
{         
	u8 x0=x;
	width+=x;
	height+=y;
    while((*p<='~')&&(*p>=' '))//判断是不是非法字符!
    {       
        if(x>=width){x=x0;y+=size;}
        if(y>=height)break;//退出
        LCD_ShowChar(x,y,*p,size,0);
        x+=size/2;
        p++;
    }  
}

/****************************************************************************
*函数名：LCD_ShowFontHZ
*输  入：x：汉字显示的X坐标
*      * y：汉字显示的Y坐标
*      * cn：要显示的汉字
*      * wordColor：文字的颜色
*      * backColor：背景颜色
*输  出：
*功  能：写二号楷体汉字
****************************************************************************/
#if 0
void LCD_ShowFontHZ(u16 x, u16 y, u8 *cn)	 
{  
	u8 i, j, wordNum;
	u16 color;
	while (*cn != '\0')
	{
		LCD_Set_Window(x, y, x+31, y+28);
		for (wordNum=0; wordNum<20; wordNum++)
		{	//wordNum扫描字库的字数
			if ((CnChar32x29[wordNum].Index[0]==*cn)
			     &&(CnChar32x29[wordNum].Index[1]==*(cn+1)))
			{
				for(i=0; i<116; i++) 
				{	//MSK的位数
					color=CnChar32x29[wordNum].Msk[i];
					for(j=0;j<8;j++) 
					{
						if((color&0x80)==0x80)
						{
							LCD_WriteData_Color(FRONT_COLOR); 						
						} 						
						else
						{
							LCD_WriteData_Color(BACK_COLOR); 
						} 
						color<<=1;
					}//for(j=0;j<8;j++)结束
				}    
			}
		} //for (wordNum=0; wordNum<20; wordNum++)结束 	
		cn += 2;
		x += 32;
	}
}
#endif


#if 1
void LCD_ShowFontHZ(u16 x, u16 y, u8 *cn)
{
	u8 i, j, wordNum;
	u16 color;
	u16 x0=x; 
	u16 y0=y; 
	while (*cn != '\0')
	{
		for (wordNum=0; wordNum<20; wordNum++)
		{	//wordNum扫描字库的字数
			if ((CnChar32x29[wordNum].Index[0]==*cn)
			     &&(CnChar32x29[wordNum].Index[1]==*(cn+1)))
			{
				for(i=0; i<116; i++) 
				{	//MSK的位数
					color=CnChar32x29[wordNum].Msk[i];
					for(j=0;j<8;j++) 
					{
						if((color&0x80)==0x80)
						{
							LCD_DrawFRONT_COLOR(x,y,FRONT_COLOR);
						} 						
						else
						{
							LCD_DrawFRONT_COLOR(x,y,BACK_COLOR);
						} 
						color<<=1;
						x++;
						if((x-x0)==32)
						{
							x=x0;
							y++;
							if((y-y0)==29)
							{
								y=y0;
							}
						}
					}//for(j=0;j<8;j++)结束
				}	
			}
			
		} //for (wordNum=0; wordNum<20; wordNum++)结束 	
		cn += 2;
		x += 32;
		x0=x;
	}
}	
#endif

void LCD_ShowPicture(u16 x, u16 y, u16 wide, u16 high,u8 *pic)
{
	u16 temp = 0;
	long tmp=0,num=0;
	LCD_Set_Window(x, y, x+wide-1, y+high-1);
	num = wide * high*2 ;
	do
	{  
		temp = pic[tmp + 1];
		temp = temp << 8;
		temp = temp | pic[tmp];
		LCD_WriteData_Color(temp);//逐点显示
		tmp += 2;
	}
	while(tmp < num);	
}

/*
 * 显示 12x12 汉字/字符
 * x, y : 起始坐标
 * index: 汉字索引(在字库中, 0~94)
 * color: 前景色
 * back : 背景色
 */
void LCD_ShowChinese12x12(u16 x, u16 y, u8 index, u16 color, u16 back)
{
    u8 i, j;
    u8 byte1, byte2;
    const unsigned char *p = hanzi_1212[index];

    for (i = 0; i < 12; i++) 
    {
        byte1 = p[i * 2];     
        byte2 = p[i * 2 + 1]; 

        for (j = 0; j < 12; j++) 
        {
            u8 bit_val;
            if (j < 8)
                bit_val = (byte1 >> (7 - j)) & 0x01; 
            else
                bit_val = (byte2 >> (7 - (j - 8))) & 0x01; 

            if (bit_val)
            {
                FRONT_COLOR = color;       // 设置前景色
                LCD_DrawPoint(x + i, y + j); // 画点
            }
            else
            {
                FRONT_COLOR = back;        // 设置背景色
                LCD_DrawPoint(x + i, y + j); 
            }
        }
    }
}
