#ifndef CBUF_H_
#define CBUF_H_

/* CIRCULAR BUFFER
   The circular buffer can be treated either as a buffer of bytes, or a buffer
   of TCP segments. Don't mix and match the functions unless you know what
   you're doing! */

   #include <stdint.h>
   #include <stdlib.h>

struct cbufhead {
    size_t r_index;
    size_t w_index;
    size_t size;
    uint8_t* buf;
};

void cbuf_init(struct cbufhead* chdr, uint8_t* buf, size_t len);

size_t cbuf_write(struct cbufhead* chdr, uint8_t* data, size_t data_len);
size_t cbuf_read(struct cbufhead* chdr, uint8_t* data, size_t numbytes, int pop);
size_t cbuf_read_offset(struct cbufhead* chdr, uint8_t* data, size_t numbytes, size_t offset);
size_t cbuf_pop(struct cbufhead* chdr, size_t numbytes);
size_t cbuf_used_space(struct cbufhead* chdr);
size_t cbuf_free_space(struct cbufhead* chdr);
size_t cbuf_size(struct cbufhead* chdr);

size_t cbuf_reass_write(struct cbufhead* chdr, size_t offset, uint8_t* data, size_t numbytes, uint8_t* bitmap, size_t* firstindex);
size_t cbuf_reass_merge(struct cbufhead* chdr, size_t numbytes, uint8_t* bitmap);
size_t cbuf_reass_count_set(struct cbufhead* chdr, size_t offset, uint8_t* bitmap, size_t limit);
int cbuf_reass_within_offset(struct cbufhead* chdr, size_t offset, size_t index);

/*
int cbuf_write_segment(struct cbufhead* chdr, uint8_t* segment, size_t seglen);
size_t cbuf_pop_segment(struct cbufhead* chdr, size_t segsize);
size_t cbuf_peek_segment_size(struct cbufhead* chdr);
size_t cbuf_peek_segment(struct cbufhead* chdr, uint8_t* data, size_t numbytes);
*/

#endif
