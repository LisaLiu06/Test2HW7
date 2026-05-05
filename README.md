# Homework 7

In this homework, you will create a chat client that is able to communicate with a server.

Instructions can be found under

    instructions.md

All your changes must be done under the `src` directory.

Your code should be organized as follows. (Ignoring all markdown files)

    src
        \_ Makefile
        \_ chatclientgui.c
        \_ chatserver.c
        \_ util.h

All files and directories must be named exactly as above case-sensitive.
You should not commit any extraneous files, such as object files or executables.

All rules about Makefiles and compilation are listed on HW2. **Your Makefile must produce the chatclientgui executable and may optionally create the chatserver too.** We will use our precompiled chatserver to test your client.

If you are allocating heap memory, you should follow all rules listed on HW3 and HW4.

All submissions must include at least five git commits with **meaningful** messages. If you're working in a team, each team member must commit at least three commits.

## Submission

To submit:

    git commit -am "hw7 completed"
    git push origin main

After submitting, you should re-test with your submitted version. Details on how to do that are [here](https://github.com/cs3157-borowski/guides/blob/main/submission.md).

## Acknowledgments

This homework was originally created by Dr. Brian Borowski. Parts of this homework were modified by Palash Sharma in Spring 2025, and Dr. Brian Borowski, John Mitnik, and Sam Weldon in Spring 2026.
