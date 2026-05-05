# Chat Client

## Overview

The goal of this assignment is to create a chat client with a graphical user interface
that is able to communicate with a server. The client will send messages to the server,
which is then able to broadcast the message to all the other clients on the system.
Similarly, the client will be able to receive messages directly from the server,
thus enabling the user to read messages sent by all other clients on the system.

The code that handles the GUI has been provided for you. One of the main goals of this
assignment is to give you experience reading, understanding, and adding to a substantial
existing codebase. Your job is to read the skeleton code carefully and fill in the
blank sections that enable communication with the chat server. A sample
executable has been provided for you on BSB; please study its behavior.

You must generate an executable called `chatclientgui`.
## Part 0

You will be hosting your server on IP address: 127.0.0.1, which will listen and wait for connections on a port. For this part, you will just do a little math to find the port numbers that you should use in your testing. This does not mean that your client will **only** be connecting to servers listening for connections on ports in your port number range, but this is just to reduce port number collisions from all the students testing their client on our BSB server.

(1) Log into the server and run the `id -u` command to find your user ID on the BSB server.
```
$ id -u
1003
```
Take the last three digits: 003

(2) Pick a number, x, between 10 and 63 (inclusive), and use this function to obtain your port number:

`1000 * x + (the last three digits of your user ID)`

For example, a person with the id 1003 can use any of these ports: 10003, 21003, 62003, etc.

Using this scheme, we assign 54 ports to each student without overlap. It may take some time for a port to become available after you just tested with it, so rotate through your available ports when testing.


## Part 1

This program requires three command line arguments: server IP, port, and a username. If any other number of arguments has been supplied, the usage message must be printed
to stderr where `%s` is the executable name, and you must return `EXIT_FAILURE`.

    "Usage: %s <server IP> <port> <username>\n"

The IPv4 address must be written in dot-decimal notation: a.b.c.d, where
a, b, c, and d can be any integer 0 through 255. If you are running the server
locally, you specify the address as 127.0.0.1. Do not worry about getting the word
localhost to work. Use `inet_pton()` to check the IP address for validity. If
the IP address is invalid, you must print

    "Error: Invalid IP address '%s'.\n"

and exit in failure.

The port must be an integer in the range [1024, 65535]. If it is not an integer, you must print

    "Error: Invalid input '%s' received for port number.\n"

and exit in failure.

If the port number is out of range, you must print

    "Error: Port must be in range [1024, 65535].\n"

and exit in failure.

Check that the username supplied in the command line arguments is of appropriate length
(i.e. in the range [1, MAX_NAME_LEN], MAX_NAME_LEN is defined in util.h) and print the following message if it is not, where `%s` is the supplied username.

    "Error: Username '%s' exceeds limit of %d characters.\n"


## Part 3

After parsing the command line arguments, take the following steps:

1. Create a TCP socket
2. Connect to the server
3. Receive the welcome message from the server

    If the number of bytes received is 0, that means the server closed the connection on the client, and you should print an error message and exit in failure.

    *Note: You should store all messages received from the socket in
    inbuf and produce all outbound messages from STDIN in outbuf. Null-terminate all strings
    before sending them to make processing input easier on the server. The server null-terminates
    all strings sent to the client.*

4. Send the username to the server.

    Note that messages must be sent and received using the `send_with_length()` and `recv_with_length()` functions, found in util.h.

    If any of these steps fail, you must print an error message (that includes the strerror of errno) to stderr and exit in failure.

5. Next, complete the definition of `handle_signal()` and register it as the signal handler
for SIGINT and SIGWINCH via sigaction.

## Part 4

The code to set up and begin running the GUI is already complete and should not be altered.

The GUI supports scrolling up to view previous messages by keeping the chat's message history
in memory. This is achieved using a fixed-size circular queue data structure, *msg_queue_t*,
which is provided for you. It exposes the following API functions that you will need to interact with:

```
msgs_queue_init
msgs_queue_add
msgs_queue_free
```

Parse the welcome message you received from the server and add each line to the message queue. You should see it at the top of the GUI like this:

![Welcome Message](https://i.ibb.co/cH8XjWr/Screenshot-2026-04-20-12-21-42-PM.png)

From this point forward, if a failure condition occurs, instead of printing to the terminal, use `snprintf()` to print an error message into the `g_status_msg` buffer to be displayed at the bottom of the GUI, like this:

![GUI Error Message Example](https://i.ibb.co/yBGhz2fN/Screenshot-2026-04-20-12-22-26-PM.png)


## Part 5

Using `fd_set` and `select` (man 2 select), your program should now loop forever,
prompting the user for input and determining if there is activity on either of the two
file descriptors.

The first thing that happens in each iteration of the loop is a switch statement on the
value of `g_run_status`. The provided code already handles each case correctly. Your job
is to ensure g_run_status has the correct value whenever it is checked.

Both the socket and `STDIN_FILENO` should be
added to the `fd_set`. If there is activity on:

1. `STDIN_FILENO`

    Your chat client should work if the user types directly into the terminal or if a file is redirected
    through stdin. A newline character marks the end of a message and must be sent before reading additional
    characters from the file. Code has been provided for you in the `handle_stdin()` and `handle_stdin_file()` functions that
    read one character at a time and handle special inputs that trigger GUI functionality. Note, `handle_stdin_file()` does no error
    checking by design. It is meant for you to use as a means of sending messages in bulk (via file redirection).

    Your job is to complete the TODOs in `handle_stdin()`.

    Send the message to the server. 

    If sending the message to the server succeeds, display the following message via `g_status_msg`:

        "Message sent."

    If sending fails, display the following error, where `%s` is strerror(errno) (but do not kill the program):

        "Warning: Failed to send message to server. %s.\n"

    If the message is equal to "bye", return from `handle_stdin()` and set `g_run_status` to the appropriate value to shut down the client. 


3. Client Socket

    Again, a skeleton function called `handle_client_socket()` has been provided for you. Your job is to complete the TODOs found inside.

    Receive data from the socket and store it in inbuf. If the number
    of bytes received is -1, warn the user (via `g_status_msg`) with the following message, where `%s` is strerror(errno):

        "Warning: Failed to receive incoming message. %s"

    If the number of bytes received is 0, the server abruptly broke the
    connection with the client, the server was shut down, crashed, or perhaps the network failed.
    Return the appropriate value from `handle_client_socket()` and set `g_run_status`.

    Compare the message received to "bye" and if it's equal, return the appropriate value from `handle_client_socket()` and set `g_run_status`.

    Otherwise, print "Message received." into `g_status_msg`, and the provided code will then display the message from the server in the GUI.

Make sure you close the socket before your program terminates. Be sure to check for memory leaks as well.

## Testing your code

We have provided the code for the chat server in `chatserver.c`. The code for the server is complete, so you must not make any changes to it. But go through it and try to understand it; this will really help with coding the client. Additionally, reading the server code can help you understand the expected behavior of the client. Here is how to test your work. First, open up two windows:

1. Add chatserver to your Makefile so that you can create the executable.
2. In one window, start your `chatserver` by running `./chatserver <port-number>`. Remember to use an appropriate port number to reduce collisions with other students.
3. Then, in your second window, build your `chatclientgui` and run using `./chatclientgui 127.0.0.1 <port-number> <username>`. This should be the port number that you supplied to `chatserver`.
4. On your third and fourth windows, create additional `chatclientgui` instances and run using `./chatclientgui 127.0.0.1 <port-number> <username>`. This should be the port number that you supplied to `chatserver`. The usernames should be distinct.
5. You can also try sending an entire file's worth of data by redirecting a file such as input.txt to stdin, as in `./chatclientgui 127.0.0.1 <port-number> <username> < input.txt`.

Like all homeworks, you should code and test your implementation incrementally. You can test at different stages, but the first thing you want to do is make sure you are establishing the connection between the server and the client. As usual, if you have questions, please post on EdStem or attend OH.
