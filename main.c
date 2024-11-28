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
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>
#include <usbhdfsd-common.h>

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

IRX_DEFINE(iomanX);
IRX_DEFINE(fileXio);
IRX_DEFINE(fileXio_verbose);
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

int iopInit(int verbose) {
  // Uncomment commented code for PCSX2 (note that the issue is not reproducible there)
  // Initialize the RPC manager and reset IOP
  // SifInitRpc(0);
  // while (!SifIopReset("", 0)) {
  // };
  // while (!SifIopSync()) {
  // };

  // Initialize the RPC manager
  SifInitRpc(0);

  int ret, iopret = 0;
  // // Apply patches required to load modules from EE RAM
  // if ((ret = sbv_patch_enable_lmb()))
  //   return ret;
  // if ((ret = sbv_patch_disable_prefix_check()))
  //   return ret;

  // Load embedded modules
  IRX_LOAD(iomanX);
  if (verbose) {
    IRX_LOAD(fileXio_verbose);
  } else {
    IRX_LOAD(fileXio);
  }
  // IRX_LOAD(ps2dev9);
  IRX_LOAD(bdm);
  IRX_LOAD(bdmfs_fatfs);
  IRX_LOAD(ata_bd);

  return 0;
}

// Initializes IOP modules
int main(int argc, char *argv[]) {
  // Initialize the screen
  init_scr();

  logString("IOP init\n");
  // Using iopInit(0) to load the regular fileXio makes the example fail on loading rom0:FONTM.
  // Using iopInit(1) to load fileXio_verbose makes the example succeed.
  int res = iopInit(0);
  if (res) {
    logString("Failed to init IOP\n");
    sleep(10);
    return -1;
  }

  logString("Waiting for BDM\n");
  sleep(5);

  logString("Opening mass0 via open\n");
  int fd = open("mass0:", O_RDONLY | O_DIRECTORY);
  if (fd < 0) {
    logString("ERROR: Failed to open: %d\n", fd);
  } else {
    // Not using fileXioIoctl2 makes the example succeed
    char driverName[10];
    logString("Getting driver name via fileXioIoctl2\n");
    res = fileXioIoctl2(ps2sdk_get_iop_fd(fd), USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, driverName, sizeof(driverName));
    if (res >= 0) {
      driverName[sizeof(driverName) - 1] = '\0';
      logString("Driver name is %s\n", driverName);
    } else {
      logString("Failed to get driver name: %d\n", res);
    }
    close(fd);
  }

  // fileXioExit(); // Uncommenting this makes the example succeed

  // Not creating the directory makes the example succeed
  logString("Creating mass0:/testdir\n");
  res = mkdir("mass0:/testdir", 0777);
  if (res) {
    logString("ERROR: Failed to create: %d\n", res);
  }

  // Always fails with non-verbose fileXio after fileXioIoctl2 and mkdir are called
  logString("Opening FONTM\n");
  fd = open("rom0:FONTM", O_RDONLY);
  if (fd < 0) {
    logString("ERROR: Failed to open: %d\n", fd);
  } else
    close(fd);

  logString("Done");
  sleep(10);
  return 0;
}
