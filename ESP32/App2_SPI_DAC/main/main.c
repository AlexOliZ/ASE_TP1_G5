#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "eeprom.h"
#include "esp_event.h"
#include "freertos/queue.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "driver/dac_common.h"
#include "esp_system.h"

static const char *TAG = "MAIN";

#define	EEPROM_MODEL LC040A

void dump(uint8_t *dt, int n)
{
	uint16_t clm = 0;
	uint8_t data;
	uint32_t saddr =0;
	uint32_t eaddr =n-1;

	printf("--------------------------------------------------------\n");
	uint32_t addr;
	for (addr = saddr; addr <= eaddr; addr++) {
		data = dt[addr];
		if (clm == 0) {
			printf("%05x: ",addr);
		}

		printf("%02x ",data);
		clm++;
		if (clm == 16) {
			printf("| \n");
			clm = 0;
		}
	}
	printf("--------------------------------------------------------\n");
}


void app_main(void)
{
    ESP_LOGI(TAG, "EEPROM_MODEL=%s", "24LC040A");
	EEPROM_t dev;
	spi_master_init(&dev);
	int32_t totalBytes = eeprom_TotalBytes(&dev);
	ESP_LOGI(TAG, "totalBytes=%d Bytes",totalBytes);
	int16_t pageSize = eeprom_PageSize(&dev);
	ESP_LOGI(TAG, "pageSize=%d Bytes",pageSize);
	int16_t lastPage = eeprom_LastPage(&dev);
	ESP_LOGI(TAG, "lastPage=%d Page",lastPage);

	// Get Status Register
	uint8_t reg;
	esp_err_t ret;
	ret = eeprom_ReadStatusReg(&dev, &reg);
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "ReadStatusReg Fail %d",ret);
		while(1) { vTaskDelay(1); }
	} 
	ESP_LOGI(TAG, "readStatusReg : 0x%02x", reg);

	uint8_t wdata[128];
	int len;
	dac_output_enable(DAC_CHANNEL_1);

    //write increase
    for (int i=0; i<128; i++) {
		wdata[i]=i;	
		dac_output_voltage(DAC_CHANNEL_1, i);
		vTaskDelay(30 / portTICK_PERIOD_MS); 
	}  

	for (int addr=0; addr<128;addr++) {
		len =  eeprom_WriteByte(&dev, addr, wdata[addr]);
		ESP_LOGI(TAG, "WriteByte(addr=%d) len=%d", addr, len);
		if (len != 1) {
			ESP_LOGI(TAG, "WriteByte Fail addr=%d", addr);
			while(1) { vTaskDelay(1); }
		}
	}

	// Read 128 byte from Address=0
	uint8_t rbuf[128];
	memset(rbuf, 0, 128);
	len =  eeprom_Read(&dev, 0, rbuf, 128);
	if (len != 128) {
		ESP_LOGI(TAG, "Read Fail");
		while(1) { vTaskDelay(1); }
	}
	ESP_LOGI(TAG, "Read Data: len=%d", len);
	dump(rbuf, 128);
}