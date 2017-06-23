#include "mbed.h"

#include "FATFileSystem.h"
#include <errno.h>
#include "stm32l1xx_hal_flash.h"

#define SD

#ifdef SD
#include "SDBlockDevice.h"

SDBlockDevice drive(PB_15, PB_14, PB_13, PB_12);
//SDBlockDevice drive(MBED_CONF_SD_SPI_MOSI, MBED_CONF_SD_SPI_MISO, MBED_CONF_SD_SPI_CLK, MBED_CONF_SD_SPI_CS);
//
#endif

#ifdef DF
#include "DataFlashDevice.h"

DataFlashDevice drive(PB_15, PB_14, PB_13, PB_12, PB_2, 1000000);
//DataFlashDevice drive(MBED_CONF_SD_SPI_MOSI, MBED_CONF_SD_SPI_MISO, MBED_CONF_SD_SPI_CLK, MBED_CONF_SD_SPI_CS, PB_2, 1000000);

#endif
FATFileSystem fs("disk");

FlashIAP flash;

void return_error(int ret_val) {
	if (ret_val) {
#ifndef NDEBUG
		printf("Failure. %d\n", ret_val);
#endif
	} else {
#ifndef NDEBUG
		printf("done.\n");
#endif
	}
}

void errno_error(void* ret_val) {
	if (ret_val == NULL) {
#ifndef NDEBUG
		printf(" Failure. %d \n", errno);
#endif
	} else {
#ifndef NDEBUG
		printf(" done.\n");
#endif
	}
}
void apply_update(FILE *file, uint32_t address);
#ifdef fstest
bool FSTest() {
	int error = 0;

	printf("Opening root directory.");
	DIR* dir = opendir("/disk/");

	struct dirent* de;
	printf("Printing all filenames:\r\n");
	while ((de = readdir(dir)) != NULL) {
		printf("  %s\r\n", &(de->d_name)[0]);
	}

	printf("Closeing root directory. ");
	error = closedir(dir);
	return_error(error);
	printf("Filesystem Demo complete.\r\n");

	printf("Opening a new file, numbers.txt.");
	FILE* fd = fopen("/disk/numbers.txt", "w+");
	errno_error(fd);

	for (int i = 0; i < 20; i++) {
		printf("Writing decimal numbers to a file (%d/20)\r\n", i);
		fprintf(fd, "%d\r\n", i);
	}
	printf("Writing decimal numbers to a file (20/20) done.\r\n");

	printf("Closing file.");
	fclose(fd);
	printf(" done.\r\n");

	printf("Re-opening file read-only.");
	fd = fopen("/disk/numbers.txt", "r");
	errno_error(fd);

	printf("Dumping file to screen.\r\n");
	char buff[16] = {0};
	while (!feof(fd)) {
		int size = fread(&buff[0], 1, 15, fd);
		fwrite(&buff[0], 1, size, stdout);
	}
	printf("EOF.\r\n");

	printf("Closing file.");
	fclose(fd);
	printf(" done.\r\n");

	printf("Opening root directory.");
	dir = opendir("/disk/");
	errno_error(fd);

	printf("Printing all filenames:\r\n");
	while ((de = readdir(dir)) != NULL) {
		printf("  %s\r\n", &(de->d_name)[0]);
	}

	printf("Closing root directory. ");
	error = closedir(dir);
	return_error(error);
	printf("Filesystem Demo complete.\r\n");
}
#endif
int erase_flash(uint32_t start, uint32_t end) {
	uint32_t PAGEError;
	FLASH_EraseInitTypeDef EraseInitStruct;
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
	EraseInitStruct.PageAddress = start;
	EraseInitStruct.NbPages = (end - start) / FLASH_PAGE_SIZE;
	//printf("erase %x\r\n", start);

	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(
			FLASH_FLAG_EOP | FLASH_FLAG_PGAERR | FLASH_FLAG_WRPERR);

	if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) == HAL_OK) {
		HAL_FLASH_Lock();
	} else {
#ifndef NDEBUG
		printf("erase failed at %x\r\n", PAGEError);
#endif
		return -1;
	}
	return 0;
}
int program_flash(uint32_t start, uint32_t end, char* data) {
	char* ptr = data;
	uint32_t addr = start;
	while (addr < end) {
		HAL_FLASH_Unlock();
#ifndef NDEBUG
		printf("program %x\r\n",addr);
#endif
		if (HAL_FLASH_Program(TYPEPROGRAM_WORD, addr, *(uint32_t*) ptr)
				== HAL_OK) {
			HAL_FLASH_Lock();
		} else {
#ifndef NDEBUG
			printf("program failed at %x\r\n", addr);
#endif
			return -1;
		}
		addr += 4;
		ptr += 4;
	}
	return 0;
}

int main() {
	wait_ms(100);
#ifndef NDEBUG
	printf("begin.\r\n");
#endif
	int error = 0;

	if (drive.init() == 0) {

		error = fs.mount(&drive);
		if (error == FR_OK) {

			//FSTest();
			FILE *file = fopen(MBED_CONF_APP_UPDATE_FILE, "rb");
			if (file != NULL) {
#ifndef NDEBUG
				printf("Firmware update found\r\n");
#endif

				apply_update(file, POST_APPLICATION_ADDR);

				rewind(file);
				uint32_t buffer;
				uint32_t *ptr;
				ptr = (uint32_t*) POST_APPLICATION_ADDR;
				while (true) {

					// Read data for this page
					int size_read = fread(&buffer, 1, 4, file);
					if (size_read <= 0) {
						break;
					}
					if (buffer != *ptr) {
#ifndef NDEBUG
						printf("verify failed at %x, should be %x read %x\r\n",ptr, buffer, *ptr);
#endif
					}
					ptr++;
				}

				fclose(file);
				remove (MBED_CONF_APP_UPDATE_FILE);
			} else {
#ifndef NDEBUG
				printf("No update found to apply\r\n");
#endif
			}

			fs.unmount();
			drive.deinit();

		}
	}
#ifndef NDEBUG
	printf("Starting application\r\n");
#endif
	mbed_start_application (POST_APPLICATION_ADDR);
}
void apply_update2(FILE *file, uint32_t address) {
	flash.init();

	const uint32_t page_size = flash.get_page_size();
	char *page_buffer = new char[page_size];
	uint32_t addr = address;
	uint32_t next_sector = addr + flash.get_sector_size(addr);
	bool sector_erased = false;
	while (true) {

		// Read data for this page
		memset(page_buffer, 0, sizeof(page_buffer));
		int size_read = fread(page_buffer, 1, page_size, file);
		if (size_read <= 0) {
			break;
		}

		// Erase this page if it hasn't been erased
		if (!sector_erased) {
			flash.erase(addr, flash.get_sector_size(addr));
			sector_erased = true;
		}

		// Program page
		flash.program(page_buffer, addr, page_size);

		addr += page_size;
		if (addr >= next_sector) {
			next_sector = addr + flash.get_sector_size(addr);
			sector_erased = false;

		}
	}
	delete[] page_buffer;

	flash.deinit();
}
void apply_update(FILE *file, uint32_t address) {
	uint32_t page_size = FLASH_PAGE_SIZE;
	char page_buffer[FLASH_PAGE_SIZE];
	uint32_t addr = address;
	while (true) {

		// Read data for this page
		memset(page_buffer, 0, page_size);
		int size_read = fread(page_buffer, 1, page_size, file);
		if (size_read <= 0) {
			break;
		}

		erase_flash(addr, addr + page_size);

		program_flash(addr, addr + page_size, page_buffer);
		addr += page_size;

	}

	delete[] page_buffer;

}
