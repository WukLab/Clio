/**
 * @file axidma_transfer.c
 * @date Sunday, November 29, 2015 at 12:23:43 PM EST
 * @author Brandon Perez (bmperez)
 * @author Jared Choi (jaewonch)
 *
 * This program performs a simple AXI DMA transfer. It takes the input file,
 * loads it into memory, and then sends it out over the PL fabric. It then
 * receives the data back, and places it into the given output file.
 *
 * By default it uses the lowest numbered channels for the transmit and receive,
 * unless overriden by the user. The amount of data transfered is automatically
 * determined from the file size. Unless specified, the output file size is
 * made to be 2 times the input size (to account for creating more data).
 *
 * This program also handles any additional channels that the pipeline
 * on the PL fabric might depend on. It starts up DMA transfers for these
 * pipeline stages, and discards their results.
 *
 * @bug No known bugs.
 **/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <fcntl.h>              // Flags for open()
#include <sys/stat.h>           // Open() system call
#include <sys/types.h>          // Types for open()
#include <stdint.h>
#include <unistd.h>             // Close() system call
#include <string.h>             // Memory setting and copying
#include <getopt.h>             // Option parsing
#include <errno.h>              // Error codes

// #include "util.h"               // Miscellaneous utilities
// #include "conversion.h"         // Convert bytes to MiBs
#include "libaxidma.h"          // Interface ot the AXI DMA library
#include "lego_mem.h"

#define DATA_SEND_CHANNEL 0
#define DATA_RECV_CHANNEL 1
#define CTRL_SEND_CHANNEL 2
#define CTRL_RECV_CHANNEL 3

/*----------------------------------------------------------------------------
 * Main
 *----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
    int rc;
    axidma_dev_t axidma_dev;
    const array_t *tx_chans, *rx_chans;

    void * buf;
    int max_bytes = 4096;


    // Initialize the AXIDMA device
    axidma_dev = axidma_init();
    if (axidma_dev == NULL) {
        fprintf(stderr, "Error: Failed to initialize the AXI DMA device.\n");
        rc = 1;
        goto destroy_axidma;
    }

    // Get the tx and rx channels if they're not already specified
    rx_chans = axidma_get_dma_rx(axidma_dev);
    if (rx_chans->len < 1) {
        fprintf(stderr, "Error: No transmit channels were found.\n");
        rc = -ENODEV;
        goto destroy_axidma;
    }


    // Try opening the input and output images
    buf = axidma_malloc(axidma_dev, max_bytes);

	if (buf == NULL) {
		fprintf(stderr, "Failed to allocate the input buffer.\n");
		rc = -ENOMEM;
		goto ret;
	}

    // Since the function is blocking, we need to call with exact data size
    // First, get the header
    rc = axidma_oneway_transfer(axidma_dev, DATA_RECV_CHANNEL, buf, LEGOMEM_HEADER_SIZE, true);
    if (rc < 0) {
		fprintf(stderr, "DMA read header transaction failed.\n");
		goto free_buf;
    }

    // Then read the payload
    int payload_size = ((struct lego_mem_header *)buf)->size - LEGOMEM_HEADER_SIZE;
    printf("Total Size %d, Expected payload size %d\n", ((struct lego_mem_header *)buf)->size, payload_size);
    rc = axidma_oneway_transfer(axidma_dev, DATA_RECV_CHANNEL, buf + LEGOMEM_HEADER_SIZE, payload_size, true);
    if (rc < 0) {
		fprintf(stderr, "DMA read payload transaction failed.\n");
		goto free_buf;
    }

    printf("Lego Mem Read %d Bytes", LEGOMEM_HEADER_SIZE + payload_size);

    // Recv Test
free_buf:
	axidma_free(axidma_dev, buf, max_bytes);

destroy_axidma:
    axidma_destroy(axidma_dev);
ret:
    return rc;
}
