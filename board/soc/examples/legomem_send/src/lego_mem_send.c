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
 * Internal Definitions
 *----------------------------------------------------------------------------*/

// A convenient structure to carry information around about the transfer
struct dma_transfer {
    int ctrl_channel;      // The channel used to send the data
    int data_channel;      // The channel used to send the data
    void *input_buf;        // The buffer to hold the input data
};

/*----------------------------------------------------------------------------
 * DMA File Transfer Functions
 *----------------------------------------------------------------------------*/

void build_write_cmd(void * buf, int buf_size) {
	uint8_t *bytes = (uint8_t *)buf;
	// build data witch sends through the network
	for (int i = 0; i < buf_size; i++)
		bytes[i] = i % 256;

	struct lego_header * header = (struct lego_header *)buf;
	header->pid = 0x5335;
	header->tag = 0x86;
	header->opcode = OP_REQ_WRITE;
	header->seqId = 0;
	header->size = buf_size;

	// Runtime info, commands from SoC needs to fill in runtime info
	header->dest_port = 0;
	header->dest_ip = 0xc0a80102u;
	header->cont = MAKE_CONT(LEGOMEM_CONT_NET, LEGOMEM_CONT_NET, LEGOMEM_CONT_NONE, LEGOMEM_CONT_NONE);

	struct lego_mem_access_header *access = (struct lego_mem_access_header *)buf;
	access->length = buf_size - LEGOMEM_ACCESS_HEADER_SIZE;
	access->va = 0;

}

static int legomem_test(axidma_dev_t dev)
{
    int rc;
    const int message_bytes = 293;
    void *input_buf;

    // SoC -> CoreMem -> Network
    // Allocate a buffer for the input file, and read it into the buffer
    input_buf = axidma_malloc(dev, message_bytes);

    if (input_buf == NULL) {
        fprintf(stderr, "Failed to allocate the input buffer.\n");
        rc = -ENOMEM;
        goto ret;
    }

    build_write_cmd(input_buf, message_bytes);

    // Perform the transfer
    // Perform the main transaction

    rc = axidma_oneway_transfer(dev, DATA_SEND_CHANNEL, input_buf, message_bytes, true);
    if (rc < 0) {
        fprintf(stderr, "DMA read write transaction failed.\n");
        goto free_input_buf;
    }

    printf("DATA Sent %d\n", message_bytes);

free_input_buf:
    axidma_free(dev, input_buf, message_bytes);
ret:
    return rc;
}

/*----------------------------------------------------------------------------
 * Main
 *----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
    int rc;
    axidma_dev_t axidma_dev;
    const array_t *tx_chans, *rx_chans;


    // Try opening the input and output images

    // Initialize the AXIDMA device
    axidma_dev = axidma_init();
    if (axidma_dev == NULL) {
        fprintf(stderr, "Error: Failed to initialize the AXI DMA device.\n");
        rc = 1;
        goto destroy_axidma;
    }

    // Get the tx and rx channels if they're not already specified
    tx_chans = axidma_get_dma_tx(axidma_dev);
    if (tx_chans->len < 1) {
        fprintf(stderr, "Error: No transmit channels were found.\n");
        rc = -ENODEV;
        goto destroy_axidma;
    }

    // Transfer the file over the AXI DMA
    rc = legomem_test(axidma_dev);
    rc = (rc < 0) ? -rc : 0;

destroy_axidma:
    axidma_destroy(axidma_dev);
ret:
    return rc;
}
