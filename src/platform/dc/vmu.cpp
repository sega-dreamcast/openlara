#include <stdio.h>
#include <string.h>
#include <time.h>
#include "vmu.h"
#include <ronin/ronin.h>

vmuresult vmu_errno;

static void setlcd(struct vmsinfo *info, void *bit)
{
  unsigned int param[50];
  
  param[0] = MAPLE_FUNC_LCD<<24;
  param[1] = 0;
  memcpy(param+2, bit, 48*4);
  maple_docmd(info->port, info->dev, MAPLE_COMMAND_BWRITE, 50, param);
}

static void clearlcd(struct vmsinfo *info)
{
  unsigned int bit[50];
  
  memset(bit, 0, sizeof(bit));
  setlcd(info, bit);
}

void conv_lcd_icon(unsigned char *bit, const unsigned char *in)
{
  int i,x,j;
  unsigned int *src = (unsigned int *)in;
  unsigned char *dst = bit + (48/8) * 32;

  for (i=0; i<32; i++) {
    unsigned char v;
    unsigned int b = *src++;
    v = 0;
    *--dst = 0xff;
    for (j= 0; j<4; j++) {
      for (x=0; x<8; x++) {
	      v <<= 1;
	      v |= (b & 1)?1:0;
	      b >>= 1;
      }
      *--dst = v;
    }
    *--dst = 0xff;
  }
}

void conv_icon(unsigned char *bit, const unsigned char *in)
{
  int i,x,j;
  const unsigned char *src = in;
  unsigned char *dst = ((unsigned char *)bit) + 32;
  unsigned short *pal = (unsigned short *)bit;

  pal[0] = 0xf000;
  pal[15] = 0xffff;

  for (i=0; i<32; i++) {
    unsigned char v;
    unsigned char b;
    for (j= 0; j<4; j++) {
      b = *src++;
      for (x=0; x<4; x++) {
	      v = (b & 0x80)?0x00:0xf0;
	      v |= (b & 0x40)?0x00:0x0f;
	      b <<= 2;
	      dst[x] = v;
      }
      dst += 4;
    }
  }
}

bool save_to_vmu(int unit, const char *filename, const char *buf, int buf_len, unsigned char *icon, unsigned char *lcd)
{
  struct vms_file_header header;
  struct vmsinfo info;
  struct superblock super;
  struct vms_file file;
  char new_filename[16];
  unsigned int free_cnt;
  time_t long_time;
  struct tm *now_time;
  struct timestamp stamp;
  int size = buf_len;

  if (!vmsfs_check_unit(unit, 0, &info)) {
    vmu_errno = VMU_NO;
    return false;
  }
  if (!vmsfs_get_superblock(&info, &super)) {
    vmu_errno = VMU_NORES;
    return false;
  }
  free_cnt = vmsfs_count_free(&super);

  strncpy(new_filename, filename, sizeof(new_filename));
  if (vmsfs_open_file(&super, new_filename, &file))
    free_cnt += file.blks;
  
  if (((128+512+size+511)/512) > free_cnt) {
    vmu_errno = VMU_NOSPACE;
    return false;
  }
  
  memset(&header, 0, sizeof(header));
  strncpy(header.shortdesc, "Save Data",sizeof(header.shortdesc));
  strncpy(header.longdesc, "OpenLara", sizeof(header.longdesc));
  strncpy(header.id, "OpenLara", sizeof(header.id));
  header.numicons = 1;
  memcpy(header.palette, icon, sizeof(header.palette));

  time(&long_time);
  now_time = localtime(&long_time);
  if (now_time != NULL) {
    stamp.year = now_time->tm_year + 1900;
    stamp.month = now_time->tm_mon + 1;
    stamp.wkday = now_time->tm_wday;
    stamp.day = now_time->tm_mday;
    stamp.hour = now_time->tm_hour;
    stamp.minute = now_time->tm_min;
    stamp.second = now_time->tm_sec;
  }

  clearlcd(&info);
  vmsfs_beep(&info, 1);

  if (!vmsfs_create_file(&super, new_filename, &header, icon+sizeof(header.palette), NULL, buf, size, &stamp)) {
#ifndef NOSERIAL
    fprintf(stderr,"%s",vmsfs_describe_error());
#endif
    vmsfs_beep(&info, 0);
    vmu_errno = VMU_WRITEFAILE;
    return false;
  }
  vmsfs_beep(&info, 0);

  setlcd(&info, lcd);

  vmu_errno = VMU_OK;
  
  return true;
}

bool load_from_vmu(int unit, const char *filename, char *buf, int *buf_len, unsigned char *lcd)
{
  struct vmsinfo info;
  struct superblock super;
  struct vms_file file;
  
  if (!vmsfs_check_unit(unit, 0, &info)) {
    vmu_errno = VMU_NO;
    return false;
  }
  if (!vmsfs_get_superblock(&info, &super)) {
    vmu_errno = VMU_NORES;
    return false;
  }
  if (!vmsfs_open_file(&super, filename, &file)) {
    vmu_errno = VMU_NOFILE;
    return false;
  }

  clearlcd(&info);

  int size = file.size;
  
  if (!vmsfs_read_file(&file, (unsigned char *)buf, size)) {
    vmu_errno = VMU_READFAILE;
    return false;
  }

  *buf_len = size;

  setlcd(&info, lcd);

  vmu_errno = VMU_OK;

  return true;
}

