#include <string.h>
#include <stdio.h>

#include <uapi/vregion.h>

#include "jpeglib.h"

#include "test.h"
#include "rmem.h"
#include "rarray.h"

#include "../../core.h"

/*
 * XXX Tune me
 * The number of created threads
 */
static int test_nr_threads[] = { 25 };

const int buffer_size = 1024*1024*4;
const int num_iter = 1024 * 4;

static int job(void * sbuf, void *tbuf);

struct remote_mem *rarray, *warray;
int jpg_size;
void * jpg;
FILE *jpg_file;

static pthread_barrier_t thread_barrier;
static pthread_barrier_t thread_barrier2;

static inline void die(const char * str, ...)
{
	va_list args;
	va_start(args, str);
	vfprintf(stderr, str, args);
	fputc('\n', stderr);
	exit(1);
}

struct thread_info {
	int id;
	int cpu;
};

#define NR_MAX 128
uint64_t saved_ns[NR_MAX];

static void *thread_func(void *_ti)
{
	struct thread_info *ti = (struct thread_info *)_ti;
	int cpu, node;
	void *rbuf, *wbuf;
	struct timespec tstart={0,0}, tend={0,0};

	if (pin_cpu(ti->cpu))
		die("can not pin to cpu %d\n", ti->cpu);
	legomem_getcpu(&cpu, &node);
	dprintf_CRIT("thread id %d running on CPU %d\n", ti->id, cpu);
	pthread_barrier_wait(&thread_barrier);

	//usleep(ti->id * 5000);
	sleep(ti->id);

	/*
	 * XXX
	 * this test requires smaller vregion size
	 * adjust at all 3 places: cn, monitor and soc core.o
	 */
	BUG_ON(VREGION_SIZE != PAGE_SIZE);

	// alloc read/write arrays per thread
	ralloc(rarray, NULL, 0, VREGION_SIZE, ti->id);
	ralloc(rarray, NULL, 1, VREGION_SIZE, ti->id);

	pthread_barrier_wait(&thread_barrier);
	sleep(ti->id);

	// reg send buffer
	rbuf = malloc(buffer_size);
	wbuf = rcreatebuf(rarray, buffer_size, ti->id);

	memcpy(wbuf + sizeof(struct legomem_read_write_req), jpg, jpg_size);
	rarray_write(rarray, wbuf, 0, jpg_size, 0, ti->id);
	// XXX reassign, mem_write would destroy the buffer if batched send
	memcpy(wbuf + sizeof(struct legomem_read_write_req), jpg, jpg_size);

	uint64_t ns;

	pthread_barrier_wait(&thread_barrier2);

	clock_gettime(CLOCK_MONOTONIC, &tstart);
	for (size_t i = 0; i < num_iter; i++) {

	/** clock_gettime(CLOCK_MONOTONIC, &tstart); */
		rarray_read(rarray, rbuf, 0, config.jpg_size, 0, ti->id);
    /**     clock_gettime(CLOCK_MONOTONIC, &tend); */
    /** ns = (uint64_t) tend.tv_sec * 1000000000ULL - */
    /**               (uint64_t) tstart.tv_sec * 1000000000ULL + */
    /**               tend.tv_nsec - tstart.tv_nsec; */
    /**                 printf("READ %lu\n", ns); */


		// printf("get cat at %p, %02x %02x\n", rbuf, ((char*)rbuf)[0], ((char*)rbuf)[1]);

		/** FILE * savefile; */
		/** if (i == 0) savefile = fopen("cat_recv_first.jpg", "w"); */
		/** else savefile = fopen("cat_recv.jpg", "w"); */
		/** fwrite(rbuf, 1, config.jpg_size, savefile); */
		/** fclose(savefile); */

	/** clock_gettime(CLOCK_MONOTONIC, &tstart); */
		// int output_size = job(jpg, wbuf + sizeof(struct legomem_read_write_req));
		usleep(300);
		int output_size  = 196608;
    /**     clock_gettime(CLOCK_MONOTONIC, &tend); */
    /** ns = (uint64_t) tend.tv_sec * 1000000000ULL - */
    /**               (uint64_t) tstart.tv_sec * 1000000000ULL + */
    /**               tend.tv_nsec - tstart.tv_nsec; */
    /**                 printf("JOB %lu\n", ns); */

		//printf("decode size %d\n", output_size);

	/** clock_gettime(CLOCK_MONOTONIC, &tstart); */
		rarray_write(warray, wbuf, 0, output_size, 1, ti->id);
/**         clock_gettime(CLOCK_MONOTONIC, &tend); */
/** ns = (uint64_t) tend.tv_sec * 1000000000ULL - */
/**               (uint64_t) tstart.tv_sec * 1000000000ULL + */
/**               tend.tv_nsec - tstart.tv_nsec; */
/**             printf("WRITE %lu\n", ns); */

		/** printf("After Writeback %d\n", i); */
	}
	clock_gettime(CLOCK_MONOTONIC, &tend);

ns = (uint64_t) tend.tv_sec * 1000000000ULL -
	      (uint64_t) tstart.tv_sec * 1000000000ULL +
	      tend.tv_nsec - tstart.tv_nsec;
	saved_ns[ti->id] = ns;

    printf("thread id=%d Iamge Test end time %lu\n", ti->id, ns);

    return NULL;
}

// program
//int main(int argc, char *argv[])
int test_jpeg(char *unused)
{
	int k, i, ret;
	int nr_threads;
	pthread_t *tid;
	struct thread_info *ti;

    	parse_config(0, NULL);

	jpg_size = config.jpg_size;
	jpg = malloc(config.array_cell_size);
	jpg_file = fopen("./test/jpeg/data/cat.jpg", "r");
	    if (jpg_file == NULL) {
	    	printf("no file find!\n");
		exit(0);
	    }
            fread(jpg, 1, jpg_size, jpg_file);
            fclose(jpg_file);

        rarray = rinit(RMEM_ACCESS_READ | RMEM_ACCESS_WRITE,  (size_t)1024*1024*1024, NULL);
        warray = rarray;

	ti = malloc(sizeof(*ti) * NR_MAX);
	tid = malloc(sizeof(*tid) * NR_MAX);
	if (!tid || !ti)
		die("OOM");

	for (k = 0; k < ARRAY_SIZE(test_nr_threads); k++) {
		nr_threads = test_nr_threads[k];

		pthread_barrier_init(&thread_barrier, NULL, nr_threads);
		pthread_barrier_init(&thread_barrier2, NULL, nr_threads);

		for (i = 0; i < nr_threads; i++) {
			/*
			 * cpu 0 is used for gbn polling now
			 * in case
			 */
			ti[i].cpu = mgmt_dispatcher_thread_cpu + 1 + i;
			ti[i].id = i;
			ret = pthread_create(&tid[i], NULL, thread_func, &ti[i]);
			if (ret)
				die("fail to create test thread");
		}

		for (i = 0; i < nr_threads; i++) {
			pthread_join(tid[i], NULL);
		}

		for (i = 0; i < nr_threads; i++) {
    			printf("DONE thread id=%d Iamge Test end time %lu\n", i, saved_ns[i]);
		}
	}

	return 0;
    return 0;
}

// simple copy..
#if 0
static void job(void * sbuf, void *tbuf) {
	const char * message = "This is a message...";
    memcpy(sbuf, message, strlen(message) + 1);
    memcpy(tbuf, sbuf, buffer_size);
}
#endif

__used static int job(void * sbuf, void *tbuf) {
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
