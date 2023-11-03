/************************************************************************
* 5 semestre - Eng. da Computao - Insper
*
* 2021 - Exemplo com HC05 com RTOS
*
*/

#include <asf.h>
#include "conf_board.h"
#include <string.h>

/************************************************************************/
/* defines                                                              */
/************************************************************************/

// LEDs
#define LED_PIO      PIOC
#define LED_PIO_ID   ID_PIOC
#define LED_IDX      8
#define LED_IDX_MASK (1 << LED_IDX)

// Botão
#define BUT_PIO      PIOA
#define BUT_PIO_ID   ID_PIOA
#define BUT_IDX      11
#define BUT_IDX_MASK (1 << BUT_IDX)

#define BUT1_PIO      PIOB
#define BUT1_PIO_ID   ID_PIOB
#define BUT1_IDX      3
#define BUT1_IDX_MASK (1 << BUT1_IDX)

#define BUT2_PIO      PIOA
#define BUT2_PIO_ID   ID_PIOA
#define BUT2_IDX      0
#define BUT2_IDX_MASK (1 << BUT2_IDX)

#define BUT3_PIO      PIOA
#define BUT3_PIO_ID   ID_PIOA
#define BUT3_IDX      6
#define BUT3_IDX_MASK (1 << BUT3_IDX)

#define AFEC_POT AFEC0
#define AFEC_POT_ID ID_AFEC0
#define AFEC_POT_CHANNEL 0 // Canal do pino PD30

// usart (bluetooth ou serial)
// Descomente para enviar dados
// pela serial debug

#define DEBUG_SERIAL

#ifdef DEBUG_SERIAL
#define USART_COM USART1
#define USART_COM_ID ID_USART1
#else
#define USART_COM USART0
#define USART_COM_ID ID_USART0
#endif

/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/

#define TASK_BLUETOOTH_STACK_SIZE            (4096/sizeof(portSTACK_TYPE))
#define TASK_BLUETOOTH_STACK_PRIORITY        (tskIDLE_PRIORITY)
TimerHandle_t xTimer;
QueueHandle_t xQueueButFreq;
QueueHandle_t xQueueProc;
int flagHandshake = 0;
volatile int flagOn = 0;
typedef struct {
	uint value;
} adcData;

/************************************************************************/
/* prototypes                                                           */
/************************************************************************/

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);
static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel, afec_callback_t callback);

/************************************************************************/
/* constants                                                            */
/************************************************************************/

/************************************************************************/
/* variaveis globais                                                    */
/************************************************************************/

/************************************************************************/
/* RTOS application HOOK                                                */
/************************************************************************/

/* Called if stack overflow during execution */

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	/* If the parameters have been corrupted then inspect pxCurrentTCB to
	* identify which task has overflowed its stack.
	*/
	for (;;) {
	}
}

/* This function is called by FreeRTOS idle task */
extern void vApplicationIdleHook(void) {
	pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
}

/* This function is called by FreeRTOS each tick */
extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	/* Force an assert. */
	configASSERT( ( volatile void * ) NULL );
}


/************************************************************************/
/* handlers / callbacks                                                 */
/************************************************************************/
void but_callback(void) {
	// BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	// xSemaphoreGiveFromISR(xSemaphoreBut, &xHigherPriorityTaskWoken);
	// verify if is fall or rise edge
	uint increment = 0;
	if(pio_get(BUT_PIO, PIO_INPUT, BUT_IDX_MASK) == 0) {
		// rise edge
		increment = 1;
		} else {
		// fall edge
		increment = 0;
	}
	if(flagOn == 1) {
		xQueueSendFromISR(xQueueButFreq, (void *)&increment, 10);
	}
}

void but1_callback(void) {

	uint increment = 0;
	if(pio_get(BUT1_PIO, PIO_INPUT, BUT1_IDX_MASK) == 0) {
		// rise edge
		increment = 7;
		} else {
		// fall edge
	}
	if(flagOn == 1) {
		xQueueSendFromISR(xQueueButFreq, (void *)&increment, 10);
	}
}

void but2_callback(void) {

	uint increment = 0;
	if(pio_get(BUT2_PIO, PIO_INPUT, BUT2_IDX_MASK) == 0) {
		// rise edge
		increment = 9;
		} else {
		// fall edge
	}
	if(flagOn == 1) {
		xQueueSendFromISR(xQueueButFreq, (void *)&increment, 10);
	}
}

void but3_callback(void) {

	if(flagOn == 0) {
		flagOn = 1;
		pio_set(LED2_PIO, LED2_IDX_MASK);
		} else {
		flagOn = 0;
		pio_clear(LED2_PIO, LED2_IDX_MASK);
	}
}

static void AFEC_pot_callback(void) {
	adcData adc;
	adc.value = afec_channel_get_value(AFEC_POT, AFEC_POT_CHANNEL);
	BaseType_t xHigherPriorityTaskWoken = pdTRUE;
	if(flagOn == 1) {
		xQueueSendFromISR(xQueueProc, &adc, &xHigherPriorityTaskWoken);
	}
}
/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

void io_init(void) {

	// Ativa PIOs
	pmc_enable_periph_clk(LED_PIO_ID);
	pmc_enable_periph_clk(BUT_PIO_ID);
	pmc_enable_periph_clk(BUT1_PIO_ID);
	pmc_enable_periph_clk(BUT2_PIO_ID);
	pmc_enable_periph_clk(BUT3_PIO_ID);

	// Configura Pinos
	pio_configure(LED_PIO, PIO_OUTPUT_0, LED_IDX_MASK, PIO_DEFAULT | PIO_DEBOUNCE);
	pio_configure(BUT_PIO, PIO_INPUT, BUT_IDX_MASK, PIO_PULLUP);
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_configure(BUT2_PIO, PIO_INPUT, BUT2_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_configure(BUT3_PIO, PIO_INPUT, BUT3_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_handler_set(BUT_PIO,
	BUT_PIO_ID,
	BUT_IDX_MASK,
	PIO_IT_EDGE,
	but_callback);

	pio_handler_set(BUT1_PIO,
	BUT1_PIO_ID,
	BUT1_IDX_MASK,
	PIO_IT_EDGE,
	but1_callback);

	pio_handler_set(BUT2_PIO,
	BUT2_PIO_ID,
	BUT2_IDX_MASK,
	PIO_IT_FALL_EDGE,
	but2_callback);

	pio_handler_set(BUT3_PIO,
	BUT3_PIO_ID,
	BUT3_IDX_MASK,
	PIO_IT_FALL_EDGE,
	but3_callback);

	// Ativa interrupção e limpa primeira IRQ gerada na ativacao
	pio_enable_interrupt(BUT_PIO, BUT_IDX_MASK);
	pio_get_interrupt_status(BUT_PIO);
	
	pio_enable_interrupt(BUT1_PIO, BUT1_IDX_MASK);
	pio_get_interrupt_status(BUT1_PIO);

	pio_enable_interrupt(BUT2_PIO, BUT2_IDX_MASK);
	pio_get_interrupt_status(BUT2_PIO);

	pio_enable_interrupt(BUT3_PIO, BUT3_IDX_MASK);
	pio_get_interrupt_status(BUT3_PIO);

	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 4);

	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_SetPriority(BUT1_PIO_ID, 4);

	NVIC_EnableIRQ(BUT2_PIO_ID);
	NVIC_SetPriority(BUT2_PIO_ID, 4);

	NVIC_EnableIRQ(BUT3_PIO_ID);
	NVIC_SetPriority(BUT3_PIO_ID, 4);
}
static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
		#if (defined CONF_UART_CHAR_LENGTH)
		.charlength = CONF_UART_CHAR_LENGTH,
		#endif
		.paritytype = CONF_UART_PARITY,
		#if (defined CONF_UART_STOP_BITS)
		.stopbits = CONF_UART_STOP_BITS,
		#endif
	};

	/* Configure console UART. */
	stdio_serial_init(CONF_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	#if defined(__GNUC__)
	setbuf(stdout, NULL);
	#else
	/* Already the case in IAR's Normal DLIB default configuration: printf()
	* emits one character at a time.
	*/
	#endif
}

uint32_t usart_puts(uint8_t *pstring) {
	uint32_t i ;

	while(*(pstring + i))
	if(uart_is_tx_empty(USART_COM))
	usart_serial_putchar(USART_COM, *(pstring+i++));
}

void usart_put_string(Usart *usart, char str[]) {
	usart_serial_write_packet(usart, str, strlen(str));
}

int usart_get_string(Usart *usart, char buffer[], int bufferlen, uint timeout_ms) {
	uint timecounter = timeout_ms;
	uint32_t rx;
	uint32_t counter = 0;

	while( (timecounter > 0) && (counter < bufferlen - 1)) {
		if(usart_read(usart, &rx) == 0) {
			buffer[counter++] = rx;
		}
		else{
			timecounter--;
			vTaskDelay(1);
		}
	}
	buffer[counter] = 0x00;
	return counter;
}

void usart_send_command(Usart *usart, char buffer_rx[], int bufferlen,
char buffer_tx[], int timeout) {
	usart_put_string(usart, buffer_tx);
	usart_get_string(usart, buffer_rx, bufferlen, timeout);
}

void config_usart0(void) {
	sysclk_enable_peripheral_clock(ID_USART0);
	usart_serial_options_t config;
	config.baudrate = 9600;
	config.charlength = US_MR_CHRL_8_BIT;
	config.paritytype = US_MR_PAR_NO;
	config.stopbits = false;
	usart_serial_init(USART0, &config);
	usart_enable_tx(USART0);
	usart_enable_rx(USART0);

	// RX - PB0  TX - PB1
	pio_configure(PIOB, PIO_PERIPH_C, (1 << 0), PIO_DEFAULT);
	pio_configure(PIOB, PIO_PERIPH_C, (1 << 1), PIO_DEFAULT);
}

int hc05_init(void) {
	char buffer_rx[128];
	usart_send_command(USART_COM, buffer_rx, 1000, "AT", 100);
	vTaskDelay( 500 / portTICK_PERIOD_MS);
	usart_send_command(USART_COM, buffer_rx, 1000, "AT", 100);
	vTaskDelay( 500 / portTICK_PERIOD_MS);
	usart_send_command(USART_COM, buffer_rx, 1000, "AT+NAMEFernanda", 100);
	vTaskDelay( 500 / portTICK_PERIOD_MS);
	usart_send_command(USART_COM, buffer_rx, 1000, "AT", 100);
	vTaskDelay( 500 / portTICK_PERIOD_MS);
	usart_send_command(USART_COM, buffer_rx, 1000, "AT+PIN0000", 100);
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/
//fer
void vTimerCallback(TimerHandle_t xTimer) {
	/* Selecina canal e inicializa conversão */
	afec_channel_enable(AFEC_POT, AFEC_POT_CHANNEL);
	afec_start_software_conversion(AFEC_POT);
}

void task_comunicacao(void){
	char readChar;
	while(1){
		if (usart_read(USART_COM, &readChar)==1){
			if (readChar == 'W'){
				flagHandshake = 1;
			}
			
		}
	}
}

void task_bluetooth(void) {
	printf("Task Bluetooth started \n");
	
	printf("Inicializando HC05 \n");
	config_usart0();
	hc05_init();

	// configura LEDs e Botões
	io_init();

	config_AFEC_pot(AFEC_POT, AFEC_POT_ID, AFEC_POT_CHANNEL, AFEC_pot_callback);

	xTimer = xTimerCreate("Timer",
							100,
							pdTRUE,
							(void *)0,
							vTimerCallback);
	xTimerStart(xTimer, 0);

	char button1 = '0';
	char eof = 'X';
	int recived = 0;
	int flagPulo = 0;
	char readChar;
    // receber dados da fila
  	adcData adc;
	// Task não deve retornar.
	pio_clear(LED2_PIO, LED2_IDX_MASK);
	while(1) {
		if(flagOn == 1){
			if(flagHandshake == 0){
				button1 = 'W';
				while(!usart_is_tx_ready(USART_COM)) {
					vTaskDelay(10 / portTICK_PERIOD_MS);
				}
				usart_write(USART_COM, button1);
				
				// envia fim de pacote
				while(!usart_is_tx_ready(USART_COM)) {
					vTaskDelay(10 / portTICK_PERIOD_MS);
				}
				usart_write(USART_COM, eof);

				// dorme por 10 ms
				vTaskDelay(10 / portTICK_PERIOD_MS);
				
			} else {
				//atualiza valor do botão
				if( xQueueReceive( xQueueButFreq, &recived, 10)) {
					// printf("\n");
					// printf("Button 1: %d \n", recived);
					// printf("\n");
					if(recived == 0) {
						button1 = '0';
						flagPulo = 0;
					}
					else if(recived == 1){
						pio_set(LED_PIO, LED_IDX_MASK);
						vTaskDelay(50);
						pio_clear(LED_PIO, LED_IDX_MASK);
						button1 = '1';
						flagPulo = 1;
					} else if(recived == 7){
						button1 = '7';
						flagPulo = 0;
					} else if (recived == 9){
						button1 = '9';
						flagPulo = 0;
					}
					
				}
				if(xQueueReceive(xQueueProc, &(adc), 10)){
					if(adc.value > 2500) {
						if(button1 == '1') {
							button1 = '3';
						} else {
							button1 = '2';
						}
					} else if(adc.value < 1500){
						if(button1 == '1') {
							button1 = '5';
						} else {
							button1 = '4';
						}
					} else if (adc.value > 1500 && adc.value < 2500) {
						if(button1 == '1') {
							button1 = '1';
						} else if(button1 == '7') {
							button1 = '7';
						} else {
							button1 = '0';
						}
					}
				}
				// envia status botão
				while(!usart_is_tx_ready(USART_COM)) {
					vTaskDelay(10 / portTICK_PERIOD_MS);
				}
				usart_write(USART_COM, button1);
				
				// envia fim de pacote
				while(!usart_is_tx_ready(USART_COM)) {
					vTaskDelay(10 / portTICK_PERIOD_MS);
				}
				usart_write(USART_COM, eof);

				// dorme por 500 ms
				vTaskDelay(10 / portTICK_PERIOD_MS);
			}
		}
		
	}
}

static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel, afec_callback_t callback) {
  /*************************************
   * Ativa e configura AFEC
   *************************************/
  /* Ativa AFEC - 0 */
  afec_enable(afec);

  /* struct de configuracao do AFEC */
  struct afec_config afec_cfg;

  /* Carrega parametros padrao */
  afec_get_config_defaults(&afec_cfg);

  /* Configura AFEC */
  afec_init(afec, &afec_cfg);

  /* Configura trigger por software */
  afec_set_trigger(afec, AFEC_TRIG_SW);

  /*** Configuracao específica do canal AFEC ***/
  struct afec_ch_config afec_ch_cfg;
  afec_ch_get_config_defaults(&afec_ch_cfg);
  afec_ch_cfg.gain = AFEC_GAINVALUE_0;
  afec_ch_set_config(afec, afec_channel, &afec_ch_cfg);

  /*
  * Calibracao:
  * Because the internal ADC offset is 0x200, it should cancel it and shift
  down to 0.
  */
  afec_channel_set_analog_offset(afec, afec_channel, 0x200);

  /***  Configura sensor de temperatura ***/
  struct afec_temp_sensor_config afec_temp_sensor_cfg;

  afec_temp_sensor_get_config_defaults(&afec_temp_sensor_cfg);
  afec_temp_sensor_set_config(afec, &afec_temp_sensor_cfg);

  /* configura IRQ */
  afec_set_callback(afec, afec_channel, callback, 1);
  NVIC_SetPriority(afec_id, 4);
  NVIC_EnableIRQ(afec_id);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/

int main(void) {
	/* Initialize the SAM system */
	sysclk_init();
	board_init();

	configure_console();
	xQueueButFreq = xQueueCreate(32, sizeof(uint32_t));
	if (xQueueButFreq == NULL)
	printf("falha em criar a queue \n");

	xQueueProc = xQueueCreate(100, sizeof(adcData));
	if (xQueueProc == NULL)
	printf("falha em criar a queue xQueueProc \n");

	/* Create task to make led blink */
	xTaskCreate(task_bluetooth, "BLT", TASK_BLUETOOTH_STACK_SIZE, NULL,	TASK_BLUETOOTH_STACK_PRIORITY, NULL);
	xTaskCreate(task_comunicacao, "COM", TASK_BLUETOOTH_STACK_SIZE, NULL,	TASK_BLUETOOTH_STACK_PRIORITY, NULL);
	
	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1){}

	/* Will only get here if there was insufficient memory to create the idle task. */
	return 0;
}