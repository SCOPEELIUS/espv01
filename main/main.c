#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <string.h>


//Handles
SemaphoreHandle_t semaphore;
TaskHandle_t taskHandle=NULL;
SemaphoreHandle_t semaphore2;

//Software interrupt number
#define GPIO_INPUT_IO_16 16
#define SWI_PRIORITY_CHANGE 0

//pinDeclaration
#define GPIO_OUTPUT_IO_23 23
#define GPIO_OUTPUT_IO_21 21
#define UART_TX_18 18
#define UART_RX_5  19


//interrupts
void IRAM_ATTR gpio_isr_handler(void* arg){
	BaseType_t xHigherPriorityTaskWoken=pdFALSE;
	gpio_set_level(GPIO_OUTPUT_IO_21,true);
	xSemaphoreGiveFromISR(semaphore2,&xHigherPriorityTaskWoken);

	if(xHigherPriorityTaskWoken==pdTRUE){
			portYIELD_FROM_ISR();
		}
}



//gpioInit
void gpioInit(){
	gpio_config_t io_config,config1,config2;
	io_config.intr_type=GPIO_INTR_DISABLE;
	io_config.mode=GPIO_MODE_OUTPUT;
	io_config.pin_bit_mask=(1ULL<<GPIO_OUTPUT_IO_23);
	io_config.pull_down_en=GPIO_PULLDOWN_DISABLE;
	io_config.pull_up_en=GPIO_PULLUP_DISABLE;
	gpio_config(&io_config);

	config2.intr_type=GPIO_INTR_DISABLE;
	config2.mode=GPIO_MODE_OUTPUT;
	config2.pin_bit_mask=(1ULL<<GPIO_OUTPUT_IO_21);
	config2.pull_down_en=GPIO_PULLDOWN_DISABLE;
	config2.pull_up_en=GPIO_PULLUP_DISABLE;
	gpio_config(&config2);



	config1.intr_type=GPIO_INTR_POSEDGE;
	config1.mode=GPIO_MODE_INPUT;
	config1.pin_bit_mask=(1ULL<<GPIO_INPUT_IO_16);
	config1.pull_down_en=GPIO_PULLDOWN_DISABLE;
	config1.pull_up_en=GPIO_PULLUP_DISABLE;
	gpio_config(&config1);

	gpio_install_isr_service(0);
	gpio_isr_handler_add(GPIO_INPUT_IO_16,gpio_isr_handler,(void*)GPIO_INPUT_IO_16);

	uart_config_t uart_config={
		.baud_rate=9600,
		.data_bits=UART_DATA_8_BITS,
		.parity=UART_PARITY_DISABLE,
		.stop_bits=UART_STOP_BITS_1,
		.flow_ctrl=UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config(UART_NUM_1,&uart_config);
	uart_set_pin(UART_NUM_1,UART_TX_18,UART_RX_5,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
	if(uart_driver_install(UART_NUM_1,256,256,0,NULL,0)!=ESP_OK){
		printf("Failed to initialize the uart driver  ");
		return;
	}

}



void vTaskFunction(void* params){
	for(;;){
		int i=0;
		gpio_set_level(GPIO_OUTPUT_IO_23,true);
		while (i<10){
			printf("This is a task in %d \n",xPortGetCoreID());
					vTaskDelay(1000/portTICK_PERIOD_MS);
					i++;
		}

xSemaphoreGive(semaphore);
printf("Semaphore notified  core %d  \n",xPortGetCoreID());
i=0;
vTaskDelay(1000/portTICK_PERIOD_MS);
	}
}
void vTask2Function(){
	for(;;){
		if(ulTaskNotifyTake(pdTRUE,2000 / portTICK_PERIOD_MS)){
			gpio_set_level(GPIO_OUTPUT_IO_21,false);
			vTaskPrioritySet(taskHandle,3);
			printf("Took the notification ");
		}
		printf("core %d Waiting for message \n",xPortGetCoreID());
		xSemaphoreTake(semaphore,portMAX_DELAY);
		printf("core %d Got notified \n",xPortGetCoreID());
		printf("This is a task in %d \n",xPortGetCoreID());
		gpio_set_level(GPIO_OUTPUT_IO_23,false);


	}
}

void vUartSendTask(void* param){
	char* data="Hello sent data from controller \n";
	for(;;){
		xSemaphoreTake(semaphore2,portMAX_DELAY);
		printf("This is a task in %d \n",xPortGetCoreID());
		uart_write_bytes(UART_NUM_1,data,strlen(data));
		printf("Sending data to UART");
		xTaskNotifyGive(taskHandle);
	}
}

void app_main(void){
	gpioInit();
	semaphore=xSemaphoreCreateBinary();
	semaphore2=xSemaphoreCreateBinary();
	xTaskCreate(vTaskFunction,"Task 1",1024*2,NULL,2,NULL);
	xTaskCreatePinnedToCore(vTask2Function,"Task2",1024*2,NULL,2,&taskHandle,1);
	xTaskCreate(vUartSendTask,"Task 3",1024*3,NULL,1,NULL);
}
