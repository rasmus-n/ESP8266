#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"
#include "espconn.h"

#define MY_LED 14
#define MY_INPUT 12

ETSTimer my_timer;
ETSTimer my_debounce_timer;

struct espconn my_conn;
esp_tcp my_tcp;
uint32_t input_count = 0;

void ICACHE_FLASH_ATTR my_timer_cb(void *arg);
LOCAL void input_intr_handler(void *arg);
void ICACHE_FLASH_ATTR my_debounce_timer_cb(void *arg);

//extern uint8_t at_wifiMode;

LOCAL void ICACHE_FLASH_ATTR
my_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
  char cn[3] = {0,0,0};
  int8_t n;
  char temp[8];
  struct espconn *pesp_conn = arg;

//  uart0_sendStr("Data: ");
//  uart0_tx_buffer(pusrdata, length);

  if(length>5)
  {
    if (0 == os_memcmp(pusrdata, "UART:", 5))
    {
      uart0_tx_buffer(pusrdata+5, length-5);
    }
  }

  if(length >= 6)
  {
    if (0 == os_memcmp(pusrdata, "COUNT?", 6))
    {
      os_sprintf(temp, "%d\r\n", input_count);
      uart0_sendStr(temp);
      espconn_sent(pesp_conn, temp, os_strlen(temp));
    }
  }

  if(length>=8)
  {
    if (0 == os_memcmp(pusrdata, "GPIO", 4))
    {
      os_memcpy(cn, pusrdata+4, 2);
      n = atoi(cn);
      if(pusrdata[7] == '0')
      {
        GPIO_OUTPUT_SET(n, 0);
      }
      else
      {
        GPIO_OUTPUT_SET(n, 1);
      }
    }
  }
}

LOCAL void ICACHE_FLASH_ATTR
my_connect_cb(void *arg)
{
  char temp[8];
  struct espconn *pesp_conn = arg;
  espconn_regist_recvcb(pesp_conn, my_recv_cb);
  os_sprintf(temp, "dav\r\n");
  espconn_sent(pesp_conn, temp, os_strlen(temp));
//  uart0_sendStr("Connect\r\n");
//  espconn_regist_reconcb(pesp_conn, webserver_recon);
}

void user_init(void)
{
  struct ip_info ip_data;
  char temp[64];
  struct station_config ap_data;

//  wifi_set_opmode(STATION_MODE);
//  os_sprintf(ap_data.ssid, "yourSSID");
//  os_sprintf(ap_data.password, "yourPASSWD");
//  wifi_station_set_config(&ap_data);

  my_conn.type = ESPCONN_INVALID;

  gpio_init();
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  uart0_sendStr("Hello world!\r\n");

  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);

  PIN_PULLUP_EN(PERIPHS_IO_MUX_MTDI_U);

  ETS_GPIO_INTR_DISABLE();
  ETS_GPIO_INTR_ATTACH(input_intr_handler, NULL);
  GPIO_DIS_OUTPUT(MY_INPUT);
  gpio_pin_intr_state_set(GPIO_ID_PIN(MY_INPUT), GPIO_PIN_INTR_NEGEDGE);

  ETS_GPIO_INTR_ENABLE();

  os_timer_disarm(&my_timer);
  os_timer_setfn(&my_timer, &my_timer_cb, 0);
  os_timer_arm(&my_timer, 250, TRUE);

  os_timer_disarm(&my_debounce_timer);
  os_timer_setfn(&my_debounce_timer, &my_debounce_timer_cb, 0);
}

void ICACHE_FLASH_ATTR
my_timer_cb(void *arg)
{
  struct ip_info ip_data;
  char temp[64];
  static uint8_t status = 0;

  status ^= 1;
  GPIO_OUTPUT_SET(MY_LED, status);

  if(STATION_GOT_IP == wifi_station_get_connect_status())
  {
    wifi_get_ip_info(STATION_IF, &ip_data);
    os_sprintf(temp, "%d.%d.%d.%d\r\n", IP2STR(&ip_data.ip));
    uart0_sendStr(temp);

    my_conn.type = ESPCONN_TCP;
    my_conn.proto.tcp = &my_tcp;
    my_conn.proto.tcp->local_port = 333;

    espconn_regist_connectcb(&my_conn, my_connect_cb);
    espconn_accept(&my_conn);

    os_timer_disarm(&my_timer);
    GPIO_OUTPUT_SET(MY_LED, 0);
  }
}

void ICACHE_FLASH_ATTR
my_debounce_timer_cb(void *arg)
{
  ETS_GPIO_INTR_DISABLE();
  gpio_pin_intr_state_set(GPIO_ID_PIN(MY_INPUT), GPIO_PIN_INTR_NEGEDGE);
  ETS_GPIO_INTR_ENABLE();
}

LOCAL void
input_intr_handler(void *arg)
{
  uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

  input_count++;
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

  ETS_GPIO_INTR_DISABLE();
  gpio_pin_intr_state_set(GPIO_ID_PIN(MY_INPUT), GPIO_PIN_INTR_DISABLE);
  ETS_GPIO_INTR_ENABLE();

  os_timer_arm(&my_debounce_timer, 200, FALSE);
}
