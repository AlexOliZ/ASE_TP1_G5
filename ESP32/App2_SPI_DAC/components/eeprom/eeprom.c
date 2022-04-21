#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "esp_log.h"
#include "eeprom.h"

#define TAG "EEPROM"

#define EEPROM_HOST VSPI_HOST

#define GPIO_CS		5
#define GPIO_MISO	19
#define GPIO_MOSI	23
#define GPIO_SCLK	18

// Initialize devive

void spi_master_init(EEPROM_t * dev)
{	 
	esp_err_t ret;

	ESP_LOGI(TAG, "GPIO_MISO=%d",GPIO_MISO);
	ESP_LOGI(TAG, "GPIO_MOSI=%d",GPIO_MOSI);
	ESP_LOGI(TAG, "GPIO_SCLK=%d",GPIO_SCLK);
	ESP_LOGI(TAG, "GPIO_CS=%d",GPIO_CS);
	//gpio_pad_select_gpio( GPIO_CS );
	gpio_reset_pin(GPIO_CS);
	gpio_set_direction(GPIO_CS, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_CS, 0);

	spi_bus_config_t buscfg = {
		.sclk_io_num = GPIO_SCLK,
		.mosi_io_num = GPIO_MOSI,
		.miso_io_num = GPIO_MISO,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1
	};

	ret = spi_bus_initialize( EEPROM_HOST, &buscfg, SPI_DMA_CH_AUTO );
	ESP_LOGI(TAG, "spi_bus_initialize=%d",ret);
	ESP_ERROR_CHECK(ret);

	int SPI_Frequency = SPI_MASTER_FREQ_8M;
	dev->_totalBytes = 512;
	dev->_addressBits = 9;
	//dev->_pageSize = 16;
	//dev->_lastPage = (dev->_totalBytes/dev->_pageSize)-1;

	spi_device_interface_config_t devcfg = {
		.clock_speed_hz = SPI_Frequency,
		.spics_io_num = GPIO_CS,
		.queue_size = 1,
		.mode = 0,
	};
	spi_device_handle_t handle;
	ret = spi_bus_add_device( EEPROM_HOST, &devcfg, &handle);
	ESP_LOGI(TAG, "spi_bus_add_device=%d",ret);
	ESP_ERROR_CHECK(ret);
	
	dev->_SPIHandle = handle;
}

//
// Read Status Register (RDSR)
// reg(out):Status
//
esp_err_t eeprom_ReadStatusReg(EEPROM_t * dev, uint8_t * reg)
{
	
	uint8_t data[1];
	spi_transaction_t SPITransaction = {
		.cmd = EEPROM_CMD_RDSR,
		.length = 2 * 8,
		.rx_buffer = data,
	};
	esp_err_t ret = spi_device_transmit(dev->_SPIHandle, &SPITransaction);
	ESP_ERROR_CHECK(ret);
	ESP_LOGI(TAG, "eeprom_ReadStatusReg=%x",data[0]);
	*reg = data[0];
	return ret;
}

//
// Busy check
// true:Busy
// false:Idle
//
bool eeprom_IsBusy(EEPROM_t * dev)
{
	uint8_t data[1];
	spi_transaction_t SPITransaction = {
		.cmd = EEPROM_CMD_RDSR,
		.length = 2 * 8,
		.rx_buffer = data,
	};
	esp_err_t ret = spi_device_transmit( dev->_SPIHandle, &SPITransaction );
	ESP_ERROR_CHECK(ret);
	if (ret != ESP_OK) return false;
	if( (data[0] & EEPROM_STATUS_WIP) == EEPROM_STATUS_WIP) return true;
	return false;
}


//
// Write enable check
// true:Write enable
// false:Write disable
//
bool eeprom_IsWriteEnable(EEPROM_t * dev)
{
	uint8_t data[0];
	spi_transaction_t SPITransaction = {
		.cmd = EEPROM_CMD_RDSR,
		.length = 2 * 8,
		.rx_buffer = data,
	};
	esp_err_t ret = spi_device_transmit( dev->_SPIHandle, &SPITransaction );
	ESP_ERROR_CHECK(ret);
	if (ret != ESP_OK) return false;
	if( (data[0] & EEPROM_STATUS_WEL) == EEPROM_STATUS_WEL) return true;
	return false;
}

//
// Set write enable
//
esp_err_t eeprom_WriteEnable(EEPROM_t * dev)
{
	uint8_t data[1];
	data[0] = EEPROM_CMD_WREN;
	spi_transaction_t SPITransaction = {
		.length = 1 * 8,
		.tx_buffer = data,
	};
	esp_err_t ret = spi_device_transmit( dev->_SPIHandle, &SPITransaction );
	ESP_ERROR_CHECK(ret);
	return ret;
}

//
// Set write disable
//
esp_err_t eeprom_WriteDisable(EEPROM_t * dev)
{
	uint8_t data[1];
	data[0] = EEPROM_CMD_WRDI;
	spi_transaction_t SPITransaction = {
		.length = 1 * 8,
		.tx_buffer = data,
	};
	esp_err_t ret = spi_device_transmit( dev->_SPIHandle, &SPITransaction );
	ESP_ERROR_CHECK(ret);
	return ret;
}

//
// Read from Memory Array (READ)
// addr(in):Read start address (16-Bit Address)
// buf(out):Read data
// n(in):Number of data bytes to read
//
int16_t eeprom_Read(EEPROM_t * dev, uint16_t addr, uint8_t *buf, int16_t n)
{ 
	esp_err_t ret;

	if (addr >= dev->_totalBytes) return 0;

	uint8_t data[3];
	for (int i=0;i<n;i++) {
		uint16_t _addr = addr + i;
		
		data[0] = EEPROM_CMD_READ;
		if(addr>0xFF){
			data[0] = (data[0] | 0x08);
		}
		data[1] = (_addr & 0xFF);
		spi_transaction_t SPITransaction = {
			// .cmd = command,
			// .addr = (addr & 0xFF),
			.length = 3 * 8,
			.tx_buffer = data,
			.rx_buffer = data,
		};

		ret = spi_device_transmit( dev->_SPIHandle, &SPITransaction );
		ESP_ERROR_CHECK(ret);
		if (ret != ESP_OK) return 0;
		buf[i] = data[2];
	}
	return n;
}

//
// Write to Memory Array (WRITE)
// addr(in):Write start address (16-Bit Address)
// wdata(in):Write data
//
int16_t eeprom_WriteByte(EEPROM_t * dev, uint16_t addr, uint8_t wdata)
{
	esp_err_t ret;

	if (addr >= dev->_totalBytes) return 0;

	// Set write enable
	ret = eeprom_WriteEnable(dev);
	ESP_ERROR_CHECK(ret);
	if (ret != ESP_OK) return 0;

	// Wait for idle
	while( eeprom_IsBusy(dev) ) {
		vTaskDelay(1);
	}
	
	uint8_t data[3];
	data[0] = EEPROM_CMD_WRITE;
	if(addr>0xFF){
		data[0] = (data[0] | 0x08);
	}
	data[1] = (addr & 0xFF);
	data[2] = wdata;
	spi_transaction_t SPITransaction = {
		// .cmd = command,
		// .addr = (addr & 0xFF),
		.length = 3 * 8,
		.tx_buffer = data,
	};
	
	ret = spi_device_transmit( dev->_SPIHandle, &SPITransaction );
	ESP_ERROR_CHECK(ret);
	if (ret != ESP_OK) return 0;

	// Wait for idle
	while( eeprom_IsBusy(dev) ) {
		vTaskDelay(1);
	}
	return 1;
}

// Get total byte
//
int32_t eeprom_TotalBytes(EEPROM_t * dev)
{
	return dev->_totalBytes;
}

// Get page size
//
// int16_t eeprom_PageSize(EEPROM_t * dev)
// {
// 	return dev->_pageSize;
// }

// Get last page
//
// int16_t eeprom_LastPage(EEPROM_t * dev)
// {
// 	return dev->_lastPage;
// }