/*
 *Stage-1 UART polling LOOPBACK

*/
#include<stdio.h>
#include<string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#define UART_PORT       UART_NUM_1
#define TXD_PIN         4           //Change to free GPIO on board
#define RXD_PIN         5           //
#define Buf_size        1024
#define Baud_rate       9600        //9600 / 115200

static const char* Tag = "PIN-1";

void uart_init(void);
void uart_txd(int*);
void uart_rxd(uint8_t*);

void app_main()
{
    uart_init();
    uint8_t rx_buf[Buf_size];
    int counter = 0;

    printf("%s\n",Tag);
    while(1)
    {
       uart_txd(&counter);
       //counter++;//after sometime pass this as a pointer to a uart_txd()
       uart_rxd(rx_buf);
       vTaskDelay(pdMS_TO_TICKS(1500));
    
    }
}

void uart_init(void)
{
    uart_config_t uart_config = {               //8N1   | 0 (start bit) | D0 D1 D2 D3 D4 D5 D6 D7 (8 Data bits)  | (no parity) |  1 (sb1) (1 stop bit)  |
        .baud_rate = Baud_rate,                 //9600
        .data_bits = UART_DATA_8_BITS,          //8 BIT
        .parity = UART_PARITY_DISABLE,          //No Parity
        .stop_bits = UART_STOP_BITS_1,          //1 Stop Bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  
        .source_clk = UART_SCLK_DEFAULT,
    };

    // event_queue_size = 0 here -> pure polling mode, no ISR-fed queue

    //(uart_port_t uart_num, const uart_config_t *uart_config)
    ESP_ERROR_CHECK(uart_param_config(UART_PORT,&uart_config));

    //(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int queue_size, QueueHandle_t *uart_queue, int intr_alloc_flags)
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT,Buf_size*2,Buf_size*2,0,NULL,0));

    //(uart_port_t uart_num, int TXD, int RXD, ...)
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT,TXD_PIN,RXD_PIN,
                                UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE));                
}

void uart_rxd(uint8_t* rx_buf)
{
     //(uart_port_t uart_num, void *buf, uint32_t length, uint32_t ticks_to_wait)
       int rx_len = uart_read_bytes(UART_NUM_1,rx_buf,Buf_size-1,pdMS_TO_TICKS(500));

       if(rx_len>0)
       {
        rx_buf[rx_len] = '\0';
        printf("      Recived byes :%d  recived data : [%s]\n",rx_len,(char*)rx_buf);
       }
       else
       {
        printf("\nnothing recived\n");
       }
}

void uart_txd(int* counter)
{
    char tx_msg[64];
    int len = snprintf(tx_msg,sizeof(tx_msg),"PING:%d",(*counter)++);
    //(uart_port_t uart_num, const void *src, size_t size, int brk_len)
    uart_write_bytes(UART_NUM_1,tx_msg,len);

    printf("\nBYTES SENT [%d] MSG: [%s] sent\n",len,tx_msg);
}