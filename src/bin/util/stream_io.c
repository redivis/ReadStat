/* Redivis: IO handlers for reading a parser's input from standard input (e.g. a pipe), which can't seek. Reads fill
 * the whole buffer unless the input ends, since a pipe can return less than was asked for. Seeking forward reads and
 * discards; seeking back, or to the end, fails, which tells parsers that support it (currently only SAS7BDAT) to parse
 * in a single forward pass. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined(_MSC_VER)
#   include <unistd.h>
#else
#   include <io.h>
#   define read _read
#endif

#include "../../readstat.h"
#include "stream_io.h"

typedef struct stream_io_ctx_s {
    int             fd;
    readstat_off_t  pos;
} stream_io_ctx_t;

static int stream_open_handler(const char *path, void *io_ctx) {
    stream_io_ctx_t *ctx = (stream_io_ctx_t *)io_ctx;
    ctx->fd = 0;
    ctx->pos = 0;
    return ctx->fd;
}

static int stream_close_handler(void *io_ctx) {
    return 0;
}

static ssize_t stream_read_handler(void *buf, size_t nbyte, void *io_ctx) {
    stream_io_ctx_t *ctx = (stream_io_ctx_t *)io_ctx;
    size_t total = 0;
    while (total < nbyte) {
        ssize_t bytes_read = read(ctx->fd, (char *)buf + total, nbyte - total);
        if (bytes_read == 0)
            break;
        if (bytes_read < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        total += bytes_read;
    }
    ctx->pos += total;
    return total;
}

static readstat_off_t stream_seek_handler(readstat_off_t offset, readstat_io_flags_t whence, void *io_ctx) {
    stream_io_ctx_t *ctx = (stream_io_ctx_t *)io_ctx;
    readstat_off_t target;
    if (whence == READSTAT_SEEK_SET) {
        target = offset;
    } else if (whence == READSTAT_SEEK_CUR) {
        target = ctx->pos + offset;
    } else {
        return -1;
    }
    if (target < ctx->pos)
        return -1;

    char buf[4096];
    while (ctx->pos < target) {
        size_t len = target - ctx->pos < (readstat_off_t)sizeof(buf) ? target - ctx->pos : sizeof(buf);
        if (stream_read_handler(buf, len, io_ctx) < (ssize_t)len)
            return -1;
    }
    return ctx->pos;
}

static readstat_error_t stream_update_handler(long file_size, readstat_progress_handler progress_handler,
        void *user_ctx, void *io_ctx) {
    /* The input's size isn't known, so there's no progress to report */
    return READSTAT_OK;
}

readstat_error_t stream_io_init(readstat_parser_t *parser) {
    readstat_error_t retval = READSTAT_OK;
    stream_io_ctx_t *io_ctx = calloc(1, sizeof(stream_io_ctx_t));

    if (io_ctx == NULL)
        return READSTAT_ERROR_MALLOC;

    if ((retval = readstat_set_open_handler(parser, stream_open_handler)) != READSTAT_OK)
        goto cleanup;
    if ((retval = readstat_set_close_handler(parser, stream_close_handler)) != READSTAT_OK)
        goto cleanup;
    if ((retval = readstat_set_seek_handler(parser, stream_seek_handler)) != READSTAT_OK)
        goto cleanup;
    if ((retval = readstat_set_read_handler(parser, stream_read_handler)) != READSTAT_OK)
        goto cleanup;
    if ((retval = readstat_set_update_handler(parser, stream_update_handler)) != READSTAT_OK)
        goto cleanup;
    if ((retval = readstat_set_io_ctx(parser, io_ctx)) != READSTAT_OK)
        goto cleanup;

    /* As with the unistd handlers, the parser frees its io_ctx when it's freed */
    parser->io->io_ctx_needs_free = 1;
    io_ctx = NULL;

cleanup:
    free(io_ctx);
    return retval;
}
