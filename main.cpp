#include "mbed.h"


#include "FATFileSystem.h"
#include <errno.h>


#define DF

#ifdef SD
#include "SDBlockDevice.h"
DigitalOut sdpower(PB_2,1);

SDBlockDevice drive(PB_15, PB_14, PB_13, PB_12);
#endif

#ifdef DF
#include "DataFlashDevice.h"

DataFlashDevice drive(PB_15, PB_14, PB_13, PB_12, PB_2, 1000000);
#endif
FATFileSystem fs("disk");

FlashIAP flash;



void return_error(int ret_val) {
	if (ret_val)
		printf("Failure. %d\n", ret_val);
	else
		printf("done.\n");
}

void errno_error(void* ret_val) {
	if (ret_val == NULL)
		printf(" Failure. %d \n", errno);
	else
		printf(" done.\n");
}
void apply_update(FILE *file, uint32_t address);
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
	char buff[16] = { 0 };
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
int main()
{
		printf("begin.\r\n");

		int error = 0;

    drive.init();

    error =fs.mount(&drive);
	if(error==FR_OK)
	{
	
		FSTest();
    FILE *file = fopen(MBED_CONF_APP_UPDATE_FILE, "rb");
    if (file != NULL) {
        printf("Firmware update found\r\n");

        apply_update(file, POST_APPLICATION_ADDR);

        fclose(file);
        remove(MBED_CONF_APP_UPDATE_FILE);
    } else {
        printf("No update found to apply\r\n");
    }

    fs.unmount();
    drive.deinit();

	}

    printf("Starting application\r\n");

    mbed_start_application(POST_APPLICATION_ADDR);
}

void apply_update(FILE *file, uint32_t address)
{
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
