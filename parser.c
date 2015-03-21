#include "parser.h"
#include <stdlib.h>

#define PARSER_GROUND          0
#define PARSER_DCS             1
#define PARSER_DCS_INTERM      2
#define PARSER_DCS_PARAM       3
#define PARSER_DCS_PASSTHROUGH 4
#define PARSER_DCS_IGNORE      5
#define PARSER_OSC             6
#define PARSER_CSI             7
#define PARSER_CSI_INTERM      8
#define PARSER_CSI_PARAM       9
#define PARSER_CSI_IGNORE      10

struct mode_handler_s {
    void(*input[256])(vt_parser_t, unsigned char);
    void(*escaped_input)(vt_parser_t, unsigned char);
    void(*enter)(vt_parser_t);
    void(*exit)(vt_parser_t);
};

static struct mode_handler_s mode_table[];

static void
_mode_set(vt_parser_t parser, unsigned mode) {
    if (mode_table[parser->mode].exit)
        mode_table[parser->mode].exit(parser);
    if (mode_table[mode].enter)
        mode_table[mode].enter(parser);
    parser->mode = mode;
}

void
vt_parser_init(vt_parser_t parser) {
    parser->mode = PARSER_GROUND;
    parser->escaped = 0;        
    parser->buffer = parser->fb_buffer;
    parser->buf_len = 0;
    parser->buf_alloc = 1;
}

void
vt_parser_destroy(vt_parser_t parser) {
    if (parser->buffer != parser->fb_buffer) {
        if (parser->realloc) 
            parser->realloc(parser->buffer, 0);
        else
            realloc(parser->buffer, 0);
    }
}

void
vt_parser_input(vt_parser_t parser, unsigned char *buffer, unsigned size) {
    for (unsigned i = 0; i < size; ++ i) {
        int ch = buffer[i];
        if (ch == 0x1b) {
            parser->escaped = 1;
        } else if (parser->escaped) {
            parser->escaped = 0;
            switch (ch) {
            case 0x50:
                _mode_set(parser, PARSER_DCS);
                break;
            case 0x5b:
                _mode_set(parser, PARSER_CSI);
                break;
            case 0x5c:
                _mode_set(parser, PARSER_GROUND);
                break;
            case 0x5d:
                _mode_set(parser, PARSER_OSC);
                break;
            default:
                _mode_set(parser, PARSER_GROUND);
                if (mode_table[parser->mode].escaped_input)
                    mode_table[parser->mode].escaped_input(parser, ch);
            }
        } else {
            switch (ch) {
            case 0x18:
            case 0x1a:
                _mode_set(parser, PARSER_GROUND);
                break;
            }

            void(*input)(vt_parser_t, unsigned char) =
                mode_table[parser->mode].input[ch];
            if (input)
                input(parser, ch);
        }
    }
}

static void
_buffer_putc(vt_parser_t parser, unsigned char ch) {
    while (parser->buf_len + 1 >= parser->buf_alloc) {
        unsigned n_alloc = (parser->buf_alloc << 1) + 1;
        unsigned char *n_buf;
        if (parser->realloc) 
            n_buf = parser->realloc(parser->buffer == parser->fb_buffer ?
                                    NULL : parser->buffer,
                                    n_alloc);
        else
            n_buf = realloc(parser->buffer == parser->fb_buffer ?
                            NULL : parser->buffer,
                            n_alloc);
            
        if (n_buf == NULL) {
            /* this should not happen. When alloc failed, set the
             * buffer to fallback */
            parser->buffer = parser->fb_buffer;
            parser->buf_len = 0;
            parser->buf_alloc = 1;
            return;
        }
        parser->buffer = n_buf;
        parser->buf_alloc = n_alloc;
    }

    parser->buffer[parser->buf_len ++] = ch;
}

static unsigned char *
_buffer_export(vt_parser_t parser) {
    parser->buffer[parser->buf_len] = 0;
    return parser->buffer;
}

static void
_buffer_clear(vt_parser_t parser) {
    parser->buf_len = 0;
}

/* abstract transition functions */

static void _ground_input(vt_parser_t parser, unsigned char ch) {
    parser->putc(parser->data, ch);
}

#define _ground {                               \
        .input = {                              \
            [ 0x00 ... 0xff ] = _ground_input,  \
        },                                      \
        .enter = _buffer_clear,                 \
    }
    
/* == DCS ================================= */

static void _dcs_input_0(vt_parser_t parser, unsigned char ch) {
    /* 0x20 ... 0x2f */
    _buffer_putc(parser, ch);
    _mode_set(parser, PARSER_DCS_INTERM);
}

static void _dcs_input_1(vt_parser_t parser, unsigned char ch) {
    /* 0x3a */
    _mode_set(parser, PARSER_DCS_IGNORE);
}

static void _dcs_input_2(vt_parser_t parser, unsigned char ch) {
    /* 0x30 ... 0x39, 0x3b ... 0x3f */
    _buffer_putc(parser, ch);
    _mode_set(parser, PARSER_DCS_PARAM);
}

static void _dcs_input_3(vt_parser_t parser, unsigned char ch) {
    /* 0x40 ... 0x7e */
    _buffer_putc(parser, ch);
    _mode_set(parser, PARSER_DCS_PASSTHROUGH);
}

#define _dcs {                                  \
        .input = {                              \
            [ 0x20 ... 0x2f ] = _dcs_input_0,   \
            [ 0x30 ... 0x39 ] = _dcs_input_2,   \
            [ 0x3a ]          = _dcs_input_1,   \
            [ 0x3b ... 0x3f ] = _dcs_input_2,   \
            [ 0x40 ... 0x7e ] = _dcs_input_3,   \
        },                                      \
    }

static void _dcs_interm_input_0(vt_parser_t parser, unsigned char ch) {
    /* 0x20 ... 0x2f */
    _buffer_putc(parser, ch);
}

static void _dcs_interm_input_1(vt_parser_t parser, unsigned char ch) {
    /* 0x30 ... 0x3f */
    _mode_set(parser, PARSER_DCS_IGNORE);
}

static void _dcs_interm_input_2(vt_parser_t parser, unsigned char ch) {
    /* 0x40 ... 0x7e */
    _buffer_putc(parser, ch);
    _mode_set(parser, PARSER_DCS_PASSTHROUGH);
}

#define _dcs_interm {                                   \
        .input = {                                      \
            [ 0x20 ... 0x2f ] = _dcs_interm_input_0,    \
            [ 0x30 ... 0x3f ] = _dcs_interm_input_1,    \
            [ 0x40 ... 0x7e ] = _dcs_interm_input_2,    \
        }                                               \
    }

#define _dcs_ignore { 0 }

static void _dcs_param_input_0(vt_parser_t parser, unsigned char ch) {
    /* 0x20 ... 0x2f */
    _buffer_putc(parser, ch);
    _mode_set(parser, PARSER_DCS_INTERM);
}

static void _dcs_param_input_1(vt_parser_t parser, unsigned char ch) {
    /* 0x30 ... 0x39, 0x3b */
    _buffer_putc(parser, ch);
}

static void _dcs_param_input_2(vt_parser_t parser, unsigned char ch) {
    /* 0x3a, 0x3c ... 0x3f */
    _mode_set(parser, PARSER_DCS_IGNORE);
}

static void _dcs_param_input_3(vt_parser_t parser, unsigned char ch) {
    /* 0x40 ... 0x7e */
    _buffer_putc(parser, ch);
    _mode_set(parser, PARSER_DCS_PASSTHROUGH);
}

#define _dcs_param {                                    \
        .input = {                                      \
            [ 0x20 ... 0x2f ] = _dcs_param_input_0,     \
            [ 0x30 ... 0x39 ] = _dcs_param_input_1,     \
            [ 0x3a ]          = _dcs_param_input_2,     \
            [ 0x3b ]          = _dcs_param_input_1,     \
            [ 0x3c ... 0x3f ] = _dcs_param_input_2,     \
            [ 0x40 ... 0x7e ] = _dcs_param_input_3,     \
        }                                               \
    }

static void _dcs_passthrough_enter(vt_parser_t parser) {
    unsigned char *buf = _buffer_export(parser);
    parser->dcs_enter(parser->data, buf);
    _buffer_clear(parser);
}

static void _dcs_passthrough_input(vt_parser_t parser, unsigned char ch) {
    /* 0x00 ... 0x7e, 0x80 ... 0xff */
    parser->dcs_putc(parser->data, ch);
}

static void _dcs_passthrough_exit(vt_parser_t parser) {
    parser->dcs_exit(parser->data);
}

#define _dcs_passthrough {                              \
        .input = {                                      \
            [ 0x00 ... 0x7e ] = _dcs_passthrough_input, \
            [ 0x80 ... 0xff ] = _dcs_passthrough_input, \
        },                                              \
        .enter = _dcs_passthrough_enter,                \
        .exit  = _dcs_passthrough_exit,                 \
    }

/* == OSC ================================= */

static void _osc_input(vt_parser_t parser, unsigned char ch) {
    /* 0x20 ... 0xff */
    _buffer_putc(parser, ch);
}

static void _osc_exit(vt_parser_t parser) {
    unsigned char *buf = _buffer_export(parser);
    parser->osc(parser->data, buf);
    _buffer_clear(parser);
}

#define _osc {                                  \
        .input = {                              \
            [ 0x20 ... 0xff ] = _osc_input,     \
        },                                      \
        .exit = _osc_exit,                      \
    }

/* == CSI ================================= */

static void _csi_input_0(vt_parser_t parser, unsigned char ch) {
    /* 0x00 ... 0x1f */
    parser->putc(parser->data, ch);
}

static void _csi_input_1(vt_parser_t parser, unsigned char ch) {
    /* 0x20 ... 0x2f */
    _buffer_putc(parser, ch);
    _mode_set(parser, PARSER_CSI_INTERM);
}

static void _csi_input_2(vt_parser_t parser, unsigned char ch) {
    /* 0x3a */
    _mode_set(parser, PARSER_CSI_IGNORE);
}

static void _csi_input_3(vt_parser_t parser, unsigned char ch) {
    /* 0x30 ... 0x39, 0x3b ... 0x3f */
    _buffer_putc(parser, ch);
    _mode_set(parser, PARSER_CSI_PARAM);
}

static void _csi_input_4(vt_parser_t parser, unsigned char ch) {
    /* 0x40 ... 0x7e */
    unsigned char buf[2] = { ch, 0 };
    parser->csi(parser->data, buf);
    _mode_set(parser, PARSER_GROUND);
}

#define _csi {                                  \
        .input = {                              \
            [ 0x00 ... 0x1f ] = _csi_input_0,   \
            [ 0x20 ... 0x2f ] = _csi_input_1,   \
            [ 0x30 ... 0x39 ] = _csi_input_3,   \
            [ 0x3a ]          = _csi_input_2,   \
            [ 0x3b ... 0x3f ] = _csi_input_3,   \
            [ 0x40 ... 0x7e ] = _csi_input_4,   \
        },                                      \
    }
    
static void _csi_interm_input_0(vt_parser_t parser, unsigned char ch) {
    /* 0x00 ... 0x1f */
    parser->putc(parser->data, ch);
}

static void _csi_interm_input_1(vt_parser_t parser, unsigned char ch) {
    /* 0x20 ... 0x2f */
    _buffer_putc(parser, ch);
}

static void _csi_interm_input_2(vt_parser_t parser, unsigned char ch) {
    /* 0x30 ... 0x3f */
    _mode_set(parser, PARSER_CSI_IGNORE);
}

static void _csi_interm_input_3(vt_parser_t parser, unsigned char ch) {
    /* 0x40 ... 0x7e */
    _buffer_putc(parser, ch);
    unsigned char *buf = _buffer_export(parser);
    parser->csi(parser->data, buf);
    _buffer_clear(parser);
    _mode_set(parser, PARSER_GROUND);
}

#define _csi_interm {                                   \
        .input = {                                      \
            [ 0x00 ... 0x1f ] = _csi_interm_input_0,    \
            [ 0x20 ... 0x2f ] = _csi_interm_input_1,    \
            [ 0x30 ... 0x3f ] = _csi_interm_input_2,    \
            [ 0x40 ... 0x7e ] = _csi_interm_input_3,    \
        },                                              \
    }

static void _csi_param_input_0(vt_parser_t parser, unsigned char ch) {
    /* 0x00 ... 0x1f */
    parser->putc(parser->data, ch);
}

static void _csi_param_input_1(vt_parser_t parser, unsigned char ch) {
    /* 0x20 ... 0x2f */
    _buffer_putc(parser, ch);
    _mode_set(parser, PARSER_CSI_INTERM);
}

static void _csi_param_input_2(vt_parser_t parser, unsigned char ch) {
    /* 0x30 ... 0x39, 0x3b */
    _buffer_putc(parser, ch);
}

static void _csi_param_input_3(vt_parser_t parser, unsigned char ch) {
    /* 0x3a, 0x3c ... 0x3f */
    _mode_set(parser, PARSER_CSI_IGNORE);
}

static void _csi_param_input_4(vt_parser_t parser, unsigned char ch) {
    /* 0x40 ... 0x7e */
    unsigned char *buf = _buffer_export(parser);
    parser->csi(parser->data, buf);
    _buffer_clear(parser);
    _mode_set(parser, PARSER_GROUND);
}

#define _csi_param {                                    \
        .input = {                                      \
            [ 0x00 ... 0x1f ] = _csi_param_input_0,     \
            [ 0x20 ... 0x2f ] = _csi_param_input_1,     \
            [ 0x30 ... 0x39 ] = _csi_param_input_2,     \
            [ 0x3a ]          = _csi_param_input_3,     \
            [ 0x3b ]          = _csi_param_input_2,     \
            [ 0x3c ... 0x3f ] = _csi_param_input_3,     \
            [ 0x40 ... 0x7e ] = _csi_param_input_4,     \
        },                                              \
    }

static void _csi_ignore_input_0(vt_parser_t parser, unsigned char ch) {
    /* 0x00 ... 0x1f */
    parser->putc(parser->data, ch);
}

static void _csi_ignore_input_1(vt_parser_t parser, unsigned char ch) {
    /* 0x40 ... 0x7e, 0x80 ... 0xff */
    _mode_set(parser, PARSER_GROUND);
}

#define _csi_ignore {                                   \
        .input = {                                      \
            [ 0x00 ... 0x1f ] = _csi_param_input_0,     \
            [ 0x40 ... 0x7e ] = _csi_param_input_1,     \
            [ 0x80 ... 0xff ] = _csi_param_input_1,     \
        },                                              \
    }

/* define modes */

static struct mode_handler_s mode_table[] = {
    [ PARSER_GROUND ]          = _ground,
    [ PARSER_DCS ]             = _dcs,
    [ PARSER_DCS_INTERM ]      = _dcs_interm,
    [ PARSER_DCS_PARAM ]       = _dcs_param,
    [ PARSER_DCS_PASSTHROUGH ] = _dcs_passthrough,
    [ PARSER_DCS_IGNORE ]      = _dcs_ignore,
    [ PARSER_OSC ]             = _osc,
    [ PARSER_CSI ]             = _csi,
    [ PARSER_CSI_INTERM ]      = _csi_interm,
    [ PARSER_CSI_PARAM ]       = _csi_param,
    [ PARSER_CSI_IGNORE ]      = _csi_ignore,
};
