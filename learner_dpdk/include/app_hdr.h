#ifndef _APP_HDR_H_
#define _APP_HDR_H_

#define KEY_LEN 16
#define VALUE_LEN 64
#define READ_OP 0
#define WRITE_OP 1

#ifdef __cplusplus
extern "C" {
#endif

struct app_hdr {
    uint8_t msg_type;
    uint32_t key_len;
	uint8_t key[KEY_LEN];
    uint32_t value_len;
	uint8_t value[VALUE_LEN];
} __attribute__((__packed__));

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif
