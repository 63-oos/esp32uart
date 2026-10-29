/*
 *Stage-2 UART Interrupt ESP32C3 <====> PI 5
 

*/
#include<stdio.h>
#include<string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

#define UART_PORT       UART_NUM_1
#define TXD_PIN         4           //Change to free GPIO on board
#define RXD_PIN         5           //
#define Buf_size        1024
#define Baud_rate       9600        //9600 / 115200
#define QUEUE_LEN       20          //


#define LED             GPIO_NUM_8
#define BUTTON          GPIO_NUM_9
#define Led_Active_Low  0

static QueueHandle_t uart_queue;
static QueueHandle_t button_queue;

static const char* Tag = "PIN-2";
static bool led_state = true;



void uart_init(void);
static void button_init(void);
void uart_txd(void*);
void uart_rxd(void*);
static void console_init(void);
void uart_send(const char*);
void cmd_process(const char);
void handle_cmd(const char*);
static void led_set(bool);
static void Button_task(void*);

static void IRAM_ATTR button_isr_handler(void*arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(button_queue,&gpio_num,&woken);
    if(woken) portYIELD_FROM_ISR(woken);
}

void app_main()
{
    char msg[64];
    console_init();
    button_init();
    uart_init();
    gpio_reset_pin(GPIO_NUM_8);
    gpio_set_direction(GPIO_NUM_8, GPIO_MODE_OUTPUT);
    led_set(true);
    
    xTaskCreate(uart_txd,"tx_task",3072,NULL,10,NULL);
    xTaskCreate(uart_rxd,"rx_task",3072,NULL,9,NULL);

    printf("SEND: on / off / toggle / status / from pi\n");
    snprintf(msg,sizeof(msg),"SEND: on / off / toggle / status...\n");
    printf("READY\n");
}

/*=========================================INIT==================================================*/

static void console_init(void)
{
    usb_serial_jtag_driver_config_t usb_cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));
    usb_serial_jtag_vfs_use_driver();

    // idf_monitor sends CR (\r) when you press Enter; tell the VFS layer
    // to treat that as the line ending so fgets() actually terminates.
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
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
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT,Buf_size*2,Buf_size*2,QUEUE_LEN,&uart_queue,0));

    //(uart_port_t uart_num, int TXD, int RXD, ...)
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT,TXD_PIN,RXD_PIN,
                                UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE));
    gpio_set_pull_mode(RXD_PIN,GPIO_PULLUP_ONLY);               
}
static void button_init()
{
    gpio_config_t button_config = {
        .pin_bit_mask = (1ull<<BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&button_config);

    button_queue = xQueueCreate(10,sizeof(uint32_t));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON,button_isr_handler,(void*)BUTTON);
    xTaskCreate(Button_task,"Button_task",2048,NULL,10,NULL);
}

/*========================================RXD=========================================================*/

// Sleeps until the driver posts an event. No 100 ms polling loop.
void uart_rxd(void *pv)
{
    uart_event_t event;
    char rx_buf[Buf_size];
    while(1)
    {
        if(xQueueReceive(uart_queue,&event,portMAX_DELAY))
        {
            switch(event.type)
            {
                case UART_DATA:
                {
                    size_t remaining = event.size;
                    while(remaining>0)
                    {
                        size_t chunk = remaining<sizeof(rx_buf)-1?remaining:sizeof(rx_buf)-1;
                        //(uart_port_t uart_num, void *buf, uint32_t length, uint32_t ticks_to_wait)
                        //int rx_len = uart_read_bytes(UART_PORT,rx_buf,Buf_size-1,pdMS_TO_TICKS(20));
                        int rx_len = uart_read_bytes(UART_PORT,rx_buf,chunk,pdMS_TO_TICKS(20));
                        if(rx_len<=0)
                        break;
/*                      printf("Recived bytes :%d  recived data : %s\n",rx_len,(char*)rx_buf);
                        printf("[rx_task] lowest free stack: %u bytes\n",
                            (unsigned)uxTaskGetStackHighWaterMark(NULL));*/
                        for(int i =0; i<rx_len;i++)
                            cmd_process(rx_buf[i]);
                        remaining -= rx_len;
                    }
                    break;
                }
                case UART_FIFO_OVF:
                    printf("[Event] Hardware FIFO overflow, flushing\n");
                    uart_flush_input(UART_PORT);
                    xQueueReset(uart_queue);
                    break;
                case UART_BUFFER_FULL:
                    printf("[EVENT] Ring buffer full, flushing\n");
                    uart_flush_input(UART_PORT);
                    xQueueReset(uart_queue);
                    break;
                case UART_FRAME_ERR:
                    printf("[EVENT] Frame error (baude mismatch or noise?)\n");
                    break;
                case UART_PARITY_ERR:
                    printf("[EVENT] parity error\n");
                    break;
                case UART_BREAK:
                    printf("[EVENT] break detected\n");
                    break;
                default:
                    printf("[EVENT] other event type: %d\n",event.type);
                    break;
            }
        }
    }
}

/*============================================txd================================================*/

void uart_txd(void *pv)
{

    char line[32];
    int idx = 0;

    printf("%s\n",Tag);
    while(1)
    {
        int c = getchar();
        if(c == EOF)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if(c=='\r'||c=='\n')
        {
            line[idx] = '\0';
            if(idx>0){
                uart_send(line);
                printf("\n");

/*              debugging part
                printf("\nBYTES SENT [%d] MSG: [%s] sent\n",len,line);
                printf("[rx_task] lowest free stack: %u bytes\n",
                   (unsigned)uxTaskGetStackHighWaterMark(NULL));
*/

            }
            idx = 0;
        }
        else if(c=='\b'||c==127)
        {
            if(idx>0)
            {
                idx--;
                printf("\b \b");
            }
        }
        else if(idx<(int)sizeof(line)-1)
        {
            line[idx++] = c;
            putchar(c);
        }
    }
}
void uart_send(const char* s)
{
    //(uart_port_t uart_num, const void *src, size_t size, int brk_len)
    uart_write_bytes(UART_PORT,s,strlen(s));
}

/*===============================handle cmd========================================*/

static char line[32];
static int idx = 0;

void cmd_process(const char c)
{
    
    if(c=='\n'||c=='\r')
    {
        if(c>0)
        {
            line[idx++] = '\0';
            handle_cmd(line);
            idx=0;
        }
    }
    else if(idx<sizeof(line)-1)
    {
        line[idx++] = c;
    }
    //else line to long, extra character are dropped
    
}

void handle_cmd(const char* cmd)
{
    char Ready[32];
    if((strcmp(cmd,"on"))==0)
    {
        led_set(false);
        uart_send("LED ON\n");
    }
    else if((strcmp(cmd,"off"))==0)
    {
        led_set(true);
        uart_send("LED OFF\n");
    }
    else if(strcmp(cmd,"toggle")==0)
    {
        led_state = !led_state;
        led_set(led_state);
        snprintf(Ready,sizeof(Ready),"LED %s\n",led_state?"OFF":"ON");
        uart_send(Ready);
    }
    else if(strcmp(cmd,"status")==0)
    {
        snprintf(Ready,sizeof(Ready),"LED %s\n",led_state?"OFF":"ON");
        uart_send(Ready);
    }
    printf("[CMD]: %s \n",cmd);
}

/***************************************button control**************************************************/

static void Button_task(void*pv)
{
    uint32_t gpio_num;
    char Ready[32];
    while(1)
    {/*
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // sleep until the ISR pokes us
        vTaskDelay(pdMS_TO_TICKS(30));              // let the contacts settle
        ulTaskNotifyTake(pdTRUE, 0);                // discard bounce edges that arrived meanwhile

        printf("wake-up latency: %lld us\n", (long long)(esp_timer_get_time() - isr_time_us));*/

        if(xQueueReceive(button_queue,&gpio_num,portMAX_DELAY))
        {
            int button_state = gpio_get_level(BUTTON);
            if(button_state==0)
            {
                led_state = !led_state;
                led_set(led_state);
                snprintf(Ready,sizeof(Ready),"[BUTTON PRESSED ] LED %s\n",led_state?"OFF":"ON");
                printf("%s",Ready);
                uart_send(Ready);
            }
        }
    }
}

/*******************************************LED CONTROL****************************************************/

static void led_set(bool on)
{
    led_state = Led_Active_Low?!on:on;
    gpio_set_level(GPIO_NUM_8,led_state);
}