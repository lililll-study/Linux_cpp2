#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NAME_LENGTH 16
#define PHONE_NUMBER_LENGTH 32
#define ALPHABET_COUNT 26
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
// *group是一个数组，存储通讯录的26个字母，每个字母对应一个通讯录链表
// count是联系人数量
struct contacts {
    struct person *groups[ALPHABET_COUNT];
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

// 获取姓名首字母对应的索引
int get_name_index(const char *name) {
    if (name == NULL || name[0] == '\0') return -1;
    char first_char = toupper(name[0]);
    if (first_char >= 'A' && first_char <= 'Z') {
        return first_char - 'A';
    }
    // 姓名首不是字母，报错
    INFO("Error: Name is not a word");
    return -1;
}

// // 获取字母对应的组名
// char get_group_letter(int index) {
//     if (index >= 0 && index < ALPHABET_COUNT) {
//         return 'A' + index;
//     }
//     return '?';
// }


// interface 接口层 ===
// 从插入到单一的链表，变成计算索引后插入对应的组中
int person_insert(struct person **group_head, struct person *person){
    // 如果person为NULL，说明没有要插入的联系人，返回-1表示插入失败
    if( person == NULL) return -1;
    // 如果链表为空，直接插入
    if (*group_head == NULL) {
        person->next = NULL;
        person->previous = NULL;
        *group_head = person;
        return 0;
    }
    // 按姓名顺序插入（保持组内有序）
    struct person *current = *group_head;
    struct person *prev = NULL;

    // 找到插入位置（按姓名升序排列）
    while (current != NULL && strcmp(person->name, current->name) >0) {
        prev = current;
        current = current->next;
    }

    // 插入到合适位置
    person->next = current; 
    person->previous = prev;    

    if (prev) prev->next = person; // 插入到中间或尾部
    else *group_head = person;  // 插入到头部

    if (current) current->previous = person;    // 更新后继节点的前驱指针
    return 0;
}

// 计算索引后在对应组删除
int person_delete(struct person **group_head, struct person *person){
    // 如果person为NULL，说明没有要删除的联系人，返回-1表示删除失败
    if (person == NULL) return -1;
    LIST_REMOVE(person, *group_head);
    // 删除成功，返回0
    return 0;
}
// 在单个组中搜索
struct person* person_search_in_group(struct person *group_head, const char *name){
    struct person *item = NULL;
    for (item = group_head; item != NULL; item = item->next) {
        // 如果相等则strcmp=0，说明找到了要搜索的联系人，跳出循环
        if (!strcmp(item->name, name))
            break;
    }
    return item;
}

// 全局搜索（入口函数）
// 利用首字母索引直接定位到对应的组。
struct person* person_search_global(struct contacts *cts, const char *name) {
    if (cts == NULL || name == NULL) return NULL;

    int index = get_name_index(name);
    if (index < 0 || index >= ALPHABET_COUNT) {
        return NULL;
    }
    // 只搜索对应的组
    return person_search_in_group(cts->groups[index], name);
}


// 遍历所有组
int person_traversal(struct contacts *cts){
    if (cts == NULL) return -1;
    
    int total=0, i = 0;  // 统计总人数
    for (i = 0; i < ALPHABET_COUNT; i++) {
        if (cts->groups[i] != NULL) {
            INFO("=== Group %c ===\n", 'A' + i);
            // 遍历当前组的链表
            struct person *item = cts->groups[i];
            while (item != NULL) {
                // 打印联系人信息
                INFO("name: %s, phone: %s\n", item->name, item->phone_number);
                item = item->next;
                total++;
            }
        }
    }
    // 打印总人数
    INFO("Total: %d contacts\n", total);
    return 0;
}


// 遍历指定组
int person_traversal_group(struct person *group_head, char group_letter) {
    if (group_head == NULL) {
        INFO("Group %c is empty\n", group_letter);
        return 0;
    }
    
    INFO("=== Group %c ===\n", group_letter);
    struct person *item = group_head;
    while (item != NULL) {
        INFO("  name: %s, phone: %s\n", item->name, item->phone_number);
        item = item->next;
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
    scanf("%s", p->phone_number); 

    // 计算首字母索引
    int index = get_name_index(p->name);
    if (index <0 || index >= ALPHABET_COUNT) {
        free(p);
        return -3;
    }

    // 插入联系人到联系人列表中,需要传递的是两个地址，第一个实参需要people这个指针指向的地址改了，故第一个实参需要传入指针的地址
    if (0 != person_insert(&cts->groups[index], p)){
        free(p);
        return -3;
    }
    cts->count++;
    INFO("insert success (Group %c)\n", 'A' + index);
    return 0;

}

int print_entry(struct contacts *cts){
    // cts->people
    if (cts == NULL) return -1;
    return person_traversal(cts);  // 修改：传入cts而不是people
    return 0;
}

int delete_entry(struct contacts *cts){
    if (cts == NULL) return -1;

    INFO("input name: \n");
    char name[NAME_LENGTH] = {0};
    scanf("%s", name);

    // 先计算索引，在对应的组中搜索
    int index = get_name_index(name);
    if (index < 0 || index >= ALPHABET_COUNT) {
        INFO("delete failed, invalid name\n");
        return -2;
    }

    struct person *ps = person_search_in_group(cts->groups[index], name);
    if (ps == NULL) {
        INFO("delete failed, not found\n");
        return -2;
    }

    person_delete(&cts->groups[index], ps);
    free(ps);
    cts->count--;
    INFO("delete success\n");
    return 0;
}

int search_entry(struct contacts *cts){
    if (cts == NULL) return -1;

    INFO("input name: \n");
    char name[NAME_LENGTH] = {0};
    scanf("%s", name);

    struct person *ps = person_search_global(cts, name);
    if (ps == NULL) {
        INFO("search failed, not found\n");
        return -2;
    }
    INFO("search success, name: %s, phone: %s\n", ps->name, ps->phone_number);
    return 0;
}
// 遍历所有组保存
int save_file(struct person **groups, const char *filename){
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return -1;
    }
    int count=0, i = 0;
    for (i = 0; i < ALPHABET_COUNT; i++) {
        struct person *item = groups[i];
        while (item != NULL) {
            fprintf(fp, "name: %s, phone: %s\n", item->name, item->phone_number); // 写入的数据是在缓存中
            fflush(fp); // 刷新缓冲区，把数据写入文件
            item = item->next;
            count++;
        }
    }
    fclose(fp);
    INFO("Saved %d contacts\n", count);
    return 0;
}
// 计算索引后插入对应组
int load_file(struct contacts *cts, const char *filename){
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return -1;
    }
    char name[NAME_LENGTH];
    char phone[PHONE_NUMBER_LENGTH];
    int count = 0;

    while (fscanf(fp, "name: %[^,], phone: %s\n", name, phone) == 2) {
        struct person *new_person = (struct person*)malloc(sizeof(struct person));
        if (new_person == NULL){
            fclose(fp);
            return -2;
        }
        // 把数据拷贝到新节点
        strncpy(new_person->name, name, NAME_LENGTH - 1);
        new_person->name[NAME_LENGTH - 1] = '\0';
        strncpy(new_person->phone_number, phone, PHONE_NUMBER_LENGTH - 1);
        new_person->phone_number[PHONE_NUMBER_LENGTH - 1] = '\0';
        new_person->next = NULL;
        new_person->previous = NULL;

        // 新增：计算索引并插入到对应组
        int index = get_name_index(new_person->name);
        if (index >= 0 && index < ALPHABET_COUNT) {
            person_insert(&cts->groups[index], new_person);
            count++;
        } else {
            free(new_person);
        }
    }
    cts->count = count;
    INFO("load success, loaded %d contacts\n", count);
    fclose(fp);
    return 0;
}

int save_entry(struct contacts *cts){
    if (cts == NULL) return -1;
    INFO("input filename: \n");
    char filename[NAME_LENGTH] = {0};
    scanf("%s", filename);
    return save_file(cts->groups, filename);

}
int load_entry(struct contacts *cts){
    if (cts == NULL) return -1;
    INFO("input filename: \n");
    char filename[NAME_LENGTH] = {0};
    scanf("%s", filename);
    return load_file(cts, filename);
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