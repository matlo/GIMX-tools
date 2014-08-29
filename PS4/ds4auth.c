/*
 Copyright (c) 2014 Mathieu Laurendeau
 License: GPLv3

 This tool is similar to the Ds4AuthTool.py from Frank Zhao,
 but it's written in C and it exhibits the asynchronous IO capabilities of the libusb.

 Compile: gcc -o ds4auth ds4auth.c -lusb-1.0
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <err.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/types.h>

#include <sched.h>

#include <libusb-1.0/libusb.h>

#define VENDOR 0x054c
#define PRODUCT 0x05c4

#define MAX_DATA_LENGTH 0x40

#define CHALLENGES_NB 0x05
#define RESPONSE_NB 0x12

static unsigned char challenges[CHALLENGES_NB][MAX_DATA_LENGTH] = {
    { 0xf0, 0x01, 0x00, 0x00, 0x5f, 0x3b, 0x74, 0x00, 0xc0, 0xa0, 0xc4, 0xd5, 0xfe, 0x05, 0x17, 0x5a, 0x02, 0x41, 0xf2, 0xf3, 0x73, 0x4f, 0x88, 0x76, 0x56, 0xa5, 0xf4, 0x1e, 0xac, 0xc1, 0x29, 0x92, 0xbc, 0xa7, 0x3d, 0x80, 0xfc, 0x9c, 0x97, 0xf0, 0x56, 0xa5, 0x4f, 0xab, 0x5b, 0x44, 0xf7, 0x8d, 0xbd, 0xb6, 0x7d, 0x4b, 0x40, 0xca, 0x49, 0xd3, 0x85, 0xf7, 0xba, 0xff, 0x6b, 0xb0, 0x73, 0x41 },
    { 0xf0, 0x01, 0x01, 0x00, 0x21, 0x6b, 0x0d, 0x0d, 0x8d, 0x6f, 0x82, 0x2c, 0x48, 0x56, 0x79, 0xa7, 0x97, 0x0c, 0xf7, 0x1e, 0xc1, 0xab, 0xec, 0x19, 0x45, 0x3a, 0xbc, 0x95, 0x08, 0x4d, 0x43, 0x77, 0xf6, 0xdc, 0x43, 0x1c, 0x88, 0x26, 0x82, 0x35, 0x7b, 0xbf, 0xeb, 0xbb, 0x87, 0xd2, 0x1e, 0x40, 0xcd, 0x27, 0xed, 0xbf, 0x5c, 0xea, 0x8c, 0xea, 0x5c, 0xbe, 0x94, 0x81, 0xe4, 0x49, 0xdc, 0x18 },
    { 0xf0, 0x01, 0x02, 0x00, 0x74, 0xb0, 0xd1, 0x28, 0x49, 0xb4, 0x62, 0x5d, 0x7c, 0x64, 0xa3, 0x21, 0xaa, 0xbc, 0xee, 0x10, 0x9f, 0x64, 0xe2, 0xa6, 0xa9, 0xd2, 0xf7, 0xbe, 0x9d, 0x77, 0x4d, 0x1f, 0x79, 0xca, 0xf4, 0x8e, 0xaa, 0x8f, 0x91, 0x4e, 0x76, 0x85, 0x94, 0x0a, 0x45, 0xbd, 0x05, 0x22, 0x5c, 0x6d, 0xcf, 0x8f, 0x5f, 0x0a, 0xaf, 0x97, 0x1f, 0xdd, 0x7f, 0x2a, 0xdd, 0x7c, 0xc0, 0x61 },
    { 0xf0, 0x01, 0x03, 0x00, 0xe7, 0x2e, 0xe9, 0xe2, 0xa8, 0x8d, 0x98, 0xa3, 0x81, 0xe7, 0x3a, 0xe5, 0xdc, 0x02, 0x2d, 0xa0, 0xad, 0x2e, 0xbd, 0x15, 0x4d, 0x16, 0x45, 0x76, 0x99, 0x2d, 0x81, 0xac, 0x3e, 0xb9, 0xd6, 0x38, 0xd8, 0xb3, 0x25, 0x2d, 0x9e, 0x8b, 0xdc, 0x73, 0x06, 0xa8, 0xc2, 0xc1, 0x27, 0x6d, 0x18, 0xec, 0x19, 0xe4, 0xca, 0x50, 0xd7, 0x19, 0x38, 0x6c, 0xc2, 0x01, 0x16, 0xbd },
    { 0xf0, 0x01, 0x04, 0x00, 0x49, 0xb1, 0x03, 0xfd, 0x06, 0x7d, 0x2f, 0x6f, 0xe6, 0x4e, 0x55, 0x6d, 0xe2, 0x9e, 0x68, 0x3a, 0x9b, 0x90, 0xd9, 0x7a, 0x79, 0x6c, 0x16, 0x66, 0x01, 0xd3, 0x95, 0x4f, 0xaf, 0xc5, 0x2f, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0xd0, 0xc0, 0x1a },
};

static enum
{
  E_STEP_F0,
  E_STEP_F2,
  E_STEP_F1,
  E_STEP_DONE
} step = E_STEP_F0;

static unsigned char count = 0;

static int debug = 0;

static volatile int done = 0;

void terminate(int sig)
{
  done = 1;
}

void dump(unsigned char* buf, int len)
{
  int i;
  for(i=0; i<len; ++i)
  {
    printf("0x%02x ", buf[i]);
    if(!((i+1)%8))
    {
      printf("\n");
    }
  }
  printf("\n");
}

void callback(struct libusb_transfer *transfer)
{
  struct timeval t;
  gettimeofday(&t, NULL);
  printf("%ld.%06ld ", t.tv_sec, t.tv_usec);

  struct libusb_control_setup* setup = libusb_control_transfer_get_setup(transfer);

  if(transfer->status != LIBUSB_TRANSFER_COMPLETED)
  {
    fprintf(stderr, "libusb_transfer failed: bmRequestType=0x%02x, bRequest=0x%02x, wValue=0x%04x\n", setup->bmRequestType, setup->bRequest, setup->wValue);
    done = 1;
    return;
  }

  printf("libusb_transfer: bmRequestType=0x%02x, bRequest=0x%02x, wValue=0x%04x\n", setup->bmRequestType, setup->bRequest, setup->wValue);

  if(step == E_STEP_F0)
  {
    ++count;
    if(count == CHALLENGES_NB)
    {
      step = E_STEP_F2;
    }
    send_next_transfer(transfer->dev_handle);
  }
  else if(step == E_STEP_F2)
  {
    unsigned char *data = libusb_control_transfer_get_data(transfer);
    dump(data, transfer->actual_length);
    if(data[2] == 0x00)
    {
      count = 0;
      step = E_STEP_F1;
    }
    else
    {
      sleep(1);
    }
    send_next_transfer(transfer->dev_handle);
  }
  else if(step == E_STEP_F1)
  {
    dump(libusb_control_transfer_get_data(transfer), transfer->actual_length);
    ++count;
    if(count == RESPONSE_NB)
    {
      step = E_STEP_DONE;
      done = 1;
    }
    else
    {
      send_next_transfer(transfer->dev_handle);
    }
  }
}

int send_next_transfer(libusb_device_handle* devh)
{
  struct libusb_transfer* transfer = libusb_alloc_transfer(0);

  transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER ;

  unsigned char* buffer = malloc(LIBUSB_CONTROL_SETUP_SIZE+MAX_DATA_LENGTH);

  uint8_t bmRequestType;
  uint8_t bRequest;
  uint16_t wValue = 0x0300;
  uint16_t wIndex = 0x0000;
  uint16_t wLength;

  switch(step)
  {
    case E_STEP_F0:
      bmRequestType = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
      bRequest = LIBUSB_REQUEST_SET_CONFIGURATION;
      wValue |= 0xf0;
      wLength = 0x0040;
      memcpy(buffer+LIBUSB_CONTROL_SETUP_SIZE, challenges[count], MAX_DATA_LENGTH);
      break;
    case E_STEP_F2:
      bmRequestType = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
      bRequest = LIBUSB_REQUEST_CLEAR_FEATURE;
      wValue |= 0xf2;
      wLength = 0x0010;
      break;
    case E_STEP_F1:
      bmRequestType = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
      bRequest = LIBUSB_REQUEST_CLEAR_FEATURE;
      wValue |= 0xf1;
      wLength = 0x0040;
      break;
  }

  libusb_fill_control_setup(buffer, bmRequestType, bRequest, wValue, wIndex, wLength);
  libusb_fill_control_transfer(transfer, devh, buffer, callback, (void*)42, 1000);

  int ret = libusb_submit_transfer(transfer);
  if(ret < 0)
  {
    fprintf(stderr, "libusb_submit_transfer: %s.\n", libusb_strerror(ret));
    return -1;
  }

  struct timeval t;
  gettimeofday(&t, NULL);
  printf("%ld.%06ld ", t.tv_sec, t.tv_usec);

  printf("libusb_submit_transfer: bmRequestType=0x%02x, bRequest=0x%02x, wValue=0x%04x\n", bmRequestType, bRequest, wValue);

  return 0;
}

#define MAX_FD 64

int main(int argc, char *argv[])
{
  int ret;

  libusb_device** devs;
  libusb_device_handle* devh = NULL;
  libusb_context* ctx = NULL;

  /*
   * Set highest priority & scheduler policy.
   */
  struct sched_param p =
  { .sched_priority = sched_get_priority_max(SCHED_FIFO) };

  sched_setscheduler(0, SCHED_FIFO, &p);

  setlinebuf(stdout);

  (void) signal(SIGINT, terminate);

  ret = libusb_init(&ctx);
  if(ret < 0)
  {
    fprintf(stderr, "libusb_init: %s.\n", libusb_strerror(ret));
    return -1;
  }

  ssize_t cnt = libusb_get_device_list(ctx, &devs);

  int i;
  for(i=0; i<cnt; ++i)
  {
    struct libusb_device_descriptor desc;
    ret = libusb_get_device_descriptor(devs[i], &desc);
    if(!ret)
    {
      if(desc.idVendor == VENDOR && desc.idProduct == PRODUCT)
      {
        ret = libusb_open(devs[i], &devh);
        if(!ret)
        {
          libusb_set_auto_detach_kernel_driver(devh, 1);

          ret = libusb_claim_interface(devh, 0);
          if(ret < 0)
          {
            fprintf(stderr, "Can't claim interface: %s.\n", libusb_strerror(ret));
            ret = -1;
          }
          else
          {
            break;
          }
        }
      }
    }
  }

  libusb_free_device_list(devs, 1);

  if(i == cnt)
  {
    fprintf(stderr, "no device found\n");
    exit(-1);
  }

  printf("device found\n");

  const struct libusb_pollfd** pfd_usb = libusb_get_pollfds(ctx);

  struct pollfd pfd[MAX_FD] = {};

  for (i=0; pfd_usb[i] != NULL; i++)
  {
    pfd[i].fd = pfd_usb[i]->fd;
    pfd[i].events = pfd_usb[i]->events;
  }

  int nbfd = i;

  send_next_transfer(devh);

  while(!done)
  {
    if(poll(pfd, nbfd, -1))
    {
      for(i=0; i<nbfd; ++i)
      {
        if (pfd[i].revents & (POLLERR | POLLHUP))
        {
          done = 1;
        }
        if(pfd[i].revents & (POLLOUT | POLLIN))
        {
          libusb_handle_events(ctx);
        }
      }
    }
  }

  libusb_release_interface(devh, 0);

  libusb_close(devh);

  libusb_exit(ctx);

  return 0;
}
