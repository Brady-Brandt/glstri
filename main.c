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

//holds a glsl or C line with unnecessary white space removed
typedef struct {
   int size;
   char* data;
   int capacity;
   bool endOfFile;
} Line;


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
    fprintf(output_stream, "\nconst char* %s=", variable_name);
    Line line;
    new_line(&line);
    while(true){
        get_line(input_stream, &line, false);
        if(line.size > 0){
            printf("%s\n", line.data);
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
        while(true){
            if(!end_of_input) get_line(input_stream, &input_line, false); 
            if(!end_of_output) get_line(output_stream, &output_line, true); 

            if(input_line.endOfFile){
                end_of_input = true;
            }

            if(output_line.endOfFile){
                end_of_output = true;
            }

            if(end_of_input && end_of_output) break;

            //skip over blank lines 
            if(input_line.size == 0 && output_line.size == 0){
                continue;
            }

            //skip over blank lines in input file
            if(input_line.size == 0 && output_line.size != 0){
                result = true;
                if(temp == NULL) break;
                reset_line(&output_line);
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



void write_to_existing_file(File* input_file, File* output_file, const char* variable_name){
    const int BUFF_SIZE = 512;
    char buffer[BUFF_SIZE];

    bool found_occurrence = false;

    //check if variable_name already exists in the output file
    while(fgets(buffer, BUFF_SIZE, output_file->stream) != NULL){
        if(strstr(buffer, variable_name) != NULL){
            found_occurrence = true;
            break;
        }
    }


    //create a temp file to store the current files data
    FILE* temp = tmpfile();
    if(temp == NULL){
        perror("Failed to create a temp file\n");
        exit(1);
    }
    if(found_occurrence){
        //output file start at the first line of the shader code 
        long shader_code_pos = ftell(output_file->stream);
        if(!isDiff(input_file->stream, output_file->stream, NULL)){
            return;
        }
        //copying all the code before the shader code to the temp file 
        write_until_pos(output_file->stream, temp, shader_code_pos);

        //write the shader code
        rewind(input_file->stream);
        isDiff(input_file->stream, output_file->stream, temp);
    } else { 
        //the shader code doesn't exist in the output file 
        //so we want to try to add it after the headers
        rewind(output_file->stream);
        Line current_line;
        new_line(&current_line);

        int line_len = 0;
        //trying to find a good stop for the shader code 

        long shader_code_pos = ftell(output_file->stream);
        while(true){
            shader_code_pos = ftell(output_file->stream);
            get_line(output_file->stream, &current_line, false);
            if(current_line.endOfFile) break;
            if(current_line.size != 0 && isalpha(current_line.data[0])){
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



int main(int argc, char** argv){
    if(argc < 3){
        printf("Error: Invalid arguement count\n");
        return 1;
    }
 
    char* input_file_name = argv[1];
    char* output_file_name = argv[2];

    printf("%s -> %s\n", input_file_name, output_file_name);
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

    //remove the file extension if there is one
    int end = input_len;
    for(int i = input_len - 1; i > 0; i--){
        if(input_file_name[i] == '.'){
            input_file_name[i] = '\0';
            break;
        }
    }

    char* variable_name = input_file_name;

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
