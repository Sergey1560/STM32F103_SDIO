#include "rcc.h"


void RCC_init(void){
	RCC->CR |= ((uint32_t)RCC_CR_HSEON);
	while(!(RCC->CR & RCC_CR_HSERDY)){};

	FLASH->ACR |= FLASH_ACR_PRFTBE;
	FLASH->ACR=(FLASH->ACR&((uint32_t)~FLASH_ACR_LATENCY))|((uint32_t)FLASH_ACR_LATENCY_2);

	RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;
 	RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;
 	RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;

	RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
 	RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9);
 	//RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL6);

	RCC->CR |= RCC_CR_PLLON;
	while((RCC->CR & RCC_CR_PLLRDY) == 0) {};

	RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
	RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;
	while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08) {};

	SystemCoreClockUpdate();
	INFO("System Clock: %ld", SystemCoreClock);
}

