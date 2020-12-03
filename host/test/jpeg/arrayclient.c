#include <string.h>
#include <stdio.h>

#include "jpeglib.h"

#include "test.h"
#include "rmem.h"
#include "rarray.h"

const int buffer_size = 1024*1024*4;
const int num_iter = 1024;

static int job(void * sbuf, void *tbuf);

// program
int main(int argc, char *argv[]) {
    void *rbuf, *wbuf;
    struct remote_mem *rarray, *warray;
    struct timespec tstart={0,0}, tend={0,0};

    parse_config(argc, argv);

    if (config.legomem) {
        rarray = rinit(RMEM_ACCESS_READ | RMEM_ACCESS_WRITE,  (size_t)1024*1024*1024, NULL);
        warray = rarray;

        ralloc(rarray, NULL, 0, 1024*1024);
        ralloc(rarray, NULL, 1, 1024*1024);

        rbuf = malloc(buffer_size);
        wbuf = rcreatebuf(rarray, buffer_size);

        // one special client will write the cat image...
        if (config.program != NULL) {
            int jpg_size = config.jpg_size;
            void * jpg = malloc(config.array_cell_size);
            FILE *jpg_file = fopen("./data/cat.jpg", "r");

            fread(jpg, 1, jpg_size, jpg_file);
            fclose(jpg_file);

            memcpy(buf, jpg, jpg_size);
            rarray_write(rarray, wbuf, i, jpg_size);
            return 0;
        }

    } else {
        // create array
        rarray = rinit(RMEM_ACCESS_READ,  (size_t)1024*1024*1024, config.server_rdma_read_url);
        rbuf = rcreatebuf(rarray, buffer_size);

        warray = rinit(RMEM_ACCESS_WRITE, (size_t)1024*1024*1024, config.server_rdma_write_url);
        wbuf = rcreatebuf(warray, buffer_size);
    }


    clock_gettime(CLOCK_MONOTONIC, &tstart);
    for (size_t i = 0; i < num_iter; i++) {
        rarray_read(rarray, rbuf, i, config.jpg_size);

        // printf("get cat at %p, %02x %02x\n", rbuf, ((char*)rbuf)[0], ((char*)rbuf)[1]);

        int output_size = job(rbuf, wbuf);

        // printf("decode size %d\n", output_size);

        rarray_write(warray, wbuf, i, output_size);
    }
    clock_gettime(CLOCK_MONOTONIC, &tend);

    uint64_t ns = (uint64_t) tend.tv_sec * 1000000000ULL -
                  (uint64_t) tstart.tv_sec * 1000000000ULL +
                  tend.tv_nsec - tstart.tv_nsec;

    printf("%lu\n", ns);

}

// simple copy..
#if 0
static void job(void * sbuf, void *tbuf) {
	const char * message = "This is a message...";
    memcpy(sbuf, message, strlen(message) + 1);
    memcpy(tbuf, sbuf, buffer_size);
}
#endif

static int job(void * sbuf, void *tbuf) {
    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, sbuf, config.jpg_size);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int pixel_size = cinfo.output_components;

    int bmp_size = width * height * pixel_size;
    int row_stride = width * pixel_size;

    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned char *buffer_array[1];
        buffer_array[0] = tbuf + (cinfo.output_scanline) * row_stride;
        jpeg_read_scanlines(&cinfo, buffer_array, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return bmp_size;
}


