#ifndef __VT_H__
#define __VT_H__

typedef struct vt_buffer_s *vt_buffer_t;
typedef struct vt_buffer_s {
    unsigned  n_rows;
    vt_row_t *rows;
} vt_buffer_s;

typedef struct vt_s *vt_t;
typedef struct vt_s {
    vt_parser_s parser;
    vt_buffer_s buffers[2];
    
    vt_buffer_t buffer;
} vt_s;

#endif
