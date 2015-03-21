#ifndef __VT_PARSER_H__
#define __VT_PARSER_H__

typedef struct vt_parser_s *vt_parser_t;
typedef struct vt_parser_s {
    int escaped;
    int mode;
    
    /* buffer */
    unsigned char *buffer;
    unsigned       buf_alloc, buf_len;
    unsigned char  fb_buffer[1]; /* fallback */
    
    /* interface */
    void   *data;
    void *(*realloc)(void *, unsigned); /* can be null */
    void  (*putc)(void *, unsigned char);
    void  (*csi)(void *, unsigned char *);
    void  (*osc)(void *, unsigned char *);
    void  (*dcs_enter)(void *, unsigned char *);
    void  (*dcs_putc)(void *, unsigned char);
    void  (*dcs_exit)(void *);
} vt_parser_s;

void vt_parser_init(vt_parser_t parser);
void vt_parser_destroy(vt_parser_t parser);
void vt_parser_input(vt_parser_t parser, unsigned char *buf, unsigned len);

#endif
