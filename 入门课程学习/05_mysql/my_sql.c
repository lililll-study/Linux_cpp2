
#include <mysql.h>
#include <stdio.h>
#include <string.h>


#define LHY_DB_SERVER_IP        "192.168.137.128"
#define LHY_DB_SERVER_PORT      3306
#define LHY_DB_SERVER_USER      "admin"
#define LHY_DB_SERVER_PWD       "123321"
#define LHY_DB_DEFAULTDB       "LHY_DB"

// ============ 连接池配置 ============
#define MAX_POOL_SIZE   10      // 最大连接数
#define MIN_POOL_SIZE   3       // 最小连接数（初始连接数）

// 插入一条数据
#define SQL_INSERT_TBL_USER     "INSERT TBL_USER(U_NAME, U_GENGDER) VALUES('bruce', 'dog');"
#define SQL_SELECT_TBL_USER     "SELECT * FROM TBL_USER;"

// 存储过程：删除数据
#define SQL_DELETE_TBL_USER     "CALL PROC_DELETE_USER('bruce')"

// 插入一条图片数据（？表示图片位置数据未知，待插入）
#define SQL_INSERT_IMG_USER     "INSERT TBL_USER(U_NAME, U_GENGDER, U_IMG) VALUES('lhy', 'man', ?);"

// 查询图片数据
#define SQL_SELECT_IMG_USER     "SELECT U_IMG FROM TBL_USER WHERE U_NAME='lhy';"

// 最大存储图片大小64k
#define FILE_IMAGE_LENGTH       (64 * 1024)


/*
MYSQL BLOB图片增删改查
    把图片存入数据库和从数据库中读出图片
*/

// C U R D -->
//

int lhy_mysql_select(MYSQL *handle){
    // mysql_real_query --> sql
    if (mysql_real_query(handle, SQL_SELECT_TBL_USER, strlen(SQL_SELECT_TBL_USER))){
        // =0，表示成功
        printf("mysql_real_query : %s\n", mysql_error(handle));
        return -1;
    }
    // store -->
    MYSQL_RES *res = mysql_store_result(handle);
    if (res == NULL) {
        printf("mysql_store_query : %s\n", mysql_error(handle));
        return -2;
    }
    // rows / fields
    int rows = mysql_num_rows(res);
    printf("rows : %d\n", rows);
    int fields = mysql_num_fields(res);
    printf("fields : %d\n", fields);

    // fetch
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))){
        int i=0;
        for(i=0; i<fields; i++){
            printf("%s\t", row[i]);
        }
        printf("\n");
    }

    mysql_free_result(res);
    return 0;
}


// 读取图片
// @func:
    // filename: 图片路径
    // buffer: 图片数据存储地址
int read_image(char *filename, char *buffer) {

    if (filename == NULL || buffer == NULL) return -1;

    FILE *fp = fopen(filename, "rb");
    if (fp ==NULL){
        printf("fopen failed\n");
        return -2;
    }
    // 把文件指针指向最末尾
    fseek(fp, 0, SEEK_END);
    int length = ftell(fp);
    fseek(fp, 0, SEEK_SET);// 获得文件大小

    int size = fread(buffer, 1, length, fp);
    if (size != length){
        printf("fread failed: %d\n", size);
        fclose(fp);
        return -3;
    }
    fclose(fp);
    return size;
}
// 将图片写入磁盘中
int write_image(char *filename, char *buffer, int length) {
    if (filename ==NULL || buffer == NULL || length <= 0) return -1;
    FILE *fp = fopen(filename, "wb"); //以写入的方式打开
    if (fp ==NULL){
        printf("fopen failed\n");
        return -2;
    }

    int size = fwrite(buffer, 1, length, fp);
    if (size != length){
        printf("fwrite failed: %d\n", size);
        fclose(fp);
        return -3;
    }
    fclose(fp);
    return size;
}

int MYSQL_write(MYSQL *handle, char *buffer, int length) {
    if (handle ==NULL || buffer == NULL || length <= 0) return -1;

    // 1. MYSQL预处理语句（向数据库申请一个语句对象）
    MYSQL_STMT *stmt = mysql_stmt_init(handle);// 创建一个语句句柄，后续所有操作都通过这个句柄管理
        // 这条sql上准备，相当于绑定了一个对象
    int ret = mysql_stmt_prepare(stmt, SQL_INSERT_IMG_USER, strlen(SQL_INSERT_IMG_USER));
    if(ret) {
        printf("mysql_stmt_prepare: %s\n", mysql_error(handle));
        return -2;
    }

    MYSQL_BIND param = {0};
    param.buffer_type = MYSQL_TYPE_LONG_BLOB; // MYSQL要存入的数据类型（BLOB）
    param.buffer = NULL;
    param.is_null = 0;
    param.length = NULL;

    // 2. 把stmt和param绑定起来，处理？占位符。
        // 表示使用param结构体来描述？占位符
    ret = mysql_stmt_bind_param(stmt, &param);
    if (ret) {
        printf("mysql_stmt_bind_param : %s\n", mysql_error(handle));
        return -3;
    }

    // 3. 把图片数据发送到MySQL服务器
    // 0表示给第一个？传数据；循环调用这个函数，可以使得数据被分开发送
    ret = mysql_stmt_send_long_data(stmt, 0, buffer, length);
    if (ret) {
        printf("mysql_stmt_send_long_data : %s\n", mysql_error(handle));
        return -4;
    }
    // 4. 执行，把数据写入数据库
    ret = mysql_stmt_execute(stmt);
    if (ret) {
        printf("mysql_stmt_execute : %s\n", mysql_error(handle));
        return -5;
    }
    // 5. 关闭释放（释放stmt所占用的所有资源）
    ret = mysql_stmt_close(stmt);
    if (ret) {
        printf("mysql_stmt_close : %s\n", mysql_error(handle));
        return -6;
    }

    return ret;

}

int MYSQL_read(MYSQL *handle, char *buffer, int length) {
    if (handle ==NULL || buffer == NULL || length <= 0) return -1;

    MYSQL_STMT *stmt = mysql_stmt_init(handle);
    int ret = mysql_stmt_prepare(stmt, SQL_SELECT_IMG_USER, strlen(SQL_SELECT_IMG_USER));
    if(ret) {
        printf("mysql_stmt_prepare: %s\n", mysql_error(handle));
        return -2;
    }

    MYSQL_BIND result = {0};
    result.buffer_type = MYSQL_TYPE_LONG_BLOB;
    unsigned long total_length = 0;
    result.length = &total_length; // 把total_length的地址告诉了MySQL，MySQL就把内部长度填进去

    // 把stmt和result绑定起来
    ret = mysql_stmt_bind_result(stmt, &result);
    if (ret) {
        printf("mysql_stmt_bind_result : %s\n", mysql_error(handle));
        return -3;
    }

    // 执行查询
    ret = mysql_stmt_execute(stmt);
    if (ret) {
        printf("mysql_stmt_execute : %s\n", mysql_error(handle));
        return -4;
    }

    // 把结果集下载到客户端
    ret = mysql_stmt_store_result(stmt);
    if (ret) {
        printf("mysql_stmt_store_result : %s\n", mysql_error(handle));
        return -5;
    }

    while(1){
        // 此处会通过fetch获取图片BLOB字段的实际长度，写入total_length
        ret = mysql_stmt_fetch(stmt);
            // 数据读取失败或者 只读取到一半
        if (ret != 0 && ret != MYSQL_DATA_TRUNCATED) break;
        
        int start = 0;
        while (start < (int)total_length) {
            result.buffer = buffer + start; // 指向缓冲区的下一个位置
            result.buffer_length = 1;       // 每次只读一个字节
            mysql_stmt_fetch_column(stmt, &result, 0, start);
            start += result.buffer_length;  // start每次+1
        }

    }
    mysql_stmt_close(stmt);
    return total_length;

}



int main() {
    MYSQL mysql;
    // 1. 数据库初始化
    if (NULL == mysql_init(&mysql)){
        printf("mysql_init : %s\n", mysql_error(&mysql));
        return -1;
    }
    // 2. 连接数据库
    if (!mysql_real_connect(&mysql, LHY_DB_SERVER_IP, LHY_DB_SERVER_USER,
        LHY_DB_SERVER_PWD, LHY_DB_DEFAULTDB, LHY_DB_SERVER_PORT, NULL, 0)) {
        // =0, 表示连接失败
        printf("mysql_real_connect : %s\n", mysql_error(&mysql));
        goto Exit;
    }

    printf("====================================\n");
    // MYSQL --> INSERT
#if 1// 3. 发送请求（插入一条图片数据）
    if (mysql_real_query(&mysql, SQL_INSERT_TBL_USER, strlen(SQL_INSERT_TBL_USER))){
        // =0，表示成功
        printf("mysql_real_query : %s\n", mysql_error(&mysql));
        goto Exit;
    }
#endif// 打印表中数据
    lhy_mysql_select(&mysql);


    printf("====================================\n");
    // MYSQL--> DELETE
#if 1// 4. 发送删除数据的请求
    if (mysql_real_query(&mysql, SQL_DELETE_TBL_USER, strlen(SQL_DELETE_TBL_USER))){
        // =0，表示成功
        printf("mysql_real_query : %s\n", mysql_error(&mysql));
        goto Exit;
    }

#endif
    lhy_mysql_select(&mysql);




// 1. 测试案例 -> 将图片读取后写入数据库
    printf("CASE : mysql --> read image and write mysql \n");

    char buffer[FILE_IMAGE_LENGTH] = {0};
    ///home/lhy/share/05_mysql
    int length = read_image("litte_cat.jpg", buffer);
    if(length <0 ) goto Exit;

    MYSQL_write(&mysql, buffer, length);


// 2. 测试案例 -> 将图片从表读取放入本地
    printf("CASE : mysql --> read mysql and write image \n");
    memset(buffer, 0, FILE_IMAGE_LENGTH);
    length = MYSQL_read(&mysql, buffer, FILE_IMAGE_LENGTH);
    write_image("a.jpg", buffer, length);

Exit:
    mysql_close(&mysql);
    return 0;
}



