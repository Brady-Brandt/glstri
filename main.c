#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>




typedef struct {
    const char* name;
    FILE* stream; 
    bool isOpen;
} File;


//holds a glsl or C line with unnecessary white space removed
typedef struct {
   int size;
   char* data;
   int capacity;
   bool endOfFile;
} Line;


typedef enum {
    VARIABLE = 0,
    WARNINGS = 1,
    SILENCE = 2,
    FLAGS_LEN = 3,
} FlagType;

typedef struct{
    bool isEnabled;
    char* arg;
} Flag;


typedef struct {
    Flag f[FLAGS_LEN];
} Flags;


//think this is better than having to pass around flags to every function 
static Flags flags = {0};

#define is_flag_set(flag) (flags.f[flag].isEnabled)
#define get_flag_arg(flag) (flags.f[flag].arg)
#define enable_flag(flag) (flags.f[flag] = (Flag){true, NULL})


#define print_va(fmt, ...) if(!is_flag_set(SILENCE)) printf(fmt,__VA_ARGS__)
#define print(str) if(!is_flag_set(SILENCE)) printf(str)


#define warning(fmt, ...) if(is_flag_set(WARNINGS)) printf("WARNING: " fmt, __VA_ARGS__)

typedef struct {
    Flag* array[FLAGS_LEN];
    int front;
    int rear;
} FlagQueue;




//verifies that the output file is a valid C or header file
void verify_ouput_file(const char* output_file){
    int output_len = strlen(output_file);
    char dot = output_file[output_len - 2];
    char letter = output_file[output_len - 1];

    if(dot != '.' && (letter != 'c' || letter != 'h')){
        fprintf(stderr,"%s is not a valid C or header file\n", output_file);
        exit(2);
    }
}

//checks if a file exists by attempting to open it in read mode
bool exists(char* file_name){
    FILE* f = fopen(file_name, "r"); 
    if(f == NULL){
        return false;
    }
    fclose(f);
    return true;
}



File open_file(const char* file_name, const char* mode){
    FILE* result = fopen(file_name, mode);
    if(result == NULL){
        fprintf(stderr, "Failed to open %s: ", file_name);
        perror("");
        exit(1);
    }
    File file;
    file.name = file_name;
    file.stream = result;
    file.isOpen = true;
    return file;
}


void close_file(File *f){
    if(f->isOpen){
        fclose(f->stream);
        f->isOpen = false;
    }
}

void new_line(Line* line){
    line->endOfFile = false;
    line->size = 0;
    line->data= malloc(32 * sizeof(char));
    line->capacity = 31;
}

void reset_line(Line* line){
    line->size = 0;
    line->endOfFile = false;
}

void delete_line(Line* line){
    reset_line(line);
    free(line->data);
    line->capacity = 0;
}



void get_line(FILE* stream, Line* line, bool is_formatted){
    if(line->endOfFile) return;
    char prev = 0;

    bool opening_quotes = false;
    bool closing_quotes = false;

    while(true){ 
        char c = fgetc(stream);  
        if(isspace(c)){
            //remove leading whitespace or multiple whitespace between tokens  
            if(line->size == 0 || (c != '\n' && isspace(prev))){
                prev = c;
                continue;
            }
        }
        if(c == '\n' || c == EOF){ 
            //remove trailing whitespace 
            if(isspace(prev)){
                line->data[line->size - 1] = '\0'; 
            }
            if(c == EOF){
                line->endOfFile = true;
            }
            break;
        }
        //realloc if size is going to be greater than the capacity 
        if(line->size == line->capacity - 1){
            line->data = realloc(line->data, line->capacity * 2);
            line->capacity *= 2;
        }

        //looks through data that has already been formatted by this tool 
        if(is_formatted){
            //remove \n from output file 
            if(c == 'n' && prev == '\\'){
                line->size--;
                continue;
            }
            if(c == '"'){
                if(opening_quotes) closing_quotes = true;
                if(line->size == 0) opening_quotes = true;
                continue;
            } 
            //if we find a semi-colon outside of double quotes we know that is the end of the shader code
            if(((opening_quotes && closing_quotes) || (!opening_quotes && !closing_quotes)) && c == ';'){
               line->endOfFile = true;
               break;
            }
        }
        line->data[line->size++] = c;
        prev = c;
    }
    //add null terminator
    line->data[line->size] = 0;
}


//copys the input file to the output file 
//declares the glsl code with the variable variable_name
void write_lines(FILE* input_stream, FILE* output_stream, const char* variable_name){
    fprintf(output_stream, "\nconst char* %s =", variable_name);
    Line line;
    new_line(&line);
    while(true){
        get_line(input_stream, &line, false);
        if(line.size > 0){
            int items = fprintf(output_stream, "\n    \"%s\\n\"", line.data);
        }
        if(line.endOfFile){
            fprintf(output_stream, ";\n");
            break;
        }
        reset_line(&line);
    }
    delete_line(&line);
}




bool remove_comments(Line* line, bool multi_line_comment){
    if(line->size < 2){
        return false;
    }
    char* single_line_comment = strstr(line->data, "//");

    if(single_line_comment != NULL){
       int comment_start = single_line_comment - line->data; 
       line->data[comment_start] = 0;
       line->size = comment_start;
       return false;
    }

    
    if(!multi_line_comment){
        char* multi_line_start = strstr(line->data, "/*");
        if(multi_line_start != NULL){
            int comment_start = multi_line_start - line->data;
            line->data[comment_start] = 0;
            line->size = comment_start;
            multi_line_comment = true;
        }
    }

    //check for end of multi line comment
    if(multi_line_comment){
        char* multi_line_end = strstr(line->data, "*/");
        if(multi_line_end != NULL){
            multi_line_end += 2;
            int line_len = strlen(multi_line_end);
            if(line_len < 1){
                line->data[0] = 0;
                line->size = 0;
            } else {
                memmove(line->data, multi_line_end, line_len);
                line->data[line_len + 1] = 0;
                line->size = line_len;
            }
            multi_line_comment = false;
        } else {
            line->data[0] = 0;
            line->size = 0;
        }

    }

    return multi_line_comment;
}




//returns whether there is a difference between input_file and output 
//at least for the shader code wise
//if temp != null, write the output to the temp file 
bool isDiff(FILE* input_stream, FILE* output_stream, FILE* temp){
        //loop through and compare shader code to see if we have to make changes  
        Line input_line;
        Line output_line;
        new_line(&input_line);
        new_line(&output_line);
        bool first_line = true;

        //whether there is a diff between the two files
        bool result = false;
        bool end_of_input = false;
        bool end_of_output = false;

        bool input_multi_comment = false;
        bool output_multi_comment = false;

        bool is_blank_line = false;
        while(true){
            if(!end_of_input) get_line(input_stream, &input_line, false); 
            if(!end_of_output && !is_blank_line) get_line(output_stream, &output_line, true); 
            

            is_blank_line = false;

            input_multi_comment = remove_comments(&input_line, input_multi_comment);
            output_multi_comment = remove_comments(&output_line, output_multi_comment);

            if(output_line.endOfFile){
                end_of_output = true;
            }

            if(input_line.endOfFile){
                if(!end_of_output){
                    result = true;
                    if(temp == NULL) break;
                    reset_line(&output_line);
                    continue;
                }
                end_of_input = true;
            }

            if(end_of_input && end_of_output) break;

            //skip over blank lines 
            if(input_line.size == 0 && output_line.size == 0){
                continue;
            }

            //skip over blank lines in input file
            if(input_line.size == 0 && output_line.size != 0){
                is_blank_line = true;
                continue;
            }
           
            char* output = NULL;

            //if input line is different from the output 
            //make the output be equal to the input 
            if(output_line.size != input_line.size || strcmp(input_line.data, output_line.data) != 0){
                output = input_line.data;
                result = true;
                if(temp == NULL) break;
            } else {
                output = output_line.data; 
            }    
            if(temp != NULL){
                if(first_line){
                    first_line = false;
                    fprintf(temp, "    \"%s\\n\"", output); 
                } else {
                    fprintf(temp, "\n    \"%s\\n\"", output); 
                }
                //print out the differences between files
                if(end_of_output){
                    print_va(" ---> %s\n", output);
                }
                else if(output != output_line.data){
                    print_va("%s ---> %s\n", output_line.data, output);
                }
            }
            reset_line(&input_line);
            reset_line(&output_line); 
        }

        if(temp != NULL){
            fprintf(temp, ";");
        }

        delete_line(&input_line);
        delete_line(&output_line);
        return result;
}


//writes the input stream to the output stream until the 
void write_until_pos(FILE* input_stream, FILE* output_stream, long pos){
    const int BUFF_SIZE = 512;
    char buffer[BUFF_SIZE];

    int current_buff_size = 0; 
    int current_pos = ftell(input_stream);
    while(current_pos != pos && current_pos != -1){
        char c = fgetc(input_stream);
        buffer[current_buff_size++] = c;
        if(current_buff_size == BUFF_SIZE){
            fwrite(buffer, sizeof(char), current_buff_size, output_stream);
            current_buff_size = 0;
        }
        current_pos = ftell(input_stream);
    }
    //write the remaining stuff in the buffer to a temp file 
    if(current_buff_size != 0){
        fwrite(buffer, sizeof(char), current_buff_size,output_stream);
        current_buff_size = 0;
    }
}




//checks if the variable exists in the file
bool contains_variable(File* output_file, const char* variable){
    Line line;
    new_line(&line);
    bool multi_line_comment = false;

    bool result = false;

    bool before_is_clear;
    while(true){
        before_is_clear = false;            
        get_line(output_file->stream, &line, false);
        if(line.endOfFile) break;
        multi_line_comment = remove_comments(&line, multi_line_comment);
        char* variable_start = strstr(line.data,variable);
        if(variable_start == NULL){
            reset_line(&line);
            continue;
        }
        
        if(variable_start > line.data){
            int variable_index = variable_start - line.data;
            int char_before_var = line.data[variable_index - 1];

            if(char_before_var == ' ' || char_before_var == '*' || char_before_var == '\t'){
                before_is_clear = true;
            }
        }

        if(before_is_clear || variable_start == line.data){
            int var_len = strlen(variable);
            int var_index = variable_start - line.data;
            if(var_len + var_index == line.size){
                result = true;
                break;
            }

            int char_after_var = line.data[var_index + var_len];
            if(char_after_var == ' ' || char_after_var == '=' || char_after_var == '[' || char_after_var == '\t'){
                result = true;
                break;
            }
        }
                               
        reset_line(&line); 
    }
    delete_line(&line);
    return result;
}



void write_to_existing_file(File* input_file, File* output_file, const char* variable_name){ 
    //create a temp file to store the current files data
    FILE* temp = tmpfile();
    if(temp == NULL){
        perror("Failed to create a temp file\n");
        exit(1);
    }
    if(contains_variable(output_file, variable_name)){
        //output file start at the first line of the shader code 
        long shader_code_pos = ftell(output_file->stream);
        if(!isDiff(input_file->stream, output_file->stream, NULL)){
            print("Found no difference between shader code!");
            return;
        }


        //copying all the code before the shader code to the temp file 
        rewind(output_file->stream);
        write_until_pos(output_file->stream, temp, shader_code_pos);


        warning("Changing the definition of %s to the contents of %s\n", variable_name, input_file->name);
        
        //write the shader code
        rewind(input_file->stream);
        isDiff(input_file->stream, output_file->stream, temp);
    } else { 
        //the shader code doesn't exist in the output file 
        //so we want to try to add it after the headers
        rewind(output_file->stream);
        Line current_line;
        new_line(&current_line);

        //trying to find a good stop for the shader code 
        long shader_code_pos = ftell(output_file->stream);
        bool inside_comment = false;
        while(true){
            shader_code_pos = ftell(output_file->stream);
            get_line(output_file->stream, &current_line, false);
            //don't want to place it inside a multi line comment on accident
            if(strstr(current_line.data, "/*") != NULL){
                inside_comment = true;
            } 
            if(inside_comment && strstr(current_line.data, "*/") != NULL){
                inside_comment = false;
            }

            if(current_line.endOfFile) break;
            if(!inside_comment && current_line.size != 0 && isalpha(current_line.data[0])){
                break;
            }     
            reset_line(&current_line);
        }

        delete_line(&current_line);

        rewind(output_file->stream);
        
        write_until_pos(output_file->stream, temp, shader_code_pos);
       
        //copy shader code to temp file 
        write_lines(input_file->stream, temp, variable_name);
    }

    const int BUFF_SIZE = 512;
    char buffer[BUFF_SIZE];
    //writes all the stuff after the shader to a temp file 
    while(fgets(buffer, BUFF_SIZE, output_file->stream) != NULL){ 
        fwrite(buffer, sizeof(char), strlen(buffer), temp);
    }

    close_file(output_file);
    File write_output_file = open_file(output_file->name, "w");
    //copy all the data from the temp file to the new file 
    rewind(temp);
    while(fgets(buffer, BUFF_SIZE, temp) != NULL){
        fwrite(buffer, sizeof(char), strlen(buffer), write_output_file.stream);
    }
    close_file(&write_output_file);  
}



// Function to create an empty queue
void create_queue(FlagQueue* queue) {
    queue->front = -1;
    queue->rear = -1;
}

// Function to check if the queue is full
bool is_full(FlagQueue* queue) {
    return (queue->rear == FLAGS_LEN - 1);
}

// Function to check if the queue is empty
bool is_empty(FlagQueue* queue) {
    return (queue->front == -1 && queue->rear == -1);
}

// Function to enqueue an element to the queue
void enqueue(FlagQueue* queue, Flag* data) {
    if (is_full(queue)) {
        printf("Queue is full. Cannot enqueue.\n");
        return;
    }

    if (is_empty(queue))
        queue->front = queue->rear = 0;
    else
        queue->rear++;

    queue->array[queue->rear] = data;
}

// Function to dequeue an element from the queue
Flag* dequeue(FlagQueue* queue) {
    if (is_empty(queue)) {
        printf("Queue is empty. Cannot dequeue.\n");
        return NULL;
    }
    Flag* data = queue->array[queue->front];
    if (queue->front == queue->rear)
        queue->front = queue->rear = -1;
    else
        queue->front++;

    return data;
}


void help(){
    printf("Enter flags and any flag arguements, along with two files\n");
    printf("Current Possible flags include: \n");
    printf("-s ---> Silences all output except warnings if they are enabled\n");
    printf("-v ---> Takes a variable name that will be the name of a variable in the output file\n");
    printf("-w ---> Enables warnings\n");
    printf("*Note flags are not case sensitive\n");
    exit(0);
}


void get_flags(int argc, char**argv){ 
    FlagQueue arg_queue;
    create_queue(&arg_queue);

    //last 2 args should be the files 
    for(int i = 1; i < argc - 2; i++){
        char* item = argv[i];
        int opt_len = strlen(item);

        if(strcmp(item, "--help") == 0){
            help();
        }

        //flags start with - 
        if(item[0] != '-'){
            //if it doesn't start with dash check if it is an Flag arguement 
            if(!is_empty(&arg_queue)){
                Flag* current_opt = dequeue(&arg_queue);
                current_opt->arg = item;
                continue;
            } else {
                fprintf(stderr, "Invalid flag type: %s\n", item);
                exit(4);
            }
        }  
        for(int j = 1; j < opt_len; j++){
            char c = item[j];
            switch (tolower(c)) {
                case 'v':
                    enable_flag(VARIABLE);
                    enqueue(&arg_queue, &flags.f[VARIABLE]);
                    break;

                case 'w':
                    enable_flag(WARNINGS);
                    break;

                case 's':
                    enable_flag(SILENCE);
                    break;

                default:
                    fprintf(stderr, "Invalid flag: %c\n", c);
                    exit(4);
            }
        }
    }

    if(!is_empty(&arg_queue)){
        fprintf(stderr, "Not enough arguements entered\n");
        exit(4);
    }
}



int main(int argc, char** argv){
    if(argc < 3){
        if(argc == 2 && strcmp(argv[1], "--help") == 0){
           help(); 
        }
        printf("Error: Invalid arguement count\n");
        printf("Default Usage: ./glstri input_file output_file\n");
        return 4;
    }

    char *input_file_name, *output_file_name;

    //default you just enter two files
    if(argc == 3){
        input_file_name = argv[1];
        output_file_name = argv[2];
    } else {
        get_flags(argc, argv);
        input_file_name = argv[argc - 2];
        output_file_name = argv[argc - 1];
    }

    print_va("%s -> %s\n", input_file_name, output_file_name);
    File input_file = open_file(input_file_name,"r");

    verify_ouput_file(output_file_name); 
    
    bool output_exists = exists(output_file_name);

    File output_file;
    if(output_exists){
        output_file = open_file(output_file_name, "r+");
    } else{
        output_file = open_file(output_file_name, "w");
    }
    
    //the name of the input file is going to be the name of the variable that stores the data
    int input_len = strlen(input_file_name);

    char* variable_name = NULL;

    if(is_flag_set(VARIABLE)){
        variable_name = get_flag_arg(VARIABLE);
    } else {
        //remove the file extension if there is one
        for(int i = input_len - 1; i > 0; i--){
            if(input_file_name[i] == '.'){
                input_file_name[i] = '\0';
                break;
            }
        }
        variable_name = input_file_name;
    }


    if(output_exists){
        write_to_existing_file(&input_file, &output_file, variable_name);
    } else{
        //write the declaration to the file
        write_lines(input_file.stream, output_file.stream, variable_name);
    }
    
    close_file(&input_file);
    close_file(&output_file);

    return 0;
}
