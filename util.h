/*******************************************************************************
 * Name          : util.h
 * Author        : Brian S. Borowski, Palash Sharma
 * Version       : 1.3
 * Date          : April 24, 2020
 * Last modified : January 10, 2025
 * Description   : Helpful utility functions for chat server/client.
 ******************************************************************************/
#ifndef _UTIL_H_
#define _UTIL_H_

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_MSG_LEN 1024
#define MAX_NAME_LEN 20
// +4 = '[' before name, "]: " after name
#define BUFLEN (MAX_MSG_LEN + MAX_NAME_LEN + 4)

enum parse_string_t { STRING_OK, NO_INPUT, END_OF_FILE, TOO_LONG };

/* Functions that should be used in client and server. */
bool parse_int(const char *input, int *i, const char *usage);
int send_with_length(int socket, const char *msg, uint16_t len);
int recv_with_length(int socket, char *buf, size_t buf_size);
int get_string(char *buf, const size_t maxlen);

/**
 * Takes as input a string and an in-out parameter value.
 * If the string can be parsed, the integer value is assigned to the value
 * parameter and true is returned.
 * Otherwise, value is unchanged and false is returned.
 */
bool get_integer(char *input, int *value) {
    char *stopstr;
    long lvalue = strtol(input, &stopstr, 10);
    if (stopstr - input != strlen(input) || errno == ERANGE ||
            lvalue < INT_MIN || lvalue > INT_MAX) {
        return false;
    }
    *value = (int)lvalue;
    return true;
}

int send_with_length(int socket, const char *msg, uint16_t len) {
    uint16_t len_network = htons(len);
    if (send(socket, &len_network, sizeof(len_network), 0) !=
            sizeof(len_network) ||
        send(socket, msg, len, 0) != len) {
        return -1;
    }
    return 0;
}

int recv_with_length(int socket, char *buf, size_t buf_size) {
    uint16_t len;
    ssize_t bytes_recvd;

    if ((bytes_recvd = recv(socket, &len, sizeof(len), 0)) <= 0) {
        return bytes_recvd;
    }

    if (bytes_recvd != sizeof(len) || (len = ntohs(len)) > buf_size ||
        (bytes_recvd = recv(socket, buf, len, 0)) != len) {
        return -1;
    }
    return bytes_recvd;
}

/**
 * Reads a string from STDIN up to maxlen characters. If more than maxlen
 * characters are supplied, the function consumes them.
 * Returns OK, NO_INPUT, END_OF_FILE, or TOO_LONG.
 */
int get_string(char *buf, const size_t maxlen) {
    int i = 0, ch;
    bool too_long = false;
    errno = 0;
    while ((ch = getc(stdin)) != EOF) {
        if (i > maxlen) {
            too_long = true;
        }
        if (ch == '\n') {
            break;
        }
        if (i < maxlen) {
            buf[i] = ch;
        }
        i++;
    }

    if (ch == EOF) {
        // If EOF signifies failure...
        if (errno != 0) {
            fprintf(stderr, "Warning: Failed to read from stdin. %s.\n",
                    strerror(errno));
        }
        return END_OF_FILE;
    } else if (ch != '\n') {
        // Consume any extra characters on line.
        ch = getc(stdin);
        while (ch != EOF && ch != '\n') {
            ch = getc(stdin);
        }
    }
    if (too_long) {
        return TOO_LONG;
    }
    buf[i] = '\0';
    if (i == 0) {
        return NO_INPUT;
    }

    return STRING_OK;
}

#endif
