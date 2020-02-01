/**
 * @file util.c
 * @date Sunday, December 06, 2015 at 01:06:28 AM EST
 * @author Brandon Perez (bmperez)
 * @author Jared Choi (jaewonch)
 *
 * This file contains miscalaneous utilities for out system.
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
#include <unistd.h>             // Read() and write()
#include <errno.h>              // Error codes

/*----------------------------------------------------------------------------
 * Command-Line Parsing Utilities
 *----------------------------------------------------------------------------*/

// Parses the arg string as an integer for the given option
int parse_int(char option, char *arg_str, int *data)
{
    int rc;

    rc = sscanf(optarg, "%d", data);
    if (rc < 0) {
        perror("Unable to parse argument");
        return rc;
    } else if (rc != 1) {
        fprintf(stderr, "Error: Unable to parse argument '-%c %s' as an "
                "integer.\n", option, arg_str);
        return -EINVAL;
    }

    return 0;
}

// Parses the arg string as a double for the given option
int parse_double(char option, char *arg_str, double *data)
{
    int rc;

    rc = sscanf(optarg, "%lf", data);
    if (rc < 0) {
        perror("Unable to parse argument");
        return rc;
    } else if (rc != 1) {
        fprintf(stderr, "Error: Unable to parse argument '-%c %s' as an "
                "integer.\n", option, arg_str);
        return -EINVAL;
    }

    return 0;
}

int parse_resolution(char option, char *arg_str, int *height, int *width,
        int *depth)
{
    int rc;

    rc = sscanf(optarg, "%dx%dx%d\n", height, width, depth);
    if (rc < 0) {
        perror("Unable to parse argument");
        return rc;
    } else if (rc != 3) {
        fprintf(stderr, "Error: Unable to parse argument '-%c %s' as a height "
                " x width x depth resolution.\n", option, arg_str);
        return -EINVAL;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * File Operation Utilities
 *----------------------------------------------------------------------------*/

// Performs a robust read, reading out all bytes from the buffer
int robust_read(int fd, char *buf, int buf_size)
{
    int bytes_remain, bytes_read;
    int buf_offset;

    // Read out the bytes into the buffer, accounting for EINTR
    bytes_remain = buf_size;
    while (true)
    {
        buf_offset = buf_size - bytes_remain;
        bytes_read = read(fd, buf + buf_offset, bytes_remain);
        bytes_remain = (bytes_read > 0) ? bytes_remain - bytes_read
                                        : bytes_remain;

        /* If we were interrupted by a signal, then repeat the read. Otherwise,
         * if we encountered a different error or reached EOF then stop. */
        if (bytes_read < 0 && bytes_read != -EINTR) {
            return bytes_read;
        } else if (bytes_read == 0) {
            return buf_size - bytes_remain;
        }
    }

    // We should never reach here
    assert(false);
    return -EINVAL;
}

int robust_write(int fd, char *buf, int buf_size)
{
    int bytes_remain, bytes_written;
    int buf_offset;

    // Read out the bytes into the buffer, accounting for EINTR
    bytes_remain = buf_size;
    while (true)
    {
        buf_offset = buf_size - bytes_remain;
        bytes_written = write(fd, buf + buf_offset, bytes_remain);
        bytes_remain = (bytes_written > 0) ? bytes_remain - bytes_written
                                           : bytes_remain;

        /* If we were interrupted by a signal, then repeat the write. Otherwise,
         * if we encountered a different error or reached EOF then stop. */
        if (bytes_written < 0 && bytes_written != -EINTR) {
            return bytes_written;
        } else if (bytes_written == 0) {
            return buf_size - bytes_remain;
        }
    }

    // We should never reach here
    assert(false);
    return -EINVAL;
}
