/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

// Set the DEBUG macro if needed
#ifndef DEBUG
#   if !defined(NDEBUG)
#       define DEBUG 1
#   else
#       define DEBUG 0
#   endif
#endif

#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#  define GLES2
#  include <OPenGLES/ES2/gl.h>
#  include <OpenGLES/ES2/glext.h>
#else
#  ifdef GLES2
#    include <GLES2/gl2.h>
#    include <GLES2/gl2ext.h>
#  else
#    include <GL/gl.h>
#  endif
#endif

#if DEBUG
#  define GL(line) do {                                 \
       line;                                            \
       if (gl_check_errors(__FILE__, __LINE__))         \
           exit(-1);                                    \
   } while(0)
#else
#  define GL(line) line
#endif

int gl_check_errors(const char *file, int line);
int gl_create_program(const char *vertex_shader_code,
                      const char *fragment_shader_code, const char *include,
                      const char **attr_names);

/*
 * Struct: gl_buf_info
 * Describe an OpenGL attribute.
 */
typedef struct gl_buf_info
{
    int size;
    struct {
        int         type;
        int         size;
        int         normalized;
        int         ofs;
    } attrs[16];

} gl_buf_info_t;

/*
 * Struct: gl_buf_t
 * Helper structure to store an attribute buffer data.
 *
 * Still experimental.
 *
 * A gl_buf_t instance is basically just a memory buffer with meta info about
 * the structure of the data it contains.
 *
 * The helper functions can be used to fill the buffer data without having
 * to use an explicit C struct for it.
 */
typedef struct gl_buf
{
    uint8_t *data;
    const gl_buf_info_t *info;
    int capacity;   // Number of items we can store.
    int nb;         // Current number of items.
} gl_buf_t;

/*
 * Function: gl_buf_alloc
 * Allocate buffer data.
 */
void gl_buf_alloc(gl_buf_t *buf, const gl_buf_info_t *info, int capacity);

/*
 * Function: gl_buf_release
 * Release the memory used by a buffer.
 */
void gl_buf_release(gl_buf_t *buf);

/*
 * Function: gl_buf[234][ri]
 * Set buffer data at a given index.
 *
 * If <idx> is -1, this set the current value of the buffer.
 */

void gl_buf_1f(gl_buf_t *buf, int idx, int attr, float v0);
void gl_buf_2f(gl_buf_t *buf, int idx, int attr, float v0, float v1);
void gl_buf_3f(gl_buf_t *buf, int idx, int attr, float v0, float v1, float v2);
void gl_buf_4f(gl_buf_t *buf, int idx, int attr,
               float v0, float v1, float v2, float v3);
void gl_buf_1i(gl_buf_t *buf, int idx, int attr, int v0);
void gl_buf_4i(gl_buf_t *buf, int idx, int attr,
               int v0, int v1, int v2, int v3);

/*
 * Function: gl_buf_at
 * Return a pointer to an element of a buffer
 */
void *gl_buf_at(gl_buf_t *buf, int idx, int attr);

/*
 * Function: gl_buf_next
 * Add a new empty row to the buffer and set the default index to it
 */
void gl_buf_next(gl_buf_t *buf);

/*
 * Function: gl_buf_enable
 * Enable the buffer for an opengl draw call.
 */
void gl_buf_enable(const gl_buf_t *buf);

/*
 * Function: gl_buf_disable
 * Disable a buffer after an opengl draw call.
 */
void gl_buf_disable(const gl_buf_t *buf);
