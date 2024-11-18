#include "str_seq_buf.h"

#include <string.h>

/* Private */
static inline void strbuf_init(StrSeqBuf_t* buf) { buf->mux = xSemaphoreCreateMutex(); }
static inline void strbuf_check_init(StrSeqBuf_t* buf) {
	if (buf->mux == NULL) {
		strbuf_init(buf);
	}
}
static inline void strbuf_shift_head(StrSeqBuf_t* buf) { buf->head = (buf->head + 1) % buf->size; }

/* Public */
inline uint32_t strbug_get_unread_slots(StrSeqBuf_t* buf) { return (buf->end + buf->size - buf->head) % buf->size; }
inline uint32_t strbuf_get_available_slots(StrSeqBuf_t* buf) { return buf->size - strbug_get_unread_slots(buf); }
inline bool strbuf_is_empty(StrSeqBuf_t* buf) { return buf->head == buf->end; }

void strbuf_write_nolock(StrSeqBuf_t* buf, const char* wr_buf, const size_t len) {
	if (len > strbuf_get_available_slots(buf)) {
		return; // buffer full
	}
	if (buf->end + len > buf->size) {
		const size_t len1 = buf->size - buf->end;
		memcpy(&buf->buf[buf->end], wr_buf, len1);
		memcpy(&buf->buf[0], &wr_buf[len1], len - len1);
		buf->end = len - len1;
	} else {
		memcpy(&buf->buf[buf->end], wr_buf, len);
		buf->end += len;
	}
}

void strbuf_write(StrSeqBuf_t* buf, const char* wr_buf, const size_t len) {
	strbuf_check_init(buf);
	xSemaphoreTake(buf->mux, portMAX_DELAY);
	xQueueSemaphoreTake((buf->mux), ((TickType_t)0xffffffffUL));
	strbuf_write_nolock(buf, wr_buf, len);
	xSemaphoreGive(buf->mux);
}

void strbuf_read_once_nolock(StrSeqBuf_t* buf, char* out_buf, const size_t max_len) {
	int i = 0;
	while (buf->buf[buf->head] != '\0' && i < max_len - 1) { // `i < max_len - 1` to leave space for \0
		out_buf[i++] = buf->buf[buf->head];
		strbuf_shift_head(buf);
	}
	out_buf[i] = '\0';
	strbuf_shift_head(buf);
}

void strbuf_read_all_with_action(StrSeqBuf_t* buf, StrSeqBufAction_t action, const size_t max_len) {
	if (action == NULL || strbuf_is_empty(buf)) {
		return;
	}
	strbuf_check_init(buf);
	xSemaphoreTake(buf->mux, portMAX_DELAY);
	while (!strbuf_is_empty(buf)) {
		char tmp_buf[max_len];
		strbuf_read_once_nolock(buf, tmp_buf, max_len);
		action(tmp_buf);
	}
	xSemaphoreGive(buf->mux);
}