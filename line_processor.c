#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Size of the buffers

#define MAX_CHAR 80
#define MAX_LINE 1000
#define LINE_COUNT 50
#define ALLSTOP "\3"


// Buffer Structure
struct buffer 
{
    char buffer[LINE_COUNT][MAX_LINE];
    pthread_mutex_t mutex;
    pthread_cond_t is_full;
};
struct buffer *buffer_1, *buffer_2, *buffer_3;

// Global Pipeline Buffers
void put_buff_1(struct buffer *buffer, int line, char argv[]);
void get_buff_1(struct buffer *buffer, int line, char output[]);

void put_buff_2(struct buffer *buffer, int line, char argv[]);
void get_buff_2(struct buffer *buffer, int line, char output[]);

void put_buff_3(struct buffer *buffer, int line, char argv[]);
void get_buff_3(struct buffer *buffer, int line, char output[]);


void str_gsub(char* string, const char* substring, char* replacement);

pthread_mutex_t mutex_1 = PTHREAD_MUTEX_INITIALIZER; // Initialize the mutex for buffer 1
pthread_cond_t full_1 = PTHREAD_COND_INITIALIZER; // Initialize the condition variable for buffer 1

pthread_mutex_t mutex_2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_2 = PTHREAD_COND_INITIALIZER;

pthread_mutex_t mutex_3 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_3 = PTHREAD_COND_INITIALIZER;


//consume stdin, produce buffer 1
void *get_input(void* args) {
    int stop_input = 0;             //can set to 1 to stop collecting input
        char from_input[1000] = {'\0'}; //creating a blank char array to copy over to buffer 1
        int output_line = 0;
        char *stop_sign = "STOP\n";
    int stop_entire_program = 0; //terminate the entire program if found
    
         while (!stop_input && !stop_entire_program) {
            //continuously collect input until: >49 lines, "STOP\n"
                fgets(from_input, MAX_LINE, stdin);

                //checking conditions to stop collecting input
                if (output_line > 49) {
                    stop_input = 1;
                    stop_entire_program = 1;
                }
                //search for "STOP\n" and cease collecting input from stop on
                //exclude 'STOP!'', A'LLSTOP\n', 'ISTOP', 'stop', 'Stop'
                //using strcmp() instead of strstr() since 'STOP\n' will be on its own line
                if (strcmp(from_input, stop_sign) == 0) {
                    stop_input = 1;
                    memset(from_input, '\x03', sizeof(from_input));
                    stop_entire_program = 1;
                }

                put_buff_1(buffer_1, output_line, from_input);

                output_line += 1;
        }
    return NULL;
}

//consume buffer 1, produce buffer 2
void *line_separator(void *args) {
    char input[MAX_LINE] = {'\0'};
    int output_line = 0;
    int stop_entire_program = 0; //terminate the entire program if found
    while(!stop_entire_program) {
        get_buff_1(buffer_1, output_line, input); //input is ‘char *’
        str_gsub(input, "\n", " "); // convert "\n" to " "
        if (strstr(input, ALLSTOP) != NULL) {
            stop_entire_program = 1;
        }
        put_buff_2(buffer_2, output_line, input);
        output_line += 1;
    }
        return NULL;
}

//consume buffer 2, produce buffer 3
void *plus_replace(void *args) {
    char input[MAX_LINE] = {'\0'};
    int output_line = 0;    
    int stop_entire_program =0;

    while(!stop_entire_program) {
        get_buff_2(buffer_2, output_line, input);
        if (strstr(input, ALLSTOP) != NULL) {
            stop_entire_program = 1;
        }
        str_gsub(input, "++", "^"); //exchange ++ for ^
        put_buff_3(buffer_3, output_line, input);
        output_line += 1;
    }
        return NULL;
}

//consume buffer3, produce stdin
void *write_output(void* args) {
    char input[MAX_LINE] = {'\0'};
    int output_line = 0;
    int stop_entire_program = 0;
    char forming_line[LINE_COUNT * MAX_LINE] = {'\0'};
    char line_to_print[MAX_CHAR+1] = {'\0'}; // <(80+1) max lines 
    
    int i, k = 0;
    
    while(!stop_entire_program) {
        get_buff_3(buffer_3, output_line, input);
        strcat(forming_line, input);        //combines forming_line with current input, "\0" if stopped
        
        if (strstr(input, ALLSTOP) != NULL) {
            stop_entire_program = 1;
            break;
        }
    
        while ( k < (strlen(forming_line))){ //print the values until reaching 81, then reset
            line_to_print[i] = forming_line[k];
            i++;
            k++;
            if (i == MAX_CHAR) {
                printf("%s\n", line_to_print);
                fflush(stdout);
                memset(line_to_print, '\0', MAX_CHAR);
                i = 0;
            }
        }
        output_line += 1;
    } 
    return NULL;
}

//code adapted from https://www.youtube.com/watch?v=-3ty5W_6-IQ posted by Ryan Gambord
void str_gsub(char* haystack, const char* needle, char* sub){
	const char *stringo = haystack;
    size_t len_string = strlen(needle);
    size_t lenReplacement = strlen(sub);
    char temp[MAX_LINE] = { 0 };
	char *pointer = &temp[0];

	
    while (1) {
        const char *ind = strstr(stringo, needle);
        if (ind == NULL) {
            strcpy(pointer, stringo);
            break;
        }
        memcpy(pointer, stringo, ind - stringo);
        pointer += ind - stringo;
        memcpy(pointer, sub, lenReplacement);
        pointer += lenReplacement;
        stringo = ind + len_string;
    }
    strcpy(haystack, temp);
}

// buffer 1 consumes from get_input(), produces for line_separator()
void put_buff_1(struct buffer* buffer, int line, char input[]) {
  pthread_mutex_lock(&mutex_1);        // Lock the mutex before putting the item in the buffer
  strcpy(buffer->buffer[line], input);
  pthread_cond_signal(&full_1);        // Signal to the consumer that the buffer is no longer empty
  pthread_mutex_unlock(&mutex_1);
}
// Get the next item from buffer 1
void get_buff_1(struct buffer* buffer, int line, char output[]) {
    ssize_t line_length = strlen(buffer->buffer[line]);
    pthread_mutex_lock(&mutex_1); // Lock the mutex before checking if the buffer has data
        while (line_length == 0) {    // Buffer is empty. Wait for the producer to signal that the buffer has data
                pthread_cond_wait(&full_1, &mutex_1);
                line_length = strlen(buffer->buffer[line]);
        }
        strcpy(output, buffer->buffer[line]);
        pthread_mutex_unlock(&mutex_1); 

}

//put and get methods for buffer 2
//buffer 2 consumes from line_separator() and produces for plus_replace
void put_buff_2(struct buffer* buffer, int line, char input[]) {
  pthread_mutex_lock(&mutex_2);
  strcpy(buffer->buffer[line], input); 
  pthread_cond_signal(&full_2);
  pthread_mutex_unlock(&mutex_2);
}
void get_buff_2(struct buffer* buffer, int line, char output[]) {
    ssize_t line_length = strlen(buffer->buffer[line]);
    pthread_mutex_lock(&mutex_2);
        while (line_length == 0) {
                pthread_cond_wait(&full_2, &mutex_2);
                line_length = strlen(buffer->buffer[line]);
        }
        strcpy(output, buffer->buffer[line]);
        pthread_mutex_unlock(&mutex_2);
}

//put and get methods for buffer 3
//buffer 3 consumes from plus_replace() and produces for stdin
void put_buff_3(struct buffer* buffer, int line, char input[]) {
  pthread_mutex_lock(&mutex_3);
  strcpy(buffer->buffer[line], input); 
  pthread_cond_signal(&full_3);
  pthread_mutex_unlock(&mutex_3);
}
void get_buff_3(struct buffer* buffer, int line, char output[]) {
    ssize_t line_length = strlen(buffer->buffer[line]);
    pthread_mutex_lock(&mutex_3);
        while (line_length == 0) {
                pthread_cond_wait(&full_3, &mutex_3);
                line_length = strlen(buffer->buffer[line]);
        }
        strcpy(output, buffer->buffer[line]);
        pthread_mutex_unlock(&mutex_3);
}

///////////////////////////////////////////////////////////////////////////////////////

int main(void) {
    buffer_1 = calloc(1, sizeof(struct buffer));
    buffer_2 = calloc(1, sizeof(struct buffer));
    buffer_3 = calloc(1, sizeof(struct buffer));
    
    pthread_t input_t, line_separator_t, plus_t, output_t;
    
    // Create the threads
    pthread_create(&input_t, NULL, get_input, NULL);
    pthread_create(&line_separator_t, NULL, line_separator, NULL);
    pthread_create(&plus_t, NULL, plus_replace, NULL);
    pthread_create(&output_t, NULL, write_output, NULL);

    // Wait for the threads to terminate
    pthread_join(input_t, NULL);
    pthread_join(line_separator_t, NULL);
    pthread_join(plus_t, NULL);
    pthread_join(output_t, NULL);

    return EXIT_SUCCESS;
}

