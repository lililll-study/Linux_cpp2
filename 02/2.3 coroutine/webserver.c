

#include <string.h>
#include <stdio.h>
#include "server.h"
#include "unistd.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "sys/sendfile.h"
#include "errno.h"


#define WEBSERVER_ROOTDIR "./"


// ----------
int http_requset(struct conn *c) {
    printf("request: %s\n", c->rbuffer);
    memset(c->wbuffer, 0, BUFFER_LENGTH);
    c->wlength = 0;
    c->status = 0;
}

int http_response(struct conn *c) {
    // 1. 打开图片文件
    int filefd = open("liulian.png", O_RDONLY);
    if (filefd < 0) {
        printf("Open file liulian.png failed: %s\n", strerror(errno));
        // 返回 404
        const char *text_404 = "404 Not Found";
        c->wlength = sprintf(c->wbuffer, 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n%s", (int)strlen(text_404), text_404);
        
        // 直接发送 404 响应
        send(c->fd, c->wbuffer, c->wlength, 0);
        return -1;
    }

    // 2. 获取文件大小
    struct stat stat_buf;
    fstat(filefd, &stat_buf);
    off_t file_size = stat_buf.st_size;

    // 3. 构造 HTTP 响应头
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/png\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n", 
        file_size);

    // 4. 先发送响应头
    ssize_t sent_header = send(c->fd, header, header_len, 0);
    if (sent_header < 0) {
        printf("Send header failed: %s\n", strerror(errno));
        close(filefd);
        return -1;
    }
    printf("Sent header %ld bytes\n", sent_header);

    // 5. 使用 sendfile 发送文件内容（零拷贝）
    off_t offset = 0;
    ssize_t sent_bytes = sendfile(c->fd, filefd, &offset, file_size);
    if (sent_bytes < 0) {
        printf("sendfile failed: %s\n", strerror(errno));
        close(filefd);
        return -1;
    }
    printf("sendfile sent %ld bytes (total file size: %ld)\n", sent_bytes, file_size);

    // 6. 关闭文件
    close(filefd);

    // 7. 设置 wbuffer 和 wlength 为 0（表示没有待发送的数据）
    c->wlength = 0;
    memset(c->wbuffer, 0, BUFFER_LENGTH);

    return 0;
}


