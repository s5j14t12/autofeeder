#include "stm32l476xx.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define READ_BIT(REG, BIT)   ((REG) & (BIT))
TIM_TypeDef	*timer = TIM3;

void GPIO_init();
void openDoor();
void closeDoor();
void timer_start(TIM_TypeDef *timer);
void timer_stop(TIM_TypeDef *timer);
void timer_init (TIM_TypeDef *timer);
void openDoorLevelOne();
void reset_Max7219();
int processKeyButton();
void getCurrentTime(int firstTime);
void SysTick_Handler(void);
void SystemClock_Config(void);
void UART_Transmit(char *input, int size);
void USART3_Init(void);
void startADC();
int checkTimeCorrectness(int time);
void configureADC(void);
//pinB4 for DIN, pinB5 for CS, pinB6 for CLK
extern void max7219_init();
extern void max7219_send(unsigned char address, unsigned char data);
unsigned char decodeTable[10] = {0xFE, 0xB0, 0xED, 0xF9, 0xB3, 0xDB, 0xDF, 0xF0, 0xFF, 0xFB};

#define X0 GPIOC->IDR & 0x00000008
#define X1 GPIOC->IDR & 0x00000004
#define X2 GPIOC->IDR & 0x00000002
#define X3 GPIOC->IDR & 0x00000001
#define Y0 (GPIOB->BSRR = GPIOB->BSRR | 0x00070008)
#define Y1 (GPIOB->BSRR = GPIOB->BSRR | 0x000B0004)
#define Y2 (GPIOB->BSRR = GPIOB->BSRR | 0x000D0002)
#define Y3 (GPIOB->BSRR = GPIOB->BSRR | 0x000E0001)

char currentTime[4] = "0000";
int hour=0, min=0, alarm1=8888, alarm2=8888;
int second10Count;
int isSetting=0, volumeType = 2;
int less = 40000, medium = 80000, large = 120000;

int keypadScanning(){
	//int k = 100;
	while(1){
		Y0;

		if(X0) return 1;
		if(X1) return 4;
		if(X2) return 7;
		if(X3){
			return 15;
		}

		Y1;
		if(X0) return 2;
		if(X1) return 5;
		if(X2) return 8;
		if(X3){
			return 0;
		}

		Y2;
		if(X0) return 3;
		if(X1) return 6;
		if(X2) return 9;
		if(X3){
			return 14;
		}

		Y3;
		if(X0){
			return 10;
		}
		if(X1){
			return 11;
		}
		if(X2){
			return 12;
		}
		if(X3){
			return 13;
		}


	}

	return 0;
}

int display(int data, int num_digs){

	if(num_digs > 8){
		return -1;
	}

	unsigned char datas[num_digs];
	for(int i=0; i<8; i++){
		datas[i] = 15;
	}

	int count = 0;
	for(int i=data; i>0; i=i/10){
		datas[count++] = i%10;
	}

	for(int i=count; i<num_digs; i++){
		if(i<3){
			datas[i] = 0;
		}else{
			datas[i] = 15;
		}

	}

	for(int i=0; i<4; i++){
		unsigned char temp = datas[i];
		datas[i] = datas[7-i];
		datas[7-i] = temp;
	}



	unsigned char k=8;
	for(int i=0; i<8; i++){
		if(i==5){
			max7219_send(k--, decodeTable[datas[i]]);
		}else{
			max7219_send(k--, datas[i]);
		}

	}
/*
	if(alarm1<2400 && alarm1>=0){
		max7219_send(8, '1');
	}else{
		max7219_send(8, (unsigned char)15);
	}

	if(alarm2<2400 && alarm2>=0){
		max7219_send(7, '2');
	}
	else{
		max7219_send(7, (unsigned char)15);
	}
*/
	return 0;
}

int displayA(int data[], int num_digs){

	int s=num_digs;
	for(int i=0; i<num_digs; i++){
		if(num_digs>2&&i==num_digs-3)
			max7219_send(s--, decodeTable[data[i]] & 0x7F);
		else
			max7219_send(s--, data[i]);
	}
	return 0;
}

void showTime(char time[]){
	int j=0;
	for(int i=4; i>0; i--){
		if(i!=3){
			max7219_send(i, currentTime[j++]);
		}else{
			max7219_send(i, decodeTable[currentTime[j++] - '0']);
		}
	}
	//display(s, 8);
}


void SystemClock_Config_source(){
	RCC->CR |= 0x00000100;
	while((RCC->CR & RCC_CR_HSIRDY) == 0);
	RCC->CFGR = 0xB5;
}

void SysTick_Handler(void){
	SystemClock_Config();
	second10Count++;
	if(second10Count == 6){
		second10Count = 0;
		if(min == 30){
			getCurrentTime(0);
		}else{
			min++;
			if(min==60){
				hour++;
				min = 0;
				if(hour > 23){
					hour = 0;
				}
			}
		}
		int t = hour*100 + min;
		if(isSetting==0)
			display(t, 8);

		if(alarm1 == t || alarm2 == t){
			if(volumeType == 1){
				openDoorLevelOne();
			}else{
				openDoor();
			}

			if(volumeType == 1) for(int i=0; i<10; i++);
			if(volumeType == 2) for(int i=0; i<20000; i++);
			if(volumeType == 3) for(int i=0; i<450000; i++);
			closeDoor();
		}
	}
	SystemClock_Config_source();
}


void assignTime(){
	hour = (currentTime[0] - '0')*10 + (currentTime[1] - '0');
	min = (currentTime[2] - '0')*10 + (currentTime[3] - '0');
}

void setUpAlarm1(){
	while(GPIOC->IDR&0x0000000F);
	int TIME;
	do{
		TIME = processKeyButton();
		if(TIME == 11){
			display((hour*100) + (min), 4);
			break;
		}
	}while(!checkTimeCorrectness(TIME));

	if(TIME != 11 && TIME != 12){
		alarm1 = TIME;
		int tt = (hour*100) + (min);
		display(tt, 4);
	}else if(TIME == 12){
		alarm1 = 8888;
		int tt = (hour*100) + (min);
		display(tt, 4);
	}

}

void setUpAlarm2(){
	while(GPIOC->IDR&0x0000000F);
	int TIME;
	do{
		TIME = processKeyButton();
		if(TIME == 12){
			display((hour*100) + (min), 4);
			break;
		}
	}while(!checkTimeCorrectness(TIME));

	if(TIME != 12 && TIME != 11){
		alarm2 = TIME;
		int tt = (hour*100) + (min);
		display(tt, 4);
	}else if(TIME == 11){
		alarm2 = 8888;
		int tt = (hour*100) + (min);
		display(tt, 4);
	}

}


int main(){
	GPIO_init();
	max7219_init();
	timer_init (timer);
	SystemClock_Config();
	USART3_Init();
	configureADC();
	startADC();
	GPIOB->BSRR = 0x0000000F;
	closeDoor();

	getCurrentTime(1);
	//showTime(currentTime);
	//assignTime();
	display(hour*100 + min ,4);

	uint32_t aa = 10000000;
	SystemClock_Config_source();
	SysTick_Config(aa);

	while(1){
		GPIOB->BSRR = 0x0000000F;
		int flag_keypad, k=0, flag_debounce=0, btnPressed = 1;
		flag_keypad=GPIOC->IDR&0x0000000F;									// PB8-11 to PC0-3 as receive of keyboard
		if(flag_keypad!=0){
			k=6000;
			while(GPIOC->IDR&0x0000000F){
				while(k!=0){
					flag_debounce=GPIOC->IDR&0x0000000F;
					k--;
				}
			}

			if(flag_debounce != 0){
				btnPressed = keypadScanning();
				if(btnPressed == 11 ){
					isSetting=1;
					display(alarm1, 4);
					setUpAlarm1();
					isSetting=0;
				}else if(btnPressed == 12 ){
					isSetting=1;
					display(alarm2, 4);
					setUpAlarm2();
					isSetting=0;
				}else if(btnPressed == 14){
					while(GPIOC->IDR&0x0000000F);
					isSetting = 1;
					reset_Max7219();
					max7219_send(1, (char)volumeType);
					int temptemp;
					do{
						temptemp = processKeyButton();
					}while(temptemp > 3 && temptemp <= 0);

					volumeType = temptemp;
					isSetting = 0;
					display(hour*100 + min, 4);

				}


			}
		}
	}
}

int processKeyButton(){

	int num[8] = {15,15,15,15,15,15,15,15};
	int currentState=0;
	while(1){
		GPIOB->BSRR = 0x0000000F;
		int flag_keypad, k=0, flag_debounce=0, btnPressed = 1;
		flag_keypad=GPIOC->IDR&0x0000000F;									// PB8-11 to PC0-3 as receive of keyboard

		if(flag_keypad!=0){
			k=4000;

			while(GPIOC->IDR&0x0000000F){
				while(k!=0){
					flag_debounce=GPIOC->IDR&0x0000000F;
					k--;
				}
			}
			if(flag_debounce != 0){
				if(currentState == 0) reset_Max7219();
				btnPressed = keypadScanning();
				if(btnPressed != 13 && currentState < 4 && btnPressed != 10 && btnPressed != 15){
					num[currentState++] = btnPressed;
				}else if(btnPressed == 10 && currentState > 0){
					num[currentState] = 15;
					if(currentState != 3){
						max7219_send(currentState--, 15);
					}else{
						max7219_send(currentState--, 0);
					}

				}else if(btnPressed == 13){
					// when pressed enter button "D"
					int final=0;
					for(int i=0; i<currentState; i++){
						final *= 10;
						final = final + num[i];
					}
					return final;
				}

				displayA(num, currentState);
			}

		}
	}
	while(GPIOC->IDR&0x0000000F);
}

void reset_Max7219(){
	for(int i=1; i<=8; i++){
		if(i!=3)
			max7219_send(i, 15);
		else
			max7219_send(i, 0);
	}
}

int checkTimeCorrectness(int time){
	if(time/100 > 23){
		return 0;
	}

	if(time%100 > 59){
		return 0;
	}

	return 1;
}



void getCurrentTime(int firstTime){

	int count=0,count1=0;
	while(1){
		while (!(USART3->ISR & USART_ISR_TXE));
		if(count==0)
		{
			UART_Transmit("start\n", 6);
			count1++;
		}
		while (!(USART3->ISR & USART_ISR_TXE));
		for(int i=0; i<10000; i++);
		if (USART3->ISR & USART_ISR_RXNE){
			currentTime[count]=(char)USART3->RDR;
			count++;
			UART_Transmit("1\n", 2);
			if(count==4){
				UART_Transmit(currentTime, 4);
				assignTime();
				break;
			}
		}


		if(count1>500 && firstTime == 1){
			int TIME;
			do{
				display(7777, 4);
				TIME = processKeyButton();
			}while(!checkTimeCorrectness(TIME));
			display(TIME, 8);
			hour = TIME/100;
			min = TIME%100;
			break;
		}else if(count1>500 && firstTime == 0){
			min+=1;
			break;
		}
	}

}

void timer_init (TIM_TypeDef *timer)
{
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
	// Sound freq = 4 MHz / (pres + 1) / 100
	// pres = 4 MHz / Sound freq / 100 - 1
	timer->PSC = (uint32_t) 799;
	timer->ARR = (uint32_t) 999;

	/* CH1 */
	timer->CCR1 = 25;
	timer->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1;

	timer->CR1 |= TIM_CR1_ARPE;
	timer->EGR = TIM_EGR_UG;
	timer->CCER |= TIM_CCER_CC1E;	/* CH1 */

}

void timer_start (TIM_TypeDef *timer)
{
	timer->CR1 |= TIM_CR1_CEN;
}

void timer_stop (TIM_TypeDef *timer)
{
	timer->CR1 &= ~TIM_CR1_CEN;
}

void openDoor(){
	timer_stop(timer);
	timer->CCR1 = 10;
	timer_start(timer);
	for(int i=0; i<10000000; i++);
	timer_stop(timer);
}

void closeDoor(){
	timer_stop(timer);
	timer->CCR1 = 95;
	timer_start(timer);
	for(int i=0; i<10000000; i++);
	timer_stop(timer);
}

void openDoorLevelOne(){
	timer_stop(timer);
	timer->CCR1 = 10;
	timer_start(timer);
	for(int i=0; i<1000000; i++);
	timer_stop(timer);
}

void GPIO_init(){
	RCC->AHB2ENR = RCC->AHB2ENR|0x7;

	// set up PC6 as output, keyboard input PC8~11
	GPIOC->MODER = 0xF300EF00;
	GPIOC->PUPDR = 0xF8AAAAAA;
	GPIOC->OSPEEDR = 0x55555555;
	GPIOC->AFR[0] = 0x02000002;

	//set up PB4,5,6,7 as max7219 output
	GPIOB->MODER = 0xFFAF5555;
	GPIOB->PUPDR = 0xFFAAAAAA;
	GPIOB->OSPEEDR = 0xAAAAAAAA;
	GPIOB->AFR[1] = (GPIOB->AFR[1] & ~(0xFF << 8)) | (0x77 << 8);		//set up AFRL as AF2 SUART_TX

	GPIOB->BSRR = 0x00000080;
}

void configureADC(void){
	RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
	GPIOB->ASCR |= 0x1;

	ADC123_COMMON->CCR |= 0b00000000000000010000010000000000;
	/************************ADC clock and some other config ends here********************************/
	/************************ADC main settings starts here********************************************/
	ADC1->CR &= ~ADC_CR_DEEPPWD; //Turn off the deep-power mode before the configuration
	ADC1->CR |= ADC_CR_ADVREGEN; //Turn on the voltage regulator
	while(!READ_BIT(ADC1->CR, ADC_CR_ADVREGEN)); //Polling until the ADCVERGEN is pulled up ot 1

	ADC1->CR |= 0x80000000; //Tell the ADC to do the calibration.
	while((ADC1->CR & 0x80000000) >> 31); //Polling until the calibration is done
	ADC1->CFGR &= ~ADC_CFGR_RES; // 12-bit resolution
	ADC1->CFGR |= 0x8;
	ADC1->CFGR &= ~ADC_CFGR_CONT; // Disable continuous conversion
	ADC1->CFGR &= ~ADC_CFGR_ALIGN; // Right align
				   //10987654321098765432109876543210
	ADC1->SQR1 |=  0b00000000000000000000001111000000; //We use only one ADC channel but problem is, which channel--> rank1 for channel15(pb0)
	//the higher sampling cycle can ensure the more detalied data but takes more process time, set12.5 will be fine, all takes 25 cycles
	ADC1->SMPR1 |= 0b00000000000000000000000000000010;
	//ADC conversion time may reference to manual p518
	ADC1->IER |= ADC_IER_EOCIE; //When conversion ends, do the interrupt (the end of conversion interrupt)
	/************************ADC main settings ends here********************************************/
}

void startADC()
{
	// TODO
    ADC1->CR |= ADC_CR_ADEN; //Turn on the ADC, write to enable the adc
    while(!READ_BIT(ADC1->ISR, ADC_ISR_ADRDY)); //Wait until ADC is ready for conversion
}

void USART3_Init(void) {
	// Enable clock for USART3
	RCC->APB1ENR1 |= RCC_APB1ENR1_USART3EN;
	// CR3
	USART3->CR1 &= ~(USART_CR1_M | USART_CR1_PS | USART_CR1_PCE | USART_CR1_TE | USART_CR1_RE | USART_CR1_OVER8);
	USART3->CR1 |=  USART_CR1_TE | USART_CR1_RE;
	// CR2
	USART3->CR2 &= ~USART_CR2_STOP; // 1-bit stop
	// CR3
	USART3->CR3 &= ~(USART_CR3_RTSE | USART_CR3_CTSE | USART_CR3_ONEBIT);// none hwflowctl
	USART3->BRR &= ~(0xFF);
	USART3->BRR |= 40000000L/9600L;
	// In asynchronous mode, the following bits must be kept cleared:
	//- LINEN and CLKEN bits in the USART_CR2 register,
	//- SCEN, HDSEL and IREN bits in the USART_CR3 register.
	USART3->CR2 &= ~(USART_CR2_LINEN | USART_CR2_CLKEN);
	USART3->CR3 &= ~(USART_CR3_SCEN | USART_CR3_HDSEL | USART_CR3_IREN);
	// Enable UART
	USART3->CR1 |= (USART_CR1_UE);
}

void UART_Transmit(char *input, int size) {
	if(SysTick->CTRL & SysTick_CTRL_TICKINT_Msk){
		while (!(USART3->ISR & USART_ISR_TXE));
		USART3->TDR = (uint16_t)'\r';
	}
	char *arr = input;
	for(int i=0; i<size; i++){
		// Wait to be ready, buffer empty
		while (!(USART3->ISR & USART_ISR_TXE));
		// Send data
		USART3->TDR = (uint16_t)(*arr++);
	}

}


void SystemClock_Config(void){
	//select HSI clock
	RCC->CFGR &= 0xFFFFFFFC;
	RCC->CFGR |= 0x1;
	RCC->CR |= RCC_CR_HSION;
	while((RCC->CR & RCC_CR_HSIRDY) == 0);//wait for HSI ready
	RCC->CR &= ~RCC_CR_PLLON;
	while((RCC->CR & RCC_CR_PLLRDY) == 1);//wait for PLL turn off
	//select PLL clock
	RCC->CFGR &= 0xFFFFFFFC;
	RCC->CFGR |= 0x3;
	//Mask PLLR PLLN PLLM
	RCC->PLLCFGR &= 0xF9EE808C;
	//PLLM (4~16)MHz = 2
	RCC->PLLCFGR |= (1 << 4);
	//PLLN (64~344)MHz
	RCC->PLLCFGR |= (10 << 8);
	//PLLR = 2 <80MHz
	RCC->PLLCFGR |= (0 << 25);
	//PLLSRC = HSI16
	RCC->PLLCFGR |= 0x00000002;
	//HPRE
	RCC->CFGR &= 0xFFFFFF0F;
	RCC->CFGR |= 0;//SYSCLK
	RCC->CR |= RCC_CR_PLLON;
	RCC->PLLCFGR |= (1 << 24);
	while((RCC->CR & RCC_CR_PLLRDY) == 0);
}


