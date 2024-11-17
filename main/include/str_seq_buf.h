#ifndef _STR_SEQ_BUF_H_
#define _STR_SEQ_BUF_H_

#include "os.h"

#include <stdint.h>

typedef struct {
	char* buf;
	uint32_t head;
	uint32_t end;
	uint32_t size;
	SemaphoreHandle_t mux;
} StrSeqBuf_t;

typedef void (*StrSeqBufAction_t)(const char* str);

#define DEFINE_STR_SEQ_BUF(name, size) \
	char name##_buf[size] = {0};       \
	StrSeqBuf_t name = {name##_buf, 0, 0, size, NULL}

void strbuf_init(StrSeqBuf_t* buf);
uint32_t strbug_get_unread_slots(StrSeqBuf_t* buf);
uint32_t strbuf_get_available_slots(StrSeqBuf_t* buf);
bool strbuf_is_empty(StrSeqBuf_t* buf);

void strbuf_write_nolock(StrSeqBuf_t* buf, const char* wr_buf, const size_t len);
void strbuf_write(StrSeqBuf_t* buf, const char* wr_buf, const size_t len);

void strbuf_read_once_nolock(StrSeqBuf_t* buf, char* out_buf, const size_t max_len);
void strbuf_read_all_with_action(StrSeqBuf_t* buf, StrSeqBufAction_t action, const size_t max_len);

#endif // _STR_SEQ_BUF_H_