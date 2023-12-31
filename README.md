# MTP
This program implements a multi-threaded text processing pipeline in C using pthreads. The pipeline consists of four stages: input collection, line separation, string replacement, and output writing. It employs thread synchronization mechanisms to handle data flow between the stages efficiently.

**Overview**<br>
The program utilizes four threads to perform the following tasks:

get_input: Collects input from stdin, producing data for buffer_1. <br>
line_separator: Consumes data from buffer_1, replaces newline characters with spaces, and produces data for buffer_2. <br>
plus_replace: Consumes data from buffer_2, replaces "++" with "^", and produces data for buffer_3. <br>
write_output: Consumes data from buffer_3, writes processed data to stdout in chunks of 80 characters. <br>
Each stage communicates with the next stage using buffers and utilizes mutexes and conditions to control access to the shared buffers.
