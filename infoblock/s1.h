#ifndef __IB_S1_H
#define __IB_S1_H

#define INFO_BLOCK_S1_VER_MAJOR 's'
#define INFO_BLOCK_S1_VER_MINOR '1'
#define INFO_BLOCK_S1_VERSION L8_TO_LT16(INFO_BLOCK_S1_VER_MAJOR, INFO_BLOCK_S1_VER_MINOR)

int create_information_block_s1(information_container_t *info, int fw_crc24, int fw_size, int fw_version);
int set_information_block_ptr_s1(information_container_t *info, char *data, int len, int flag);
int infomation_block_size_s1(void);
void destory_information_block_s1(void *info_ptr);
#endif