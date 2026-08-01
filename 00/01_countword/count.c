#include <stdio.h>

#define OUT 0
#define IN 1
#define INIT OUT

int count_word(char *filename) {
    int status = INIT;
    int word = 0;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return -1;
    }
    char c;
    while ((c = fgetc(fp)) != EOF) {
        if (c == ' ' || c == '\n' || c == '\t') {
            status = OUT;
        } else if (status == OUT) {
            status = IN;
            word ++;
        }
    }
    return word; 

}

/*
main函数的标准写法，用于接收命令行参数
argc - Argument Count（参数计数）：命令行参数的数量，至少为1（因为程序名本身算一个参数）。
argv - Argument Vector（参数向量）：一个字符串数组，包含了所有的命令行参数。argv[0]通常是程序的名称，argv[1]是第一个参数，以此类推。
*/
int main(int argc, char *argv[]) {
    if(argc < 2) {
        return -1;
    }
    printf("Number of words: %d\n", count_word(argv[1]));
}