#include <stdio.h>
#include <string.h>

#define OUT 0
#define IN 1
#define INIT OUT

#define MAX_WORD_LEN 24 // 单个单词的最大长度
#define MAX_WORDS 1024  // 单词种类最大数量

// 统计单词频次的结构体
struct WordEntry {
    char word[MAX_WORD_LEN];
    int count;
};
/*
也可以使用
两种用法完全等价，但是typedef的用法不支持链表中的自引用
typedef struct {
    char word[MAX_WORD_LEN];
    int count;
}WordEntry;
*/
struct WordEntry dict[MAX_WORDS]; // 单词字典
int dict_size = 0;


// 1. 查找单词在字典中是否存在
int find_word(const char *word) {
    int i = 0;
    for (i=0; i<dict_size; i++) {
        if (strcmp(dict[i].word, word) == 0) {
            return i;
        }
    }
    return -1;
}

// 2. 添加单词到统计表
void add_word(const char *word) {
    int idx = find_word(word);

    if (idx != -1) {
        dict[idx].count++;
    } else {
        strcpy(dict[dict_size].word, word);
        dict[dict_size].count = 1;
        dict_size++; 
    }
    
}

// 3. 打印统计结果
int print_dict(void) {
    int i = 0;
    printf("\n======= Word Count ========\n");
    for (i=0; i<dict_size; i++) {
        printf("%s : %d\n", dict[i].word, dict[i].count);
    }
    printf("Total unique words: %d\n", dict_size);
}



int count_word(char *filename) {
    int status = INIT;
    char word[MAX_WORD_LEN];
    int pos = 0;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return -1;
    }

    int c;
    while ((c = fgetc(fp)) != EOF) {
        // 判断是否是字母，实质上这里的status状态已经没用了
        if (isalpha(c)) {
            // 拼接字母到当前单词 (统一转为小写)
            if (pos < MAX_WORD_LEN -1) {
                word[pos++] = tolower(c);
            }
            status = IN;
        } else {
            // 遇到非字母则结束当前单词
            if (status == IN && pos > 0){
                word[pos] = '\0';
                add_word(word);
                pos = 0;
                status = OUT;
            }
        }
    }

    // 处理最后一个单词 （如果文件末尾没有分隔符）
    if (status == IN && pos > 0) {
        word[pos] = '\0';
        add_word(word);
    }
    fclose(fp);
    return 0; 

}

/*
main函数的标准写法，用于接收命令行参数
argc - Argument Count（参数计数）：命令行参数的数量，至少为1（因为程序名本身算一个参数）。
argv - Argument Vector（参数向量）：一个字符串数组，包含了所有的命令行参数。argv[0]通常是程序的名称，argv[1]是第一个参数，以此类推。
*/
int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return -1;
    }
    count_word(argv[1]);
    print_dict();

    return 0;
}