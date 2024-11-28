#include <ctype.h>
#include <debug.h>
#include <dmaKit.h>
#include <fcntl.h>
#include <gsKit.h>
#include <gsToolkit.h>
#include <iopcontrol.h>
#include <loadfile.h>
#include <ps2sdkapi.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Used to get BDM driver name
#include <usbhdfsd-common.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>

// Macros for loading embedded IOP modules
#define IRX_DEFINE(mod)                                                                                                                              \
  extern unsigned char mod##_irx[] __attribute__((aligned(16)));                                                                                     \
  extern unsigned int size_##mod##_irx

#define IRX_LOAD(mod)                                                                                                                                \
  logString("\tloading " #mod "\n");                                                                                                                 \
  if ((ret = SifExecModuleBuffer(mod##_irx, size_##mod##_irx, 0, NULL, &iopret)) < 0)                                                                \
    return ret;                                                                                                                                      \
  if (iopret == 1) {                                                                                                                                 \
    return iopret;                                                                                                                                   \
  }

// Embedded IOP modules required for reading from memory card
IRX_DEFINE(iomanX);
IRX_DEFINE(fileXio);
IRX_DEFINE(ps2dev9);
IRX_DEFINE(bdm);
IRX_DEFINE(bdmfs_fatfs);
IRX_DEFINE(ata_bd);


// Logs to screen and debug console
void logString(const char *str, ...) {
  va_list args;
  va_start(args, str);

  vprintf(str, args);
  scr_vprintf(str, args);

  va_end(args);
}

int iopInit() {
  // Initialize the RPC manager and reset IOP
  // SifInitRpc(0);
  // while (!SifIopReset("", 0)) {
  // };
  // while (!SifIopSync()) {
  // };

  // Initialize the RPC manager
  SifInitRpc(0);

  // // Apply patches required to load modules from EE RAM
  // if (((ret = sbv_patch_enable_lmb())) || ((ret = sbv_patch_disable_prefix_check()))) {
  //   logString("Failed to apply patch: %d\n", ret);
  //   return ret;
  // }

  int ret, iopret = 0;
  // Load embedded modules
  IRX_LOAD(iomanX);
  IRX_LOAD(fileXio);
  // IRX_LOAD(ps2dev9);
  IRX_LOAD(bdm);
  IRX_LOAD(bdmfs_fatfs);
  IRX_LOAD(ata_bd);

  return 0;
}

// Gets BDM driver name via fileXio
int getDeviceDriver(char *mountpoint) {
  int fd = fileXioDopen(mountpoint);
  if (fd < 0) {
    return -ENODEV;
  }

  char driverName[10];
  int deviceNumber;
  if (fileXioIoctl2(fd, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, driverName, sizeof(driverName) - 1) >= 0) {
    // Null-terminate the string before mapping
    driverName[sizeof(driverName) - 1] = '\0';
  }

  // Get device number
  fileXioIoctl2(fd, USBMASS_IOCTL_GET_DEVICE_NUMBER, NULL, 0, &deviceNumber, sizeof(deviceNumber));

  logString("Found device %s%d\n", driverName, deviceNumber);
  fileXioDclose(fd);
  return 0;
}

// Initializes IOP modules
int main(int argc, char *argv[]) {
  // Initialize the screen
  init_scr();

  logString("IOP init\n");
  int res = iopInit();
  if (res) {
    logString("Failed to init IOP\n");
    sleep(10);
    return -1;
  }

  logString("Opening mass0 via opendir\n");
  DIR *directory;
  for (int i = 0; i < 10; i++) {
    directory = opendir("mass0:");
    // Check if the directory can be opened
    if (directory == NULL) {
      logString("Can't open mass0:, delaying\n");
      sleep(1);
      continue;
    }
    logString("mass0 opened\n");
    closedir(directory);
  }

  logString("Getting device driver\n");
  if (getDeviceDriver("mass0:")) {
    logString("Failed to get device driver\n");
  }

  logString("Opening mass0 via opendir\n");
  directory = opendir("mass0:");
  if (directory == NULL) {
    logString("Can't open mass0:, delaying\n");
  } else
    closedir(directory);

  logString("Opening FONTM\n");
	int fontmFd = open("rom0:FONTM", O_RDONLY);
	if(fontmFd < 0)	{
		printf("Failed to open FONTM from ROM0\n");
	}

  logString("Done");
  sleep(20);
  return 0;
}
