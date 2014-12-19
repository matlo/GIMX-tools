/*
 * License: GPLv3
 * 
 * Compile with: gcc -o t300rs_ffb t300rs_ffb.c -lusb-1.0
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32
#include <libusb-1.0/libusb.h>
#else
#include <libusb-1.0/libusb.h>
#endif

#include <dirent.h>

#ifdef WIN32
#include <sys/stat.h>
#define LINE_MAX 1024
#endif

#define VENDOR 0x044F
#define PRODUCT 0xB66D

#define INTERRUPT_OUT_ENDPOINT 0x03

#define REPORT_TYPE_FEATURE 0x0300
#define TRANSFER_TIMEOUT 1000 //ms

#if !defined(LIBUSB_API_VERSION) && !defined(LIBUSBX_API_VERSION)
const char * LIBUSB_CALL libusb_strerror(enum libusb_error errcode)
{
  return libusb_error_name(errcode);
}
#endif

typedef enum
{
  E_CONTROL_GET_FEATURE,
  E_INTERRUPT_OUT
} e_transfer_type;

typedef struct
{
  e_transfer_type type;
  unsigned char feature;
  unsigned char length;
  int delay_ms;
  unsigned char buffer[64];
} s_transfer;

static int transfers_nb = 0;
static s_transfer* transfers = NULL;

static s_transfer cleanup =
{
  .type = E_INTERRUPT_OUT,
  .delay_ms = 4,
  .buffer = {0x38, 0x11, 0xff, 0xff}
};

static libusb_device_handle* devh = NULL;

void dump(unsigned char* data, unsigned char length)
{
  int i;
  int zeros = 0;
  for(i=length-1; i>=0; --i)
  {
    if(data[i] != 0x00)
    {
      break;
    }
    ++zeros;
  }
  for(i=0; i<length-zeros; ++i)
  {
    printf("%02x ", data[i]);
  }
  printf("\n");
}

static int send_transfers = 1;

int process_transfer(s_transfer* transfer)
{
  int res = 0;

  printf("sleep %d ms\n", transfer->delay_ms);
  usleep(transfer->delay_ms*1000);

  if (transfer->feature)
  {
    printf("get feature %02x (%d bytes)\n", transfer->feature, transfer->length);

    if(send_transfers)
    {
      res = libusb_control_transfer(devh, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
          LIBUSB_REQUEST_CLEAR_FEATURE, REPORT_TYPE_FEATURE | transfer->feature, 0x0000, transfer->buffer, transfer->length, TRANSFER_TIMEOUT);

      if (res < 0)
      {
        fprintf(stderr, "Control transfer failed: %s.\n", libusb_strerror(res));
      }
      else
      {
        printf("  ");
        dump(transfer->buffer, sizeof(transfer->buffer));
      }
    }
  }
  else
  {
    printf("send interrupt: ");

    dump(transfer->buffer, sizeof(transfer->buffer));

    if(send_transfers)
    {
      int transferred = 0;
      res = libusb_interrupt_transfer(devh, INTERRUPT_OUT_ENDPOINT,
          transfer->buffer, sizeof(transfer->buffer), &transferred, TRANSFER_TIMEOUT);

      if (res < 0)
      {
        fprintf(stderr, "Interrupt transfer failed: %s.\n", libusb_strerror(res));
      }
    }
  }

  return res;
}

int process_device(libusb_device_handle* devh)
{
  int i;
  int res;
  for (i=0; i<transfers_nb && res >= 0; ++i)
  {
    res = process_transfer(transfers+i);
  }
  return res;
}

int read_file() {
  int ret = 0;
  char line[LINE_MAX];
  DIR *dirp;
  FILE* fp;
  char file_path[PATH_MAX];
  struct dirent *d;
  unsigned int i;
#ifdef WIN32
  struct stat buf;
#endif

  dirp = opendir(".");
  if (dirp == NULL)
  {
    fprintf(stderr, "Can't open directory.\n");
    return -1;
  }

  printf("Choose a file:\n");

  char* files[64] = {};
  int files_nb = 0;

  while ((d = readdir(dirp)) && files_nb < sizeof(files)/sizeof(*files))
  {
    if(!strstr(d->d_name, ".ffb"))
    {
      continue;
    }
#ifndef WIN32
    if (d->d_type != DT_REG)
    {
      continue;
    }
#else
    snprintf(file_path, sizeof(file_path), "%s", d->d_name);
    if(stat(file_path, &buf) == 0)
    {
      if(!S_ISREG(buf.st_mode))
      {
        continue;
      }
    }
#endif
    printf("%d %s\n", files_nb, d->d_name);
    files[files_nb] = strdup(d->d_name);
    ++files_nb;
  }

  closedir(dirp);

  if(!files_nb)
  {
    fprintf(stderr, "No .ffb file found.\n");
    return -1;
  }

  int choice = -1;
  if(scanf("%d", &choice) < 1)
  {
    fprintf(stderr, "Invalid choice.\n");
    return -1;
  }

  if(choice >= 0 && choice < files_nb)
  {
    fp = fopen(files[choice], "r");
    if (!fp) {
      fprintf(stderr, "Can not open '%s'\n", files[choice]);
    } else {

      while (fgets(line, LINE_MAX, fp) && !ret) {
        if (line[0] != '#' && line[0] != '\n') {
          void* ptr = realloc(transfers, (transfers_nb+1)*sizeof(*transfers));
          if(ptr)
          {
            transfers = ptr;
            s_transfer* transfer = transfers+transfers_nb;
            memset(transfer, 0x00, sizeof(*transfers));

            int value;

            int index = 0;

            if(sscanf(line+index, "%04x", &value) < 1)
            {
              fprintf(stderr, "Invalid line: %s\n", line);
              ret = -1;
              continue;
            }

            transfer->delay_ms = value;

            index += 5;

            if(sscanf(line+index, "%02x", &value) < 1)
            {
              fprintf(stderr, "Invalid line: %s\n", line);
              ret = -1;
              continue;
            }

            transfer->type = value;

            index += 3;

            switch(transfer->type)
            {
              case E_CONTROL_GET_FEATURE:
                if(sscanf(line+index, "%02x", &value) < 1)
                {
                  fprintf(stderr, "Invalid line: %s\n", line);
                  ret = -1;
                  continue;
                }
                else
                {
                  transfer->feature = value;
                  index += 3;
                }
                if(sscanf(line+index, "%02x", &value) < 1)
                {
                  fprintf(stderr, "Invalid line: %s\n", line);
                  ret = -1;
                  continue;
                }
                else
                {
                  transfer->length = value;
                  index += 3;
                }
                break;
              case E_INTERRUPT_OUT:
                {
                  int pos = 0;
                  while(sscanf(line+index, "%02x", &value) > 0)
                  {
                    transfer->buffer[pos] = value;
                    index += 3;
                    ++pos;
                  }
                }
                break;
            }

            ++transfers_nb;
          }
          else
          {
            fprintf(stderr, "Failed to allocate a new transfer.\n");
            ret = -1;
          }
        }
      }
      fclose(fp);
    }

    for(i=0; i<files_nb; ++i)
    {
      free(files[i]);
    }
  }
  else
  {
    fprintf(stderr, "Invalid choice.\n");
    ret = -1;
  }

  return ret;
}

int main(int argc, char *argv[])
{
  libusb_context* ctx = NULL;
  int ret = -1;
  int status;
  int i;

  if(read_file() < 0)
  {
    return -1;
  }

  if(libusb_init(&ctx))
  {
    fprintf(stderr, "Can't initialize libusb.\n");
    return -1;
  }

  //libusb_set_debug(ctx, 128);

  devh = libusb_open_device_with_vid_pid(ctx, VENDOR, PRODUCT);
  if(!devh)
  {
    fprintf(stderr, "No device found on USB busses.\n");
#ifdef WIN32
    Sleep(2000);
#else
    sleep(2);
#endif
    libusb_exit(ctx);
    return -1;
  }

#if defined(LIBUSB_API_VERSION) || defined(LIBUSBX_API_VERSION)
  libusb_set_auto_detach_kernel_driver(devh, 1);
#else
#ifndef WIN32
  ret = libusb_kernel_driver_active(devh, 0);
  if(ret > 0)
  {
    ret = libusb_detach_kernel_driver(devh, 0);
    if(ret < 0)
    {
      fprintf(stderr, "Can't detach kernel driver: %s.\n", libusb_strerror(ret));
      libusb_close(devh);
      libusb_exit(ctx);
      return -1;
    }
  }
#endif
#endif
  ret = libusb_claim_interface(devh, 0);
  if(ret < 0)
  {
    fprintf(stderr, "Can't claim interface: %s.\n", libusb_strerror(ret));
    libusb_close(devh);
    libusb_exit(ctx);
    return -1;
  }

  status = process_device(devh);

  process_transfer(&cleanup);

  ret = libusb_release_interface(devh, 0);
  if(ret < 0)
  {
    fprintf(stderr, "Can't release interface: %s.\n", libusb_strerror(ret));
  }

#if !defined(LIBUSB_API_VERSION) && !defined(LIBUSBX_API_VERSION)
#ifndef WIN32
  ret = libusb_attach_kernel_driver(devh, 0);
  if(ret < 0)
  {
    fprintf(stderr, "Can't attach kernel driver: %s.\n", libusb_strerror(ret));
  }
#endif
#endif

  free(transfers);

  libusb_close(devh);
  libusb_exit(ctx);

  return ret;

}

