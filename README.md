# Overview

glstri is a command line interface that converts and embeds a file of glsl code into a C file. The default use case takes
two parameters, an input file (glsl code) and a output file (.c or .h file). If the output file doesn't exist, the glstri will create it. Then glstri will
take the contents of the input file and convert it into a const char* variable_name, where variable_name defaults to the name of the input file. If the output file does
exist, glstri will check if variable_name alreay exists. If it does, it will compare the contents of the glsl code, with the the glsl code already in the file and make changes
accordingly. If the variable does not exist, glstri just adds it to the file. 

## Install
Clone the repo 
```shell
git clone https://github.com/Brady-Brandt/glstri
```
Just run make if you are on Linux/Macos
```shell
make
```
For other operating systems, the project is just one file so just run
```shell
compiler_name main.c -o glstri 
```

## Usage
Input file vertex.glsl
```glsl
#version 330 core
layout (location = 0) in vec3 aPos; // Input vertex position
void main()
{
    gl_Position = vec4(aPos, 1.0); // Set the output position of the vertex
}

```
Output file render.c (Doesn't exist)
```shell
./glstri vertex.glsl render.c
```
Output render.c
```c
const char* vertex =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos; \n"
    "void main()\n"
    "{\n"
    "gl_Position = vec4(aPos, 1.0); \n"
    "}\n";
```

Make changes to vertex.glsl
```glsl
#version 330 core

layout (location = 0) in vec3 aPos; // Input vertex position
layout (location = 1) in vec2 aTexCoord; // Input texture coordinates
out vec2 TexCoord; // Output texture coordinates
uniform mat4 model; // Uniform model matrix

void main()
{
    gl_Position = model * vec4(aPos, 1.0); // Transform vertex position by the model matrix
    TexCoord = aTexCoord; // Pass texture coordinates to fragment shader
}
```
render.c

```c
#include <stdio.h>

const char* vertex =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos; \n"
    "void main()\n"
    "{\n"
    "gl_Position = vec4(aPos, 1.0); \n"
    "}\n";

int main(){
    printf("Hello World\n");
    return 0;
}
```
Run it again
```shell
./glstri vertex.glsl render.c
```

```
Console Output
```shell
vertex.glsl -> render.c
void main() ---> layout (location = 1) in vec2 aTexCoord; 
{ ---> out vec2 TexCoord; 
gl_Position = vec4(aPos, 1.0);  ---> uniform mat4 model; 
 ---> void main()
 ---> {
 ---> gl_Position = model * vec4(aPos, 1.0); 
 ---> TexCoord = aTexCoord; 
 ---> }
```

render.c

```c
#include <stdio.h>

const char* vertex =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos; \n"
    "layout (location = 1) in vec2 aTexCoord; \n"
    "out vec2 TexCoord; \n"
    "uniform mat4 model; \n"
    "void main()\n"
    "{\n"
    "gl_Position = model * vec4(aPos, 1.0); \n"
    "TexCoord = aTexCoord; \n"
    "}\n";

int main(){
    printf("Hello World\n");
    return 0;
}
```
### Flags
There are currently three flags. (Flags are not case sensitive)
  1. -v var_name ---> Uses the inputted var_name as the variable instead of the input file name
  2. -s          ---> Silences a output from stdout except warnings
  3. -w          ---> Enables warnings

Example  
```shell
./glstri -vsw frag vertex.glsl render.c
```
No console output for this command since silenced output and even though warnings are disabled, we aren't overwriting anything

render.c:
```c
#include <stdio.h>

const char* frag =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos; \n"
    "layout (location = 1) in vec2 aTexCoord; \n"
    "out vec2 TexCoord; \n"
    "uniform mat4 model; \n"
    "void main()\n"
    "{\n"
    "gl_Position = model * vec4(aPos, 1.0); \n"
    "TexCoord = aTexCoord; \n"
    "}\n";

const char* vertex =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos; \n"
    "layout (location = 1) in vec2 aTexCoord; \n"
    "out vec2 TexCoord; \n"
    "uniform mat4 model; \n"
    "void main()\n"
    "{\n"
    "gl_Position = model * vec4(aPos, 1.0); \n"
    "TexCoord = aTexCoord; \n"
    "}\n";

int main(){
    printf("Hello World\n");
    return 0;
}
```
