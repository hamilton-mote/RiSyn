/* LINKED BUFFER */

#include "lbuf.h"
#include <string.h>

void lbuf_init(struct lbufhead* buffer) {
    memset(buffer, 0x00, sizeof(struct lbufhead));
}

struct ip_iovec* lbuf_to_iovec(struct lbufhead* buffer) {
    if (buffer == NULL || buffer->head == NULL) {
        return NULL;
    } else {
        return &buffer->head->iov;
    }
}

int lbuf_append(struct lbufhead* buffer, struct lbufent* newentry) {
    struct lbufent* tail = buffer->tail;
    if (tail == NULL) {
        buffer->head = newentry;
        buffer->tail = newentry;
        buffer->length = (uint32_t) newentry->iov.iov_len;
        newentry->iov.iov_next = NULL;
    } else if (newentry->iov.iov_len <= (uint32_t) tail->extraspace) {
        memcpy(tail->iov.iov_base + tail->iov.iov_len,
               newentry->iov.iov_base, newentry->iov.iov_len);
        tail->extraspace -= newentry->iov.iov_len;
        buffer->length += (uint32_t) newentry->iov.iov_len;
        tail->iov.iov_len += newentry->iov.iov_len;
        return 2;
    } else {
        tail->iov.iov_next = &newentry->iov;
        buffer->tail = newentry;
        buffer->length += (uint32_t) newentry->iov.iov_len;
        newentry->iov.iov_next = NULL;
    }
    return 1;
}

uint32_t lbuf_pop(struct lbufhead* buffer, uint32_t numbytes, int* ntraversed) {
    struct lbufent* curr = buffer->head;
    uint32_t bytesleft = numbytes;
    while (bytesleft >= curr->iov.iov_len) {
        ++*ntraversed;
        buffer->head = IOV_TO_LBUFENT(curr->iov.iov_next);
        bytesleft -= curr->iov.iov_len;
        buffer->length -= curr->iov.iov_len;
        if (buffer->tail == curr) {
            /* buffer->head should be NULL. */
            buffer->tail = NULL;
            return numbytes - bytesleft;
        }
        curr = buffer->head;
    }
    /* Handle the last entry. */
    curr->iov.iov_base += bytesleft;
    curr->iov.iov_len -= bytesleft;
    buffer->length -= bytesleft;
    return numbytes;
}

int lbuf_getrange(struct lbufhead* buffer, uint32_t offset, uint32_t numbytes,
                  struct lbufent** first, uint32_t* firstoffset,
                  struct lbufent** last, uint32_t* lastextra) {
    struct lbufent* curr = buffer->head;
    uint32_t offsetleft = offset;
    uint32_t bytesleft = numbytes;
    if (buffer->length < offset + numbytes) {
        return 1; // out of range
    }
    while (offsetleft > 0 && offsetleft >= curr->iov.iov_len) {
        offsetleft -= curr->iov.iov_len;
        curr = IOV_TO_LBUFENT(curr->iov.iov_next);
    }
    *first = curr;
    *firstoffset = offsetleft;
    bytesleft += offsetleft;
    while (bytesleft > 0 && bytesleft > curr->iov.iov_len) {
        bytesleft -= curr->iov.iov_len;
        curr = IOV_TO_LBUFENT(curr->iov.iov_next);
    }
    *last = curr;
    *lastextra = curr->iov.iov_len - bytesleft;
    return 0;
}

uint32_t lbuf_used_space(struct lbufhead* buffer) {
    return buffer->length;
}
