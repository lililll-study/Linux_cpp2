#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NAME_LENGTH 16
#define PHONE_NUMBER_LENGTH 32
// 为什么要定义宏，因为上线的时候只要把info置为空，就可以去掉所有的打印语句，减少性能损失
#define INFO        printf

// entity 底层数据层层 ===
// 定义宏LIST_INSERT，用于将一个联系人插入到链表的头部
// item表示要插入的联系人，list表示链表的头指针
// 每次插入一个person后，都会把这个item插入到链表的头部。然后更新list指针，使其指向新的头部。
#define LIST_INSERT(item, list) do{ \
    item->previous = NULL; \
    item->next = list; \
    list = item; \
}while(0)

// 定义宏LIST_REMOVE，用于从链表中删除一个联系人
// item表示要删除的联系人，list表示链表的头指针
#define LIST_REMOVE(item, list) do{ \
    if(item->previous) { item->previous->next = item->next; } \
    if(item->next) { item->next->previous = item->previous; } \
    if(list == item) { list = item->next; } \
    item->previous = item->next = NULL; \
}while(0)



// 定义联系人结构体
// 包含姓名、电话号码、以及指向下一个和上一个联系人的指针
struct person
{
    char name[NAME_LENGTH];
    char phone_number[PHONE_NUMBER_LENGTH];

    struct person *next;
    struct person *previous;

};

// 定义结构体
// *people是一个链表，count是联系人数量
struct contacts {
    struct person *people;
    int count;
};

enum {
    OPER_INSERT = 1,
    OPER_DELETE,
    OPER_SEARCH,
    OPER_PRINT,
    OPER_SAVE,
    OPER_LOAD
};
// end===



// interface 接口层 ===
// *people是一个链表，*person是一个要插入/删除/搜索的联系人
// 和底层的链表进行一个隔离，提供一个接口来操作联系人列表
// 由于直接修改原来的形参(struct person *people)会导致调用者无法获取到修改后的结果，所以需要使用指针的指针来传递联系人列表的地址，以便在函数内部修改它。
int person_insert(struct person **ppeople, struct person *person){
    // 如果person为NULL，说明没有要插入的联系人，返回-1表示插入失败
    if( person == NULL) return -1;
    LIST_INSERT(person, *ppeople);
    return 0;
}

int person_delete(struct person **ppeople, struct person *person){
    // 如果person为NULL，说明没有要删除的联系人，返回-1表示删除失败
    if (person == NULL) return -1;
    LIST_REMOVE(person, *ppeople);
    // 删除成功，返回0
    return 0;

}

struct person* person_search(struct person *people, const char *name){
    struct person *item = NULL;
    for (item = people; item != NULL; item = item->next) {
        // 如果相等则strcmp=0，说明找到了要搜索的联系人，跳出循环
        if (!strcmp(item->name, name))
            break;
    }
    return item;
}

int person_traversal(struct person *people){
    struct person *item = NULL;
    for (item = people; item != NULL; item = item->next) {
        // 打印联系人姓名和电话号码
        // 不建议使用printf的方式实现，而通过定义宏的方式实现
        INFO("name: %s, phone: %s\n", item->name, item->phone_number);
    }
    return 0;
}
// end ===

// 业务层 ===
int insert_entry(struct contacts *cts){
    if (cts == NULL) return -1;
    struct person *p = (struct person*)malloc(sizeof(struct person));
    if (p == NULL) return -2;

    INFO("input name: \n");
    scanf("%s", p->name); // 当scanf超过16位时，会造成溢出（todo :待解决的问题，如何解决scanf数组溢出的问题）
    INFO("input phone number: \n");
    scanf("%s", p->phone_number); // 当scanf超过32位时，会造成
    // 插入联系人到联系人列表中,需要传递的是两个地址，第一个实参需要people这个指针指向的地址改了，故第一个实参需要传入指针的地址
    if (0 != person_insert(&cts->people, p)){
        free(p);
        return -3;
    }
    cts->count++;
    INFO("insert success\n");
    return 0;

}

int print_entry(struct contacts *cts){
    // cts->people
    if (cts == NULL) return -1;
    person_traversal(cts->people);
    return 0;
}

int delete_entry(struct contacts *cts){
    if (cts == NULL) return -1;

    INFO("input name: \n");
    char name[NAME_LENGTH] = {0};
    scanf("%s", name);

    struct person *ps = person_search(cts->people, name);
    if (ps == NULL) {
        INFO("delete failed, not found\n");
        return -2;
    }

    person_delete(&cts->people, ps);
    free(ps);
    // cts->count--;
    INFO("delete success\n");
    return 0;
}

int search_entry(struct contacts *cts){
    if (cts == NULL) return -1;

    INFO("input name: \n");
    char name[NAME_LENGTH] = {0};
    scanf("%s", name);

    struct person *ps = person_search(cts->people, name);
    if (ps == NULL) {
        INFO("search failed, not found\n");
        return -2;
    }
    INFO("search success, name: %s, phone: %s\n", ps->name, ps->phone_number);
    return 0;
}

int save_file(struct person *people, const char *filename){
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return -1;
    }
    struct person *item = NULL;
    for (item = people; item != NULL; item = item->next) {
        fprintf(fp, "name: %s, phone: %s\n", item->name, item->phone_number); // 写入的数据是在缓存中
        fflush(fp); // 刷新缓冲区，把数据写入文件
    }
    fclose(fp);
}

int load_file(struct person **ppeople, const char *filename){
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return -1;
    }
    char name[32];
    char phone[64];

    while (fscanf(fp, "name: %[^,], phone: %s\n", name, phone) == 2) {
        struct person *new_person = (struct person*)malloc(sizeof(struct person));
        if (new_person == NULL){
            fclose(fp);
            return -2;
        }
        // 把数据拷贝到新节点
        strncpy(new_person->name, name, NAME_LENGTH - 1);
        strncpy(new_person->phone_number, phone, PHONE_NUMBER_LENGTH - 1);
        new_person->next = NULL;
        new_person->previous = NULL;
        person_insert(ppeople, new_person);
    }
    INFO("load success\n");
    fclose(fp);      // ← 缺少
    return 0;        // ← 缺少
}

int save_entry(struct contacts *cts){
    if (cts == NULL) return -1;
    INFO("input filename: \n");
    char filename[NAME_LENGTH] = {0};
    scanf("%s", filename);
    save_file(cts->people, filename);

}
int load_entry(struct contacts *cts){
    if (cts == NULL) return -1;
    INFO("input filename: \n");
    char filename[NAME_LENGTH] = {0};
    scanf("%s", filename);
    load_file(&cts->people, filename);
}

// end ===


// 
void menu_info(void){
    INFO("1. insert\n");
    INFO("2. delete\n");
    INFO("3. search\n");
    INFO("4. print\n");
    INFO("5. save\n");
    INFO("6. load\n");
}

int main(){

    struct contacts *cts = malloc(sizeof(struct contacts));
    if (cts == NULL) return -1;
    memset(cts, 0, sizeof(struct contacts));
    // 注意，malloc出来的变量一定要menset为0，否则会有一些垃圾数据，可能会导致程序出错

    while(1){
        menu_info();

        int select = 0;
        scanf("%d", &select);
        switch (select) {
            case OPER_INSERT:
                insert_entry(cts);
                break;
            case OPER_DELETE:
                delete_entry(cts);
                break;
            case OPER_SEARCH:
                search_entry(cts);
                break;
            case OPER_PRINT:
                print_entry(cts);
                break;
            case OPER_SAVE:
                save_entry(cts);
                break;
            case OPER_LOAD:
                load_entry(cts);
                break;
        }
    }
}