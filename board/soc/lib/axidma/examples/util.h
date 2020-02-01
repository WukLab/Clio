/**
 * @file util.c
 * @date Sunday, December 06, 2015 at 01:10:08 AM EST
 * @author Brandon Perez (bmperez)
 * @author Jared Choi (jaewonch)
 *
 * This file contains miscalaneous utilities for out system.
 *
 * @bug No known bugs.
 **/

#ifndef UTIL_H_
#define UTIL_H_

// Command-line parsing utilities
int parse_int(char option, char *arg_str, int *data);
int parse_double(char option, char *arg_str, double *data);
int parse_resolution(char option, char *arg_str, int *height, int *width,
        int *depth);

// File operation utilities
int robust_read(int fd, char *buf, int buf_size);
int robust_write(int fd, char *buf, int buf_size);

#endif /* UTIL_H_ */
