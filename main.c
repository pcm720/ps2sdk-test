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
  // Initialize the RPC manager
  SifInitRpc(0);

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
    break;
  }

  logString("Opening mass0 via fileXioDopen\n");
  int fd = fileXioDopen("mass0:/");
  if (fd < 0) {
    logString("ERROR: Failed to open: %d\n", fd);
  } else
    fileXioDclose(fd);

  logString("Creating mass0:/testdir\n");
  res = mkdir("mass0:/testdir", 0777);
  if (res) {
    logString("ERROR: Failed to create: %d\n", res);
  }

  logString("Opening FONTM\n");
  fd = open("rom0:FONTM", O_RDONLY);
  if (fd < 0) {
    logString("ERROR: Failed to open: %d\n", fd);
  } else
    close(fd);

  logString("Done");
  sleep(20);
  return 0;
}
