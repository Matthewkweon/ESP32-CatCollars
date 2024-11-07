// /* BSD Socket API Example

//    This example code is in the Public Domain (or CC0 licensed, at your option.)

//    Unless required by applicable law or agreed to in writing, this
//    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//    CONDITIONS OF ANY KIND, either express or implied.


#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>             
#include <inttypes.h>           
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"     
#include "esp_log.h"            // for error logging
#include "esp_system.h"         
#include "driver/uart.h"
#include "driver/gptimer.h"     
#include "driver/gpio.h"        
#include "driver/ledc.h"     
#include "driver/i2c.h"   
#include "esp_adc_cal.h"
#include "driver/adc.h"

#include "./ADXL343.h"

// Master I2C
#define I2C_EXAMPLE_MASTER_SCL_IO          22   // gpio number for i2c clk
#define I2C_EXAMPLE_MASTER_SDA_IO          23   // gpio number for i2c data
#define I2C_EXAMPLE_MASTER_NUM             I2C_NUM_0  // i2c port
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE  0    // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE  0    // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_FREQ_HZ         100000     // i2c master clock freq
#define WRITE_BIT                          I2C_MASTER_WRITE // i2c master write
#define READ_BIT                           I2C_MASTER_READ  // i2c master read
#define ACK_CHECK_EN                       true // i2c master will check ack
#define ACK_CHECK_DIS                      false// i2c master will not check ack
#define ACK_VAL                            0x00 // i2c ack value
#define NACK_VAL                           0x01 // i2c nack value (Was FF)

#define GPIO_INPUT_IO_1       4
#define ESP_INTR_FLAG_DEFAULT 0
#define GPIO_INPUT_PIN_SEL    1ULL<<GPIO_INPUT_IO_1

// ADXL343
#define ACCEL_ADDR                         ADXL343_ADDRESS // 0x53

// 14-Segment Display
#define DISP_ADDR                         0x70 // alphanumeric address
#define OSC                                0x21 // oscillator cmd
#define HT16K33_BLINK_DISPLAYON            0x01 // Display on cmd
#define HT16K33_BLINK_OFF                  0    // Blink off cmd
#define HT16K33_BLINK_CMD                  0x80 // Blink cmd
#define HT16K33_CMD_BRIGHTNESS             0xE0 // Brightness cmd


#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "protocol_examples_common.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"

#define BLINK_GPIO 13 // On-board LED for ESP32
#define SERVER_PORT 8080
#define SERVER_IP "192.168.1.42" // Replace with the laptop's IP address

static const char *TAG = "udp_client";
int blink_time = 500; // Default blink time

char* name = "Otto";

char* format_time(int seconds);
void seconds_to_hms(int seconds, int *hours, int *minutes, int *remaining_seconds);
char * format_time_colon(int seconds);

static esp_adc_cal_characteristics_t *adc_chars;
// static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
// static const adc_atten_t atten = ADC_ATTEN_DB_11;
// static const adc_unit_t unit = ADC_UNIT_1;

char* state = "INACTIVE";
int inactive_timer = 0;
int active_timer = 0;
int hactive_timer = 0;
int current_timer = 0;
int real_timer = 0;

char prev_leader[100];
char leader[100];

void init_uart(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 1024 * 2, 0, 0, NULL, 0);
}

// Function to initiate i2c -- note the MSB declaration!
static void i2c_master_init(){
  // Debug
  printf("\n>> i2c Config\n");
  int err;

  // Port configuration
  int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;

  /// Define I2C configurations
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;                              // Master mode
  conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;              // Default SDA pin
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;                  // Internal pullup
  conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;              // Default SCL pin
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;                  // Internal pullup
  conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;       // CLK frequency
  conf.clk_flags = 0;     
  err = i2c_param_config(i2c_master_port, &conf);           // Configure
  if (err == ESP_OK) {printf("- parameters: ok\n");}

  // Install I2C driver
  err = i2c_driver_install(i2c_master_port, conf.mode,
                     I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                     I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
  if (err == ESP_OK) {printf("- initialized: yes\n");}

  // Data in MSB mode
  i2c_set_data_mode(i2c_master_port, I2C_DATA_MODE_MSB_FIRST, I2C_DATA_MODE_MSB_FIRST);
}

// Utility  Functions //////////////////////////////////////////////////////////

// Utility function to test for I2C device address -- not used in deploy
int testConnection(uint8_t devAddr, int32_t timeout) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  int err = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return err;
}

// Utility function to scan for i2c device
static void i2c_scanner() {
  int32_t scanTimeout = 1000;
  printf("\n>> I2C scanning ..."  "\n");
  uint8_t count = 0;
  for (uint8_t i = 1; i < 127; i++) {
    // printf("0x%X%s",i,"\n");
    if (testConnection(i, scanTimeout) == ESP_OK) {
      printf( "- Device found at address: 0x%X%s", i, "\n");
      count++;
    }
  }
  if (count == 0) {printf("- No I2C devices found!" "\n");}
}

////////////////////////////////////////////////////////////////////////////////

// Display Functions ///////////////////////////////////////////////////////////

// AI Generated:
static const uint16_t alphafonttable[] = {
    0b0000000000000001, 0b0000000000000010, 0b0000000000000100,
    0b0000000000001000, 0b0000000000010000, 0b0000000000100000,
    0b0000000001000000, 0b0000000010000000, 0b0000000100000000,
    0b0000001000000000, 0b0000010000000000, 0b0000100000000000,
    0b0001000000000000, 0b0010000000000000, 0b0100000000000000,
    0b1000000000000000, 0b0000000000000000, 0b0000000000000000,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000,
    0b0001001011001001, 0b0001010111000000, 0b0001001011111001,
    0b0000000011100011, 0b0000010100110000, 0b0001001011001000,
    0b0011101000000000, 0b0001011100000000,
    0b0000000000000000, //
    0b0000000000000110, // !
    0b0000001000100000, // "
    0b0001001011001110, // #
    0b0001001011101101, // $
    0b0000110000100100, // %
    0b0010001101011101, // &
    0b0000010000000000, // '
    0b0010010000000000, // (
    0b0000100100000000, // )
    0b0011111111000000, // *
    0b0001001011000000, // +
    0b0000100000000000, // ,
    0b0000000011000000, // -
    0b0100000000000000, // .
    0b0000110000000000, // /
    0b0000110000111111, // 0
    0b0000000000000110, // 1
    0b0000000011011011, // 2
    0b0000000010001111, // 3
    0b0000000011100110, // 4
    0b0010000001101001, // 5
    0b0000000011111101, // 6
    0b0000000000000111, // 7
    0b0000000011111111, // 8
    0b0000000011101111, // 9
    0b0001001000000000, // :
    0b0000101000000000, // ;
    0b0010010000000000, // <
    0b0000000011001000, // =
    0b0000100100000000, // >
    0b0001000010000011, // ?
    0b0000001010111011, // @
    0b0000000011110111, // A
    0b0001001010001111, // B
    0b0000000000111001, // C
    0b0001001000001111, // D
    0b0000000011111001, // E
    0b0000000001110001, // F
    0b0000000010111101, // G
    0b0000000011110110, // H
    0b0001001000001001, // I
    0b0000000000011110, // J
    0b0010010001110000, // K
    0b0000000000111000, // L
    0b0000010100110110, // M
    0b0010000100110110, // N
    0b0000000000111111, // O
    0b0000000011110011, // P
    0b0010000000111111, // Q
    0b0010000011110011, // R
    0b0000000011101101, // S
    0b0001001000000001, // T
    0b0000000000111110, // U
    0b0000110000110000, // V
    0b0010100000110110, // W
    0b0010110100000000, // X
    0b0001010100000000, // Y
    0b0000110000001001, // Z
    0b0000000000111001, // [
    0b0010000100000000, //
    0b0000000000001111, // ]
    0b0000110000000011, // ^
    0b0000000000001000, // _
    0b0000000100000000, // `
    0b0001000001011000, // a
    0b0010000001111000, // b
    0b0000000011011000, // c
    0b0000100010001110, // d
    0b0000100001011000, // e
    0b0000000001110001, // f
    0b0000010010001110, // g
    0b0001000001110000, // h
    0b0001000000000000, // i
    0b0000000000001110, // j
    0b0011011000000000, // k
    0b0000000000110000, // l
    0b0001000011010100, // m
    0b0001000001010000, // n
    0b0000000011011100, // o
    0b0000000101110000, // p
    0b0000010010000110, // q
    0b0000000001010000, // r
    0b0010000010001000, // s
    0b0000000001111000, // t
    0b0000000000011100, // u
    0b0010000000000100, // v
    0b0010100000010100, // w
    0b0010100011000000, // x
    0b0010000000001100, // y
    0b0000100001001000, // z
    0b0000100101001001, // {
    0b0001001000000000, // |
    0b0010010010001001, // }
    0b0000010100100000, // ~
    0b0011111111111111, // del
};

// Function to map characters to their binary values
uint16_t charToAlphaBinary(char c) {
    // Check if character is within the printable ASCII range
    if (c >= ' ' && c <= '~') {
        // Map the character to its corresponding binary code from the font table
        return alphafonttable[c - ' ' + 32];
    } else {
        // Return 0 for characters outside the supported range
        return 0;
    }
}

int alpha_oscillator() {
  int ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( DISP_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, OSC, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  return ret;
}

// Set blink rate to off
int no_blink() {
  int ret;
  i2c_cmd_handle_t cmd2 = i2c_cmd_link_create();
  i2c_master_start(cmd2);
  i2c_master_write_byte(cmd2, ( DISP_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd2, HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | (HT16K33_BLINK_OFF << 1), ACK_CHECK_EN);
  i2c_master_stop(cmd2);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd2, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd2);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  return ret;
}

// Set Brightness
int set_brightness_max(uint8_t val) {
  int ret;
  i2c_cmd_handle_t cmd3 = i2c_cmd_link_create();
  i2c_master_start(cmd3);
  i2c_master_write_byte(cmd3, ( DISP_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd3, HT16K33_CMD_BRIGHTNESS | val, ACK_CHECK_EN);
  i2c_master_stop(cmd3);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd3, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd3);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  return ret;
}

////////////////////////////////////////////////////////////////////////////////

static void test_alpha_display() {
    // Debug
    int ret;
    printf(">> Test Alphanumeric Display: \n");

    // Set up routines
    // Turn on alpha oscillator
    ret = alpha_oscillator();
    if(ret == ESP_OK) {printf("- oscillator: ok \n");}
    // Set display blink off
    ret = no_blink();
    if(ret == ESP_OK) {printf("- blink: off \n");}
    ret = set_brightness_max(0xF);
    if(ret == ESP_OK) {printf("- brightness: max \n");}

    // Write to characters to buffer
    uint16_t binary_str[50];

    uint16_t displaybuffer[8];
    for(int j = 0; j < 4; j++){
      displaybuffer[j] = charToAlphaBinary(' ');
    }

    int first = 1;
    int str_len = 0;
    int str_i = 0;
    // Continually writes the same command
    while (1) {
      // Send commands characters to display over I2C

        i2c_cmd_handle_t cmd4 = i2c_cmd_link_create();
        i2c_master_start(cmd4);
        i2c_master_write_byte(cmd4, ( DISP_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd4, (uint8_t)0x00, ACK_CHECK_EN);
        for (uint8_t i=0; i<8; i++) {
            i2c_master_write_byte(cmd4, displaybuffer[i] & 0xFF, ACK_CHECK_EN);
            i2c_master_write_byte(cmd4, displaybuffer[i] >> 8, ACK_CHECK_EN);
        }
        i2c_master_stop(cmd4);
        ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd4, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd4);

        if(first == 1){
            char str[100] = "Leader is ";
            strcat(str, leader);

            str_i = 0;
            while(str[str_i] != '\0'){
            if(str_i > 100){
                printf("String too long\n");
                break;
            }
            binary_str[str_i] = charToAlphaBinary(str[str_i]);
            str_i += 1;
            }
            str_len = str_i;
            str_i = 4;

            for(int j = 0; j < 4; j++){
                displaybuffer[j] = binary_str[j];
            }
            first = 0;
        }
        else{
            for(int j = 0; j < 3; j++){
                displaybuffer[j] = displaybuffer[j + 1];
            }


            displaybuffer[3] = binary_str[str_i];
            str_i += 1;

            if(str_i > str_len){
            vTaskDelay(500 / portTICK_PERIOD_MS);
            str_i = 4;
            for(int j = 0; j < 4; j++){
                displaybuffer[j] = charToAlphaBinary(' ');
            }
                first = 1;
            }
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }     
       
}


////////////////////////////////////////////////////////////////////////////////

// ADXL343 Functions ///////////////////////////////////////////////////////////

// Get Device ID
int accel_getDeviceID(uint8_t *data) {
  int ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( ACCEL_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, ADXL343_REG_DEVID, ACK_CHECK_EN);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( ACCEL_ADDR << 1 ) | READ_BIT, ACK_CHECK_EN);
  i2c_master_read_byte(cmd, data, ACK_CHECK_DIS);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret;
}

// Write one byte to register
int accel_writeRegister(uint8_t reg, uint8_t data) {
  // AI Generated
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (ACCEL_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  int ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret;

}

// Read register
uint8_t accel_readRegister(uint8_t reg) {
  // AI Generated
  uint8_t data = 0;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (ACCEL_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (ACCEL_ADDR << 1) | READ_BIT, ACK_CHECK_EN);
  i2c_master_read_byte(cmd, &data, ACK_CHECK_DIS);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return data;
}

// read 16 bits (2 bytes)
int16_t accel_read16(uint8_t reg) {
  // AI Generated
  uint8_t lsb, msb;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( ACCEL_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( ACCEL_ADDR << 1 ) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &lsb, ACK_VAL);  // Read LSB
    i2c_master_read_byte(cmd, &msb, ACK_CHECK_DIS);  // Read MSB
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return (int16_t)((msb << 8) | lsb);
}


void setRange(range_t range) {
  /* Red the data format register to preserve bits */
  uint8_t format = accel_readRegister(ADXL343_REG_DATA_FORMAT);

  /* Update the data rate */
  format &= ~0x0F;
  format |= range;

  /* Make sure that the FULL-RES bit is enabled for range scaling */
  format |= 0x08;

  /* Write the register back to the IC */
  accel_writeRegister(ADXL343_REG_DATA_FORMAT, format);

}

range_t getRange(void) {
  /* Red the data format register to preserve bits */
  return (range_t)(accel_readRegister(ADXL343_REG_DATA_FORMAT) & 0x03);
}

dataRate_t getDataRate(void) {
  return (dataRate_t)(accel_readRegister(ADXL343_REG_BW_RATE) & 0x0F);
}

// function to get acceleration
const int avg_len = 5;
float avg[] = {0, 0, 0, 0, 0};

float calculate_average(float arr[]) {
    float sum = 0.0;
    for (int i = 0; i < avg_len; i++) {
        sum += arr[i];
    }
    return sum / avg_len;
}

float calculate_magnitude(float x, float y, float z) {
    return sqrt(x * x + y * y + z * z);
}

void seconds_to_hms(int seconds, int *hours, int *minutes, int *remaining_seconds) {
    *hours = seconds / 3600;
    *minutes = (seconds % 3600) / 60;
    *remaining_seconds = seconds % 60;
}

char* format_time(int seconds) {
    static char time_str[20];
    int hours, minutes, remaining_seconds;
    seconds_to_hms(seconds, &hours, &minutes, &remaining_seconds);
    snprintf(time_str, sizeof(time_str), "%02d/%02d/%02d", hours, minutes, remaining_seconds);
    return time_str;
}
char* format_time_colon(int seconds) {
    static char time_str[20];
    int hours, minutes, remaining_seconds;
    seconds_to_hms(seconds, &hours, &minutes, &remaining_seconds);
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hours, minutes, remaining_seconds);
    return time_str;
}



void getAccel(float * xp, float *yp, float *zp) {
    *xp = accel_read16(ADXL343_REG_DATAX0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    *yp = accel_read16(ADXL343_REG_DATAY0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    *zp = accel_read16(ADXL343_REG_DATAZ0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    
    

    float mag = calculate_magnitude(*xp, *yp, *zp);
    
    for(int i = 0; i < avg_len - 1; i++){
        avg[i] = avg[i + 1];
    }
    avg[avg_len - 1] = mag;

    float current_avg = calculate_average(avg);

    if(current_avg < 10){
        state = "INACTIVE";
        inactive_timer += 1;
        current_timer = inactive_timer;

    }
    else if(current_avg < 15){
        state = "ACTIVE";
        active_timer += 1;
        current_timer = active_timer;

    }
    else{
        state = "HIGHLY ACTIVE";
        hactive_timer += 1;
        current_timer = hactive_timer;
    }

    // printf("Mag: %.2f, State: %s, Timer: %d\n", mag, state, current_timer);
    // printf("Inactive Timer: %d, Active Timer: %d, Highly Active Timer: %d\n", inactive_timer, active_timer, hactive_timer);


}

void udp_client_task(void *pvParameters) {
    char rx_buffer[128];
    char payload[100];
    struct sockaddr_in dest_addr;


    while (1) {

        dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(SERVER_PORT);
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            return;
        }
        snprintf(payload, sizeof(payload), "%d %s", hactive_timer + active_timer, name);
        // Send request to server
        int err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            close(sock);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Message sent");
        //printf("%s\n", payload);

        // Receive server response
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
        } else {
            rx_buffer[len] = 0;
            ESP_LOGI(TAG, "Received blink time: %s ms", rx_buffer);
        }

        close(sock);

        char *leader_position = strstr(rx_buffer, "\"leader\":\"");
        
        if (leader_position != NULL) {
            leader_position += strlen("\"leader\":\"");
            sscanf(leader_position, "%[^\"]", leader);
        } else {
            printf("Leader not found in the string.\n");
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS); // Request blink time every 5 seconds
    }
    vTaskDelete(NULL);
}

void accel_task(){
    while(1){
        float xVal, yVal, zVal;
        getAccel(&xVal, &yVal, &zVal);
        real_timer++;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}

void buzzer_task(){
    esp_rom_gpio_pad_select_gpio(13);
    gpio_set_direction(13, GPIO_MODE_OUTPUT);

    while(1){
        if(strcmp(leader, prev_leader) != 0 ){
            printf("Leader Change!\n");
            gpio_set_level(13, 1);
            vTaskDelay(4000 / portTICK_PERIOD_MS);  // Wait for 1 second
            gpio_set_level(13, 0);

            strcpy(prev_leader, leader);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS); 
    }
}

void app_main(void) {
    // Routine
    i2c_master_init();
    i2c_scanner();
    init_uart();

    // Check for ADXL343
    uint8_t deviceID;
    accel_getDeviceID(&deviceID);
    if (deviceID == 0xE5) {
        printf("\n>> Found ADAXL343\n");
    }

    // Disable interrupts
    accel_writeRegister(ADXL343_REG_INT_ENABLE, 0);

    // Set range
    setRange(ADXL343_RANGE_16_G);
    // Display range
    printf  ("- Range:         +/- ");
    switch(getRange()) {
        case ADXL343_RANGE_16_G:
        printf  ("16 ");
        break;
        case ADXL343_RANGE_8_G:
        printf  ("8 ");
        break;
        case ADXL343_RANGE_4_G:
        printf  ("4 ");
        break;
        case ADXL343_RANGE_2_G:
        printf  ("2 ");
        break;
        default:
        printf  ("?? ");
        break;
    }
    printf(" g\n");

    // Display data rate
    printf ("- Data Rate:    ");
    switch(getDataRate()) {
        case ADXL343_DATARATE_3200_HZ:
        printf  ("3200 ");
        break;
        case ADXL343_DATARATE_1600_HZ:
        printf  ("1600 ");
        break;
        case ADXL343_DATARATE_800_HZ:
        printf  ("800 ");
        break;
        case ADXL343_DATARATE_400_HZ:
        printf  ("400 ");
        break;
        case ADXL343_DATARATE_200_HZ:
        printf  ("200 ");
        break;
        case ADXL343_DATARATE_100_HZ:
        printf  ("100 ");
        break;
        case ADXL343_DATARATE_50_HZ:
        printf  ("50 ");
        break;
        case ADXL343_DATARATE_25_HZ:
        printf  ("25 ");
        break;
        case ADXL343_DATARATE_12_5_HZ:
        printf  ("12.5 ");
        break;
        case ADXL343_DATARATE_6_25HZ:
        printf  ("6.25 ");
        break;
        case ADXL343_DATARATE_3_13_HZ:
        printf  ("3.13 ");
        break;
        case ADXL343_DATARATE_1_56_HZ:
        printf  ("1.56 ");
        break;
        case ADXL343_DATARATE_0_78_HZ:
        printf  ("0.78 ");
        break;
        case ADXL343_DATARATE_0_39_HZ:
        printf  ("0.39 ");
        break;
        case ADXL343_DATARATE_0_20_HZ:
        printf  ("0.20 ");
        break;
        case ADXL343_DATARATE_0_10_HZ:
        printf  ("0.10 ");
        break;
        default:
        printf  ("???? ");
        break;
    }
    printf(" Hz\n\n");
    accel_writeRegister(ADXL343_REG_POWER_CTL, 0x08);

    // Initialize NVS and Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect()); // Connect to Wi-Fi

    // Start UDP client task
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
    // Start blink task
    xTaskCreate(accel_task, "accel_task", 2048, NULL, 5, NULL);

    xTaskCreate(buzzer_task, "buzzer_task", 2048, NULL, 5, NULL);

    xTaskCreate(test_alpha_display, "display_task", 4096, NULL, 5, NULL);
}
