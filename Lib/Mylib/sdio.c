#include "sdio.h"

volatile SDCard_TypeDef SDCard;
volatile SD_Status_TypeDef SDStatus;
volatile uint32_t response[4];   //Для хранения ответа от карты
volatile uint8_t transmit;       //Флаг запущенной передачи данных в SDIO
volatile uint8_t state=0;        //Для хранения состояния карты
volatile uint8_t multiblock=0;   //Используется в прерывании SDIO, чтоб слать команду STOP
volatile uint32_t error_flag=0;
volatile uint32_t trials;
volatile uint32_t sta_reg=0;

volatile uint8_t __attribute__ ((aligned (4))) buf_copy[16*1024];


void SD_check_status(SD_Status_TypeDef* SDStatus,uint32_t* reg){
	SDStatus->ake_seq_error     = (*reg & (1 << 3)) ? 1 : 0;
	SDStatus->app_cmd           = (*reg & (1 << 5)) ? 1 : 0;
	SDStatus->ready_for_data    = (*reg & (1 << 8)) ? 1 : 0;
	SDStatus->current_state     = (uint8_t)((*reg & (0x0F << 9)) >> 9);
	SDStatus->erase_reset       = (*reg & (1 << 13)) ? 1 : 0;
	SDStatus->card_ecc_disabled = (*reg & (1 << 14)) ? 1 : 0;
	SDStatus->wp_erase_skip     = (*reg & (1 << 15)) ? 1 : 0;
	SDStatus->csd_overwrite     = (*reg & (1 << 16)) ? 1 : 0;
	SDStatus->error             = (*reg & (1 << 19)) ? 1 : 0;
	SDStatus->cc_error          = (*reg & (1 << 20)) ? 1 : 0;
	SDStatus->card_ecc_failed   = (*reg & (1 << 21)) ? 1 : 0;
	SDStatus->illegal_command   = (*reg & (1 << 22)) ? 1 : 0;
	SDStatus->com_crc_error     = (*reg & (1 << 23)) ? 1 : 0;
	SDStatus->lock_unlock_failed= (*reg & (1 << 24)) ? 1 : 0;
	SDStatus->card_is_locked    = (*reg & (1 << 25)) ? 1 : 0;
	SDStatus->wp_violation      = (*reg & (1 << 26)) ? 1 : 0;
	SDStatus->erase_param       = (*reg & (1 << 27)) ? 1 : 0;
	SDStatus->erase_seq_error   = (*reg & (1 << 28)) ? 1 : 0;
	SDStatus->block_len_error   = (*reg & (1 << 29)) ? 1 : 0;
	SDStatus->address_error     = (*reg & (1 << 30)) ? 1 : 0;
	SDStatus->out_of_range      = (*reg & (1U << 31)) ? 1 : 0;
};

uint8_t SD_Cmd(uint8_t cmd, uint32_t arg, uint16_t response_type, uint32_t *response){
	SDIO->ICR = SDIO_ICR_CMD_FLAGS;
	SDIO->ARG = arg;
	SDIO->CMD = (uint32_t)(response_type | cmd); 
	SDIO->CMD |= SDIO_CMD_CPSMEN;

	while( (SDIO->STA & SDIO_STA_CMD_FLAGS) == 0){asm("nop");};

	if (response_type != SDIO_RESP_NONE) {
		response[0] =	SDIO->RESP1;
		response[1] =	SDIO->RESP2;
		response[2] =	SDIO->RESP3;
		response[3] =	SDIO->RESP4;
	}
	
	if (SDIO->STA & SDIO_STA_CTIMEOUT) {
		return 2;
	}
	if (SDIO->STA & SDIO_STA_CCRCFAIL) {
		return 3;  
	}
	return 0;
}

//#pragma GCC push_options
//#pragma GCC optimize ("O0")
uint32_t SD_transfer(uint8_t *buf, uint32_t blk, uint32_t cnt, uint32_t dir){
    uint32_t trials;
	uint8_t cmd=0;
	uint8_t *ptr = buf;
	    
    trials=SDIO_DATA_TIMEOUT;
	while (transmit && trials--) {};
	if(!trials) {
		return 1;
		}

   	state=0;
    while(state != 4){ //Дождаться когда карта будет в режиме tran (4)
	 	SD_Cmd(SD_CMD13, SDCard.RCA ,SDIO_RESP_SHORT,(uint32_t*)response); 
	 	SD_check_status((SD_Status_TypeDef*)&SDStatus,(uint32_t*)&response[0]);
	 	state=SDStatus.current_state;

	 	if((state == 5) || (state == 6)) SD_Cmd(SD_CMD12, 0, SDIO_RESP_SHORT,(uint32_t*)response);
	 };

      
	//Выключить DMA
	DMA2->IFCR=DMA_S4_CLEAR;
	DMA2_Channel4->CCR=0;
	DMA2->IFCR=DMA_S4_CLEAR;
	DMA2_Channel4->CCR=DMA_SDIO_CR;

	multiblock = (cnt == 1) ? 0 : 1;
	if (dir==UM2SD){ //Запись
				if(((uint32_t)buf % 4) != 0){
					DEBUG("Buffer not aligned");
					memcpy((uint8_t*)buf_copy,buf,cnt*512);
					ptr=(uint8_t*)buf_copy;    
				};
				DMA2_Channel4->CCR|=(0x01 << DMA_CCR_DIR_Pos);
				cmd=(cnt == 1)? SD_CMD24 : SD_CMD25;
			} 
	else if (dir==SD2UM){ //Чтение
				cmd=(cnt == 1)? SD_CMD17 : SD_CMD18;
				if(((uint32_t)buf % 4) != 0){
					ptr=(uint8_t*)buf_copy;
				};
			};
   	
    DMA2_Channel4->CMAR=(uint32_t)ptr;    //Memory address	
	DMA2_Channel4->CPAR=(uint32_t)&(SDIO->FIFO);  //SDIO FIFO Address 
	DMA2_Channel4->CNDTR=cnt*512/4;  
	
	transmit=1;
	error_flag=0;
	
	__disable_irq();
	SD_Cmd(cmd, blk, SDIO_RESP_SHORT, (uint32_t*)response);

	SDIO->ICR=SDIO_ICR_DATA_FLAGS;
	SDIO->DTIMER=(uint32_t)0x6BDD00;
	SDIO->DLEN=cnt*512;    //Количество байт (блок 512 байт)
	SDIO->DCTRL= SDIO_DCTRL | (dir & SDIO_DCTRL_DTDIR);  //Direction. 0=Controller to card, 1=Card to Controller

	DMA2_Channel4->CCR |= DMA_CCR_EN;
	SDIO->DCTRL|=1; //DPSM is enabled
	__enable_irq();

	while((SDIO->STA & (SDIO_STA_DATAEND|SDIO_STA_ERRORS)) == 0){__asm volatile ("nop");};
	
	if(SDIO->STA & SDIO_STA_ERRORS){
		error_flag=SDIO->STA;
		transmit=0;
		SDIO->ICR = SDIO_ICR_STATIC;
		DMA2_Channel4->CCR = 0;
		DMA2->IFCR = DMA_S4_CLEAR;
		return error_flag;
	}
	
	if(dir==SD2UM) { //Read
		while (DMA2_Channel4->CCR & DMA_CCR_EN) {
			if(SDIO->STA & SDIO_STA_ERRORS)	{
				transmit=0;
				return 99;
			}
				DMA2_Channel4->CCR = 0;
				DMA2->IFCR = DMA_S4_CLEAR;
			};
	
		if(((uint32_t)buf % 4) != 0){	
			memcpy(buf,(uint8_t*)buf_copy,cnt*512);
		}
	};

  

	if(multiblock > 0) SD_Cmd(SD_CMD12, 0, SDIO_RESP_SHORT, (uint32_t*)response);
	transmit=0;		
	DMA2->IFCR = DMA_S4_CLEAR;
    SDIO->ICR=SDIO_ICR_STATIC;
	return 0;	
};
//#pragma GCC pop_options



uint8_t SD_Init(void) {
	volatile uint32_t trials = 0x0000FFFF;
	uint32_t tempreg;   //Для временного хранения регистров
	uint8_t result = 0;
	
	result = SD_Cmd(SD_CMD0,0x00,SDIO_RESP_NONE,(uint32_t*)response);  //NORESP
	if (result != 0){
		ERROR("CMD0: %d",result);
		return 1;
	};
	
	result = SD_Cmd(SD_CMD8,SD_CHECK_PATTERN,SDIO_RESP_SHORT,(uint32_t*)response);  //R7
	if (result != 0) {
		ERROR("CMD8: %d",result);
		return 8;
	};
	if (response[0] != SD_CHECK_PATTERN) {
		ERROR("CMD8 check");	
		return 8;
	};

	trials = 0x0000FFFF;
	while (--trials) {
			SD_Cmd(SD_CMD55, 0 ,SDIO_RESP_SHORT,(uint32_t*)response); // CMD55 with RCA 0   R1
			SD_check_status((SD_Status_TypeDef*)&SDStatus,(uint32_t*)&response[0]);
			SD_Cmd(SD_ACMD41,(1<<20|1<<30),SDIO_RESP_SHORT,(uint32_t*)response);
			if (response[0] & SDIO_ACMD41_CHECK) break;
		}
	if (!trials) {
		ERROR("CMD41 check");	
		return 41; 
	};

	result = SD_Cmd(SD_CMD2,0x00,SDIO_RESP_LONG,(uint32_t*)response); //CMD2 CID R2
	if (result != 0) {
		ERROR("CMD2: %d",result);
		return 2;
	};
		
	SDCard.CID[0]=response[0];
	SDCard.CID[1]=response[1];
	SDCard.CID[2]=response[2];
	SDCard.CID[3]=response[3];
	
	result = SD_Cmd(SD_CMD3,0x00,SDIO_RESP_SHORT,(uint32_t*)response); //CMD3 RCA R6
	if (result != 0){
		ERROR("CMD3: %d",result);
		return 3;		
	};
	
	SDCard.RCA=( response[0] & (0xFFFF0000) );

	result = SD_Cmd(SD_CMD9,SDCard.RCA,SDIO_RESP_LONG,(uint32_t*)response); //CMD9 СSD  R2
	if (result != 0) {
		ERROR("CMD9: %d",result);
		return 9;
	}		
	
	SDCard.CSD[0]=response[0];
	SDCard.CSD[1]=response[1];
	SDCard.CSD[2]=response[2];
	SDCard.CSD[3]=response[3];
	
	SD_parse_CSD((uint32_t*)SDCard.CSD);	
		
	result = SD_Cmd(SD_CMD7,SDCard.RCA,SDIO_RESP_SHORT,(uint32_t*)response); //CMD7 tran   R1b
	SD_check_status((SD_Status_TypeDef*)&SDStatus,(uint32_t*)&response[0]);
	if (result != 0) {
		ERROR("CMD7: %d",result);
		return 7;		
	}

	state=0;
	//Дождаться когда карта будет в режиме tran (4)
	while(state != 4){
		SD_Cmd(SD_CMD13, SDCard.RCA ,SDIO_RESP_SHORT,(uint32_t*)response); 
		SD_check_status((SD_Status_TypeDef*)&SDStatus,(uint32_t*)&response[0]);
		state=SDStatus.current_state;
	};

  #if(SDIO_4BIT_Mode == 1)
		result = SD_Cmd(SD_CMD55, SDCard.RCA ,SDIO_RESP_SHORT,(uint32_t*)response); //CMD55 with RCA
		SD_check_status((SD_Status_TypeDef*)&SDStatus,(uint32_t*)&response[0]);
		if (result != 0)return 55;
	
		result = SD_Cmd(6, 0x02, SDIO_RESP_SHORT,(uint32_t*)response);      //Шлем ACMD6 c аргументом 0x02, установив 4-битный режим
		if (result != 0) {return 6;};
		if (response[0] != 0x920) {return 5;};    //Убеждаемся, что карта находится в готовности работать с трансфером

		tempreg=((0x01)<<SDIO_CLKCR_WIDBUS_Pos)| SDIO_CLKCR_CLKEN; 
		SDIO->CLKCR=tempreg;	

		#if (SDIO_HIGH_SPEED != 0)
			SD_HighSpeed();
			tempreg=((0x01)<<SDIO_CLKCR_WIDBUS_Pos)| SDIO_CLKCR_BYPASS | SDIO_CLKCR_CLKEN; 
			SDIO->CLKCR=tempreg;	
		#endif
#else
		tempreg=0;  
		tempreg=SDIO_CLKCR_CLKEN; 
		SDIO->CLKCR=tempreg;	
#endif

	DEBUG("SDINIT: ok");
	return 0;
};



void SDIO_gpio_init(void){
	/*
	SDIO:
	D0 - PC8
	D1 - PC9
	D2 - PC10
	D3 - PC11
	CK - PC12
	CMD- PD2

	Alternate function push-pull
*/
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN | RCC_APB2ENR_AFIOEN;

    GPIOC->CRH &= ~( (GPIO_CRH_MODE8 | GPIO_CRH_MODE9 | GPIO_CRH_MODE10 | GPIO_CRH_MODE11 | GPIO_CRH_MODE12) |\
					 (GPIO_CRH_CNF8  | GPIO_CRH_CNF9  | GPIO_CRH_CNF10  | GPIO_CRH_CNF11  | GPIO_CRH_CNF12) );
    
	GPIOC->CRH |= ( (GPIO_CRH_MODE8 | GPIO_CRH_MODE9 | GPIO_CRH_MODE10 | GPIO_CRH_MODE11 | GPIO_CRH_MODE12)|\
					(GPIO_CRH_CNF8_1| GPIO_CRH_CNF9_1| GPIO_CRH_CNF10_1| GPIO_CRH_CNF11_1| GPIO_CRH_CNF12_1) );
    
	GPIOD->CRL &= ~(GPIO_CRL_MODE2 | GPIO_CRL_CNF2);
	GPIOD->CRL |= GPIO_CRL_MODE2 | GPIO_CRL_CNF2_1;

	//SDIO & DMA
	RCC->AHBENR |= RCC_AHBENR_SDIOEN | RCC_AHBENR_DMA2EN;

	SDIO->CLKCR = SDIO_CLKCR_CLKEN | (58 << SDIO_CLKCR_CLKDIV_Pos); 
	SDIO->POWER |= SDIO_POWER_PWRCTRL;
}



// Issue SWITCH_FUNC command (CMD6)
// input:
//   argument - 32-bit argument for the command, refer to SD specification for description
//   resp - pointer to the buffer for response (should be 64 bytes long)
// return: SDResult
// note: command response is a contents of the CCCR register (512bit or 64bytes)
uint8_t SD_CmdSwitch(uint32_t argument, uint8_t *resp) {
	uint32_t *ptr = (uint32_t *)resp;
	uint8_t res = 0;

	// SD specification says that response size is always 512bits,
	// thus there is no need to set block size before issuing CMD6
	// Clear the data flags
	SDIO->ICR = SDIO_ICR_STATIC;

	SDIO->DTIMER = SDIO_DATA_R_TIMEOUT;
	SDIO->DLEN = 64; 	// Data length in bytes
	// Data transfer:
	//   transfer mode: block
	//   direction: to card
	//   DMA: enabled
	//   block size: 2^6 = 64 bytes
	//   DPSM: enabled
	SDIO->DCTRL = SDIO_DCTRL_DTDIR | (6 << 4) | SDIO_DCTRL_DTEN;

	// Send SWITCH_FUNCTION command
	// Argument:
	//   [31]: MODE: 1 for switch, 0 for check
	//   [30:24]: reserved, all should be '0'
	//   [23:20]: GRP6 - reserved
	//   [19:16]: GRP5 - reserved
	//   [15:12]: GRP4 - power limit
	//   [11:08]: GRP3 - driver strength
	//   [07:04]: GRP2 - command system
	//   [03:00]: GRP1 - access mode (a.k.a. bus speed mode)
	//   Values for groups 6..2:
	//     0xF: no influence
	//     0x0: default
	res=SD_Cmd(SD_CMD_SWITCH_FUNC, argument, SDIO_RESP_SHORT, (uint32_t*)response); // CMD6
	if (res != 0) {	return res; }

	// Read the CCCR register value
	while (!(SDIO->STA & (SDIO_STA_RXOVERR | SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_DBCKEND | SDIO_STA_STBITERR))) {
		// The receive FIFO is half full, there are at least 8 words in it
		if (SDIO->STA & SDIO_STA_RXFIFOHF) {
			*ptr++ = SDIO->FIFO;
			*ptr++ = SDIO->FIFO;
			*ptr++ = SDIO->FIFO;
			*ptr++ = SDIO->FIFO;
			*ptr++ = SDIO->FIFO;
			*ptr++ = SDIO->FIFO;
			*ptr++ = SDIO->FIFO;
			*ptr++ = SDIO->FIFO;
		}
	}

	// Check for errors
	if (SDIO->STA & SDIO_XFER_ERROR_FLAGS) return	1;
	
	// Read the data remnant from the SDIO FIFO (should not be, but just in case)
	while (SDIO->STA & SDIO_STA_RXDAVL) *ptr++ = SDIO->FIFO;
	
	// Clear the static SDIO flags
	SDIO->ICR = SDIO_ICR_STATIC;

	return res;
}

uint8_t SD_HighSpeed(void) {
	uint8_t CCCR[64];
	uint8_t cmd_res = 0;

	// Check if the card supports HS mode
	cmd_res = SD_CmdSwitch(
				(0x0U << 31) | // MODE: check
				(0xFU << 20) | // GRP6: no influence
				(0xFU << 16) | // GRP5: no influence
				(0xFU << 12) | // GRP4: no influence
				(0xFU <<  8) | // GRP3: no influence
				(0xFU <<  4) | // GRP2: default
				(0x1U <<  0),  // GRP1: high speed
				CCCR
			);
	if (cmd_res != 0) {return cmd_res;}

	// Check SHS bit from CCCR
	if ( (CCCR[63 - (400 / 8)] & 0x01) != 0x01) {
		return 2;
	}

	// Ask the card to switch to HS mode
	cmd_res = SD_CmdSwitch(
				(0x1U << 31) | // MODE: switch
				(0xFU << 20) | // GRP6: no influence
				(0xFU << 16) | // GRP5: no influence
				(0xFU << 12) | // GRP4: no influence
				(0xFU <<  8) | // GRP3: no influence
				(0xFU <<  4) | // GRP2: default
				(0x1U <<  0),  // GRP1: high speed
				CCCR
			);
	if (cmd_res != 0) { return 3;}

	// Note: the SD specification says "card shall switch speed mode within 8 clocks
	// after the end bit of the corresponding response"
	// Apparently, this is the reason why some SD cards demand a delay between
	// the request for mode switching and the possibility to continue communication
	
	return 0;
}

uint8_t sd_get_cardsize(void){
	if( ((SDCard.Capacity/1000) > 3000) && ((SDCard.Capacity/1000) < 5000) ) return 4;
	if( ((SDCard.Capacity/1000) > 7000) && ((SDCard.Capacity/1000) < 9000) ) return 8;
	if( ((SDCard.Capacity/1000) > 14000) && ((SDCard.Capacity/1000) < 17000) ) return 16;
	if( ((SDCard.Capacity/1000) > 31000) && ((SDCard.Capacity/1000) < 33000) ) return 32;
	return 0;
};

void SD_parse_CSD(uint32_t* reg){
	uint32_t tmp;
	//Версия CSD регистра
	if(reg[0] & (11U << 30)){
		SDCard.CSDVer=2;
	}else{
		SDCard.CSDVer=1;
	};
	//Размер карты и количество блоков	
	tmp= (reg[2] >> 16) & 0xFFFF;
	tmp |= (reg[1] & 0x3F) << 16;
	SDCard.BlockCount=tmp*1000;
	SDCard.Capacity=(tmp+1)*512;
};

uint32_t SD_get_block_count(void){
	return SDCard.BlockCount;
};