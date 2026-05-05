/*******************************************************************************
 * Name          : chatclientgui.c
 * Author        : 
 * Version       : 1.0
 * Date          : March 16, 2026
 * Last modified : April 19, 2026
 * Description   : Basic chat client with TCP sockets and ncurses GUI.
 * Dependencies  : sudo apt install libncurses5-dev libncursesw5-dev
 *                 Link with -lncurses
 * Keys:
 *   Enter          send message (adds to chat history)
 *   Up/Down        scroll chat history
 *   PageUp/PageDn  scroll faster
 *   Home/End       jump to top/bottom
 *   Ctrl+L         redraw
 *   Ctrl+C or F10  quit
 ******************************************************************************/
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "util.h"

#define PAD_ROWS_INITIAL 2000
#define MAX_MSGS 500

enum status {
    RUN_OK, CONN_LOST, SERVER_DOWN, SELECT_ERR, GOODBYE
};

/* Structs for storing messages */
typedef struct {
    char *author;
    char *text;
} msg_t;

/* Circular queue struct for storing messages */
typedef struct {
    msg_t  msgs[MAX_MSGS];
    size_t head_index;
    size_t count;
} msg_queue_t;

/* Global variables */
static volatile sig_atomic_t g_resized = true;
static volatile sig_atomic_t g_run_status = RUN_OK;
static char g_status_msg[77] =
    "Type 'bye' or press either F10 or Ctrl+C to quit, Ctrl+L to redraw.";

int pad_y = 0, view_y = 0, view_h = 0, pad_cols = 0;
bool follow_tail = true, is_a_tty;
msg_queue_t msgs;
WINDOW *chat_pad = NULL,
       *chat_frame = NULL,
       *input_win = NULL,
       *status_win = NULL;
char username[MAX_NAME_LEN + 1];
char inbuf[BUFLEN + 1];
char outbuf[MAX_MSG_LEN + 1];

/*******************************************************************************
 * GUI function definitions.
 * You are not responsible for understanding these,
 * but feel free to study them if you are interested!
 ******************************************************************************/
int clamp(int v, int lo, int hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void pad_add_wrapped(
        WINDOW *pad, int *pad_y, int pad_w,
        const char *author, const char *text) {
    int y = *pad_y;
    int x = 0;
    wmove(pad, y, x);

    // Print author (bold)
    if (author) {
        wattron(pad, A_BOLD);
        if (text) {
            wprintw(pad, "%s: ", author);
        } else {
            wprintw(pad, "%s", author);
        }
        wattroff(pad, A_BOLD);
        getyx(pad, y, x);
    }

    if (!text) {
        *pad_y = y + 2;
        return;
    }

    const char *p = text;
    while (*p) {
        // Skip leading spaces (prevents lines starting with space)
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (!*p) break;

        // Find word
        const char *word_end = p;
        while (*word_end && !isspace((unsigned char)*word_end)) {
            word_end++;
        }

        int word_len = word_end - p;
        // Case 1: Word fits on current line
        if (word_len <= (pad_w - x)) {
            waddnstr(pad, p, word_len);
            x += word_len;
            p = word_end;
        }
        else if (word_len <= pad_w) {
            // Case 2: Word doesn't fit on this line
            // Move entire word to next line
            y++;
            x = 0;
            wmove(pad, y, x);
            waddnstr(pad, p, word_len);
            x = word_len;
            p = word_end;
        } else {
            // Case 3: Word longer than full line → hard split
            int remaining = word_len;
            while (remaining > 0) {
                int space_left = pad_w - x;
                if (space_left == 0) {
                    y++;
                    x = 0;
                    wmove(pad, y, x);
                    space_left = pad_w;
                }
                int chunk = (remaining < space_left) ? remaining : space_left;
                waddnstr(pad, p, chunk);
                p += chunk;
                remaining -= chunk;
                getyx(pad, y, x);
            }
        }

        // Add space BETWEEN words (only if room and next char exists)
        if (*p && x < pad_w - 1) {
            waddch(pad, ' ');
            x++;
        }
    }

    // Sync cursor position
    getyx(pad, y, x);
    // Space between messages
    y += 2;
    *pad_y = y;
}

int rebuild_pad_from_messages_queue(
        WINDOW **ppad, int pad_rows, int pad_cols, const msg_queue_t *queue,
        int *out_pad_y) {
    WINDOW *new_pad = newpad(pad_rows, pad_cols);
    if (!new_pad) {
        return -1;
    }
    int pad_y = 0;
    for (size_t i = 0; i < queue->count; i++) {
        int idx = (queue->head_index + i) % MAX_MSGS;
        pad_add_wrapped(
            new_pad, &pad_y, pad_cols, queue->msgs[idx].author,
            queue->msgs[idx].text);
        if (pad_y >= pad_rows - 2) {
            break;
        }
    }
    if (*ppad) {
        delwin(*ppad);
    }
    *ppad = new_pad;
    *out_pad_y = pad_y;
    return 0;
}

int resize_window(bool init_setup) {
    g_resized = false;
    endwin();
    refresh();
    clear();

    int term_h, term_w;
    getmaxyx(stdscr, term_h, term_w);

    int input_h = 3,   // 3 lines high including horizontal lines.
        status_h = 3,  // 3 lines high including horizontal lines.
        chat_h = term_h - input_h - status_h; // Remainder of vertical space.

    if (term_h < 9 || term_w < 20) {
         mvprintw(0, 0, "Terminal too small.");
         refresh();
         if (chat_frame) {
             delwin(chat_frame);
             chat_frame = NULL;
         }
         if (input_win) {
             delwin(input_win);
             input_win = NULL;
         }
         if (status_win) {
             delwin(status_win);
             status_win = NULL;
         }
         return -1;
    }
    if (chat_frame) {
        wresize(chat_frame, chat_h, term_w);
    } else {
        chat_frame = newwin(chat_h, term_w, 0, 0);
    }
    if (input_win) {
        wresize(input_win, input_h, term_w);
        mvwin(input_win, chat_h, 0);
    } else {
        input_win = newwin(input_h, term_w, chat_h, 0);
    }
    if (status_win) {
        wresize(status_win, status_h, term_w);
        mvwin(status_win, chat_h + input_h, 0);
    } else {
        status_win = newwin(status_h, term_w, chat_h + input_h, 0);
    }
    pad_cols = term_w - 2;
    if (rebuild_pad_from_messages_queue(
            &chat_pad, PAD_ROWS_INITIAL, pad_cols, &msgs, &pad_y) != 0) {
        snprintf(g_status_msg, sizeof(g_status_msg),
                 "Error: Failed to create chat area.");
    } else if (!init_setup) {
        snprintf(g_status_msg, sizeof(g_status_msg),
                 "Window resized to %d x %d.", term_w, term_h);
    }
    follow_tail = true;
    return 0;
}

void draw_screen() {
    int term_w = getmaxx(stdscr);
    werase(status_win);
    box(status_win, 0, 0);
    mvwprintw(status_win, 0, 2, " Status ");
    mvwprintw(status_win, 1, 2, "%.*s", term_w - 4, g_status_msg);
    wnoutrefresh(status_win);

    int chat_h, chat_w;
    getmaxyx(chat_frame, chat_h, chat_w);
    view_h = chat_h - 2;
    if (follow_tail) {
        view_y = (pad_y > view_h) ? (pad_y - view_h) : 0;
    }
    werase(chat_frame);
    box(chat_frame, 0, 0);
    mvwprintw(chat_frame, 0, 2, " Chat ");
    wnoutrefresh(chat_frame);
    pnoutrefresh(chat_pad, view_y, 0, 1, 1, chat_h - 2, chat_w - 2);
   
    box(input_win, 0, 0);
    mvwprintw(input_win, 0, 2, " Message ");
    int input_w = getmaxx(input_win);
    wmove(input_win, 1, 1);
    for (int i = 1; i < input_w - 1; ++i) {
        waddch(input_win, ' ');
    }
   
    const char* prompt = "> ";
    int prompt_len = strlen(prompt),
        usable_w = input_w - 4 - prompt_len;
    if (usable_w < 0) {
        usable_w = 0;
    }
    int buf_len = strlen(outbuf);
    const char *display_buf = outbuf;
    if (buf_len > usable_w) {
        display_buf = outbuf + (buf_len - usable_w);
    }
    mvwprintw(input_win, 1, 2, "%s%s", prompt, display_buf);

    wmove(input_win, 1, 2 + prompt_len + strlen(display_buf));
    wnoutrefresh(input_win);
    doupdate();
}
/*******************************************************************************
 * End GUI functions.
 ******************************************************************************/

/*******************************************************************************
 * Circular message queue functions. Call these if needed.
 ******************************************************************************/

/** 
 * msgs_queue_init: Initialize a msg_queue_t
 */
void msgs_queue_init(msg_queue_t *queue) {
    queue->count = 0;
    queue->head_index = 0;
}

/**
 * msgs_queue_add: Add a new message to the queue
 */
void msgs_queue_add(msg_queue_t *queue, const char *author, const char *text) {
    bool full = (queue->count == MAX_MSGS);
    int slot = (queue->head_index + queue->count) % MAX_MSGS;
    if (full) {
        free(queue->msgs[queue->head_index].author);
        free(queue->msgs[queue->head_index].text);
        queue->msgs[queue->head_index].author = NULL;
        queue->msgs[queue->head_index].text   = NULL;
        queue->head_index = (queue->head_index + 1) % MAX_MSGS;
    } else {
        queue->count++;
    }
    queue->msgs[slot].author = author ? strdup(author) : NULL;
    queue->msgs[slot].text   = text   ? strdup(text)   : NULL;
}

/**
 * msgs_queue_free: Cleanup the message queue
 */
void msgs_queue_free(msg_queue_t *queue) {
    for (size_t i = 0; i < queue->count; i++) {
        free(queue->msgs[(queue->head_index + i) % MAX_MSGS].author);
        free(queue->msgs[(queue->head_index + i) % MAX_MSGS].text);
    }
    // Don't free queue->msgs itself, since it is a global variable.
    msgs_queue_init(queue);
}

/*******************************************************************************
 * End circular message queue functions.
 ******************************************************************************/

int handle_client_socket(int client_socket) {
    // TODO: Read the incoming message and use the number of bytes read to
    // check if the client disconnected.
    int bytes_received = recv_with_length(client_socket, inbuf, sizeof(inbuf));

    if(bytes_received == -1) {
        snprintf(g_status_msg, sizeof(g_status_msg), "Warning: Failed to receive incoming message. %s", strerror(errno));
        return RUN_OK; // ??????
    }
    else if (bytes_received == 0) {
        return CONN_LOST;
    }
    if(strcmp(inbuf, "bye") == 0) {
        return SERVER_DOWN;
    }

    // Add the new message to the GUI.
    if (is_a_tty) {
        if (inbuf[0] == '[') {
            char *sender = inbuf + 1,
                 *endname = strchr(sender, ']');
            *endname = '\0';
            int sender_len = (int)strlen(sender);
            msgs_queue_add(&msgs, sender, inbuf + sender_len + 4);
            if (msgs.count == MAX_MSGS || pad_y >= PAD_ROWS_INITIAL - 2) {
                rebuild_pad_from_messages_queue(
                    &chat_pad, PAD_ROWS_INITIAL, pad_cols, &msgs, &pad_y);
            }
            else {
                pad_add_wrapped(
                    chat_pad, &pad_y, pad_cols, sender, inbuf + sender_len + 4);
            }
        } else {
            msgs_queue_add(&msgs, NULL, inbuf);
            if (msgs.count == MAX_MSGS || pad_y >= PAD_ROWS_INITIAL - 2) {
                rebuild_pad_from_messages_queue(
                    &chat_pad, PAD_ROWS_INITIAL, pad_cols, &msgs, &pad_y);
            }
            else {
                pad_add_wrapped(chat_pad, &pad_y, pad_cols, NULL, inbuf);
            }
        }
        snprintf(g_status_msg, sizeof(g_status_msg), "Message received.");
    }
    return RUN_OK;
}

int handle_stdin_file(int client_socket) {
    int retval = get_string(outbuf, MAX_MSG_LEN);
    if (retval == END_OF_FILE) {
        return GOODBYE;
    } else if (retval != TOO_LONG && retval != NO_INPUT) {
        send_with_length(client_socket, outbuf, strlen(outbuf) + 1);
        if (strcmp(outbuf, "bye") == 0) {
            return GOODBYE;
        }
    }
    return RUN_OK;
}

int handle_stdin(int client_socket) {
    if (!is_a_tty) {
        return handle_stdin_file(client_socket);
    }
    // This section deals with the GUI, don't change it.
    int ch = getch();
    if (ch == KEY_F(10) || ch == 3) {
        return GOODBYE;
    }
    if (ch == 12) {
        g_resized = true;
    } else if (ch == KEY_UP) {
        follow_tail = false;
        view_y = clamp(view_y - 1, 0, (pad_y > view_h ? pad_y - view_h : 0));
    } else if (ch == KEY_DOWN) {
        view_y = clamp(view_y + 1, 0, (pad_y > view_h ? pad_y - view_h : 0));
        if (view_y >= (pad_y > view_h ? pad_y - view_h : 0)) {
            follow_tail = true;
        }
    } else if (ch == KEY_PPAGE) {
        follow_tail = false;
        view_y = clamp(view_y - view_h, 0,
                    (pad_y > view_h ? pad_y - view_h : 0));
    } else if (ch == KEY_NPAGE) {
        view_y = clamp(view_y + view_h, 0,
                    (pad_y > view_h ? pad_y - view_h : 0));
        if (view_y >= (pad_y > view_h ? pad_y - view_h : 0)) {
            follow_tail = true;
        }
    } else if (ch == KEY_HOME) {
        follow_tail = false;
        view_y = 0;
    } else if (ch == KEY_END) {
        follow_tail = true;
    } else if (ch == '\n' || ch == KEY_ENTER) {
        const char *s = outbuf;
        // Skip leading spaces.
        while (*s && isspace((unsigned char)*s)) {
            s++;
        }
        if (*s) {
            if (strnlen(s, MAX_MSG_LEN + 1) == MAX_MSG_LEN + 1) {
                // This should never happen.
                snprintf(g_status_msg, sizeof(g_status_msg),
                         "Error: Message not sent. Too long.");
            } else {
                // Add input to the GUI chat log.
                msgs_queue_add(&msgs, username, s);
                if (msgs.count == MAX_MSGS || pad_y >= PAD_ROWS_INITIAL - 2) {
                    rebuild_pad_from_messages_queue(
                        &chat_pad, PAD_ROWS_INITIAL, pad_cols, &msgs, &pad_y);
                }
                else {
                    pad_add_wrapped(
                        chat_pad, &pad_y, getmaxx(chat_pad), username, s);
                }
               
                // TODO: Send the message to the server
                // Write "Message sent." or a warning message into g_status_msg.
                if(send_with_length(client_socket, s, strlen(s) + 1) == -1) {
                    snprintf(g_status_msg, sizeof(g_status_msg), "Warning: Failed to send message to server. %s.\n", strerror(errno));
                }
                else {
                    snprintf(g_status_msg, sizeof(g_status_msg), "Message sent."); //
                }

                // TODO: Handle "bye" received on stdin.
                if(strcmp(s, "bye") == 0) {
                    outbuf[0] = '\0';
                    follow_tail = true;
                    return GOODBYE;
                }

            }
        }
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
        size_t len = strlen(outbuf);
        if (len > 0) {
            outbuf[len - 1] = '\0';
        }
    } else if (isprint(ch)) {
        size_t len = strnlen(outbuf, sizeof(outbuf));
        if (len < MAX_MSG_LEN) {
            outbuf[len] = (char)ch;
            outbuf[len + 1] = '\0';
        }
    }
    return RUN_OK;
}

/* Signal handler */
void handle_signal(int sig) {
    if (sig == SIGWINCH) {
        g_resized = true;
    }
    /* TODO: Handle SIGINT */
    else if (sig == SIGINT) {
        g_run_status = GOODBYE; // ???????
    }
}

void close_client_socket(int client_socket) {
    if (fcntl(client_socket, F_GETFD) != -1) {
        close(client_socket);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) { // HOW YOU ARE SUPPOSED TO PASS IN THE ARGUMENTS
        fprintf(stderr, "Usage: %s <server IP> <port> <username>\n", argv[0]);
        // SO PRINTS: "Usage: ./chatclientgui <server IP> <port> <username>"
        return EXIT_FAILURE;
    }
    is_a_tty = isatty(STDIN_FILENO);

    // TODO: Set up struct sockaddr_in proprties.
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;


    // TODO: Parse server IP.
    // converts the ip address to binary. Store the binary in &server_addr.sin_addr
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Error: Invalid IP address '%s'.\n", argv[1]);
        return EXIT_FAILURE;
    }

    // TODO: Parse server port.
    int port;
    bool suc = get_integer(argv[2], &port);
    if (!suc) {
        fprintf(stderr, "Error: Invalid input '%s' received for port number.\n", argv[2]);
        return EXIT_FAILURE;
    }
    if (port < 1024 || port > 65535) {
        fprintf(stderr, "Error: Port must be in range [1024, 65535].\n");
        return EXIT_FAILURE;
    }
    server_addr.sin_port = htons(port);
    
    // TODO: Parse username and ensure valid length.
    size_t username_len = strlen(argv[3]);
    if (username_len < 1 || username_len > MAX_NAME_LEN) {
        fprintf(stderr, "Error: Username '%s' exceeds limit of %d characters.\n", argv[3], MAX_NAME_LEN);
        return EXIT_FAILURE;
    }
    snprintf(username, sizeof(username), "%s", argv[3]);

    // TODO: Create TCP client socket. 
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        fprintf(stderr, "Error: Failed to create socket. %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    
    // TODO: Establish connection to chat server.
    if(connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error: Failed to connect to server. %s.\n", strerror(errno));
        close_client_socket(client_socket);
        return EXIT_FAILURE;
    }

    // TODO: Receive welcome message and send username to chat server.
        //(a) receive welcome message

    int bytes_received = recv_with_length(client_socket, inbuf, sizeof(inbuf));

    if(bytes_received == 0) {
        fprintf(stderr, "Error: Server closed connection. %s.\n", strerror(errno));
        close_client_socket(client_socket);
        return EXIT_FAILURE; 
    }

    else if (bytes_received == -1) {
        fprintf(stderr, "Error: Failed to receive welcome message. %s.\n", strerror(errno));
        close_client_socket(client_socket);
    }

    if (send_with_length(client_socket, username, strlen(username) + 1) == -1) {
        fprintf(stderr, "Error: Failed to send username to server. %s.\n",
                strerror(errno));
        close_client_socket(client_socket);
        return EXIT_FAILURE;
    }


    // TODO: Use sigaction to install handle_signal for SIGINT and SIGWINCH.
    struct sigaction action; 
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;

    if(sigaction(SIGINT, &action, NULL) == -1 || sigaction(SIGWINCH, &action, NULL) == -1) {
        fprintf(stderr, "Error: Failed to register signal handler. %s.\n", strerror(errno));
        close_client_socket(client_socket);
        return EXIT_FAILURE; 
    }
    

    // tells you if it's a teletypwriter
    //WTK stdin coming in from screen when user types, or from a file
    if (is_a_tty) {
        // GUI setup
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        /* Non-blocking-ish getch so UI draws immediately. */
        timeout(50);
        curs_set(1);
   
        // TODO: Initialize the global message queue.
        msgs_queue_init(&msgs); /// ???
 
        // TODO: Break up the welcome message in inbuf into multiple lines 
        // using newline as a delimiter (see: man strtok)
        // Pass each line as the second (not third) parameter to msgs_queue_add
        // in a loop so they appear in bold.

        char *line = strtok(inbuf, "\n");
        while (line != NULL) {
            msgs_queue_add(&msgs, line, NULL);
            line = strtok(NULL, "\n");
        }
    }

    // TODO: Declare fd_set.
    fd_set sockset; 

    inbuf[0] = '\0';
    bool init_setup = true;
    while (true) {
        switch (g_run_status) {
            case CONN_LOST:
                snprintf(g_status_msg, sizeof(g_status_msg),
                         "Connection to server has been lost.");
                break;
            case SERVER_DOWN:
                snprintf(g_status_msg, sizeof(g_status_msg),
                         "Server initiated shutdown.");
                break;
            case SELECT_ERR:
                snprintf(g_status_msg, sizeof(g_status_msg),
                         "Error: select() failed. %s.\n", strerror(errno));
                break;
            case GOODBYE:
                snprintf(g_status_msg, sizeof(g_status_msg), "Goodbye.");
                break;
            default:
                break;
        }

        if (is_a_tty) {
            if (g_resized) {
                if (resize_window(init_setup) < 0) {
                    continue;
                }
            }
            draw_screen();
            if (g_run_status != RUN_OK) {
                break;
            }
            init_setup = false;
        }

        // TODO: Use select to multiplex between stdin and the client socket.
        FD_ZERO(&sockset);
        FD_SET(STDIN_FILENO, &sockset);
        FD_SET(client_socket, &sockset);

        int max_fd = client_socket > STDIN_FILENO ? client_socket : STDIN_FILENO;

        if (select(max_fd + 1, &sockset, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            g_run_status = SELECT_ERR;
            continue;
        }

        //if user typed something
        if (FD_ISSET(STDIN_FILENO, &sockset)) {
            g_run_status = handle_stdin(client_socket);
        }
        
        //if server sent something ?
        if (g_run_status == RUN_OK && FD_ISSET(client_socket, &sockset)) {
            g_run_status = handle_client_socket(client_socket);
        }

        if (!is_a_tty && g_run_status != RUN_OK) {
            break;
        }
    }
    // TODO: Close client socket.
    close_client_socket(client_socket);

    if (is_a_tty) {
        // Give time to see final status message.
        sleep(1);

        // Free up gui resources.
        delwin(chat_pad);
        delwin(chat_frame);
        delwin(input_win);
        delwin(status_win);
        endwin();

        // TODO: Add any additional cleanup code needed.
        msgs_queue_free(&msgs);
    }
    return EXIT_SUCCESS;
}
