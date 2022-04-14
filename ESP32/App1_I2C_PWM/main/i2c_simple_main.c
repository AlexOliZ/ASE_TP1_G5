#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const char *TAG = "TC74";
#define MAX_TEMP 150
#define MIN_TEMP -65

/**  LEDC  **/
#define LEDC_HS_TIMER          LEDC_TIMER_1
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE

#define LEDC_HS_CH0_GPIO       (27)
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_1

#define LEDC_LS_TIMER          LEDC_TIMER_1
#define LEDC_LS_MODE           LEDC_HIGH_SPEED_MODE

#define LEDC_TEST_CH_NUM       (3)
#define DEFAULT_DUTY         (3000)
#define MAX_DUTY               (5000)


/**  I2C  **/
#define TC74_SLAVE_ADDR_A5   0x4D  /*!< default slave address for TC74 sensor */

#define READ_TEMP_REGISTER          0x00
#define READ_WRITE_CONFIG_REGISTER  0x01
#define SET_NORM_OP_VALUE           0x00  /*!< sets the 7th bit of configuration register to normal mode */
#define SET_STANBY_VALUE            0x80  /*!< sets the 7th bit of configuration register to standby mode */

#define _I2C_NUMBER(num) I2C_NUM_##num
#define I2C_NUMBER(num) _I2C_NUMBER(num)

#define I2C_MASTER_SCL_IO GPIO_NUM_22               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO GPIO_NUM_21               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM    I2C_NUMBER(0)             /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 100000                   /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0                 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0                 /*!< I2C master doesn't need buffer */

#define TC74_SLAVE_ADDR  TC74_SLAVE_ADDR_A5         /*!< slave address for TC74 sensor */
#define WRITE_BIT I2C_MASTER_WRITE                  /*!< I2C master write */
#define READ_BIT  I2C_MASTER_READ                   /*!< I2C master read */
#define ACK_CHECK_EN 0x1                            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0                           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                                 /*!< I2C ack value */
#define NACK_VAL 0x1                                /*!< I2C nack value */



esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode             = I2C_MODE_MASTER;
    conf.sda_io_num       = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
    conf.scl_io_num       = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = 0;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

esp_err_t i2c_master_read_temp(i2c_port_t i2c_num, uint8_t *tmprt)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TC74_SLAVE_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, READ_TEMP_REGISTER, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TC74_SLAVE_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, tmprt, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 300 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static void i2c_temperature_task(void *arg){

  // setup the sensor
  ESP_ERROR_CHECK(i2c_master_init());

  // signed integer value of 8 bits
  uint8_t temperature_value = 0;
  uint8_t last_measurment = 0;
  
  /* config LEDC */
  ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT,   // resolution of PWM duty
        .freq_hz = 5000,                        // frequency of PWM signal
        .speed_mode = LEDC_LS_MODE,             // timer mode
        .timer_num = LEDC_LS_TIMER,             // timer index
        .clk_cfg = LEDC_AUTO_CLK,               // Auto select the source clock
    };
    ledc_timer_config(&ledc_timer);
    
    ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM] = {
        {
            .channel    = LEDC_HS_CH0_CHANNEL,
            .duty       = DEFAULT_DUTY,
            .gpio_num   = LEDC_HS_CH0_GPIO,
            .speed_mode = LEDC_HS_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_HS_TIMER,
            .flags.output_invert = 0
        },      
    };
    ledc_channel_config(&ledc_channel[0]);
    int duty_c=0;
    // signed integer value of 8 bits
    //uint8_t operation_mode;

    // set standby mode for testing (5uA consuption)
    // i2c_master_set_tc74_mode(I2C_MASTER_NUM, SET_STANBY_VALUE);

    // set normal mode for testing (200uA consuption)
    // i2c_master_set_tc74_mode(I2C_MASTER_NUM, SET_NORM_OP_VALUE);

    // periodically read temp values from sensor and set the sensor to power saving mode
    while(1){
      last_measurment = temperature_value;
      vTaskDelay(250 / portTICK_PERIOD_MS);
      i2c_master_read_temp(I2C_MASTER_NUM,&temperature_value);
      ESP_LOGI(TAG,"Temperature is : %d",temperature_value);
      duty_c = duty_c - (last_measurment-temperature_value)*100;
      ESP_LOGI(TAG,"Duty is : %d",duty_c);
      ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, duty_c);
      ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);
      vTaskDelay(2500 / portTICK_PERIOD_MS);

    }
}

void app_main(void)
{
    // sensor handling task
    xTaskCreate(i2c_temperature_task, "i2c_temperature_task", 1024 * 2, (void *)0, 10, NULL);
}