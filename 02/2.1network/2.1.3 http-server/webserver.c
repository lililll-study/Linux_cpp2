

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
    // printf("request: %s\n", c->rbuffer);
    memset(c->wbuffer, 0, BUFFER_LENGTH);
    c->wlength = 0;
    c->status = 0;
}

int http_response(struct conn *c) {
    // printf("response\n");


#if 1
    c->wlength = sprintf(c->wbuffer, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: 82\r\n"
        "Data: Tue, 30 Apr 2024 08:00:01 GMT\r\n\r\n"
        "<html><head><title>0voice.king</title></head><body><h1>King</h1></body></html>\r\n\r\n");

#elif 0
    int filefd = open("index.html",O_RDONLY);

    struct stat stat_buf;
    fstat(filefd, &stat_buf);

    c->wlength = sprintf(c->wbuffer, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: %ld\r\n"
        "Data: Tue, 30 Apr 2024 08:00:01 GMT\r\n\r\n",
        stat_buf.st_size);

    int count = read(filefd, c->wbuffer + c->wlength, BUFFER_LENGTH - c->wlength);
    c->wlength += count;
    close(filefd);

#elif 0

    int filefd = open("index.html",O_RDONLY);

    struct stat stat_buf;
    fstat(filefd, &stat_buf);

    if (c->status == 0) {
        c->wlength = sprintf(c->wbuffer, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Accept-Ranges: bytes\r\n"
            "Content-Length: %ld\r\n"
            "Data: Tue, 30 Apr 2024 08:00:01 GMT\r\n\r\n",
            stat_buf.st_size);

        c->status = 1;
    } else if (c->status ==1) {

        int ret = sendfile(c->fd, filefd, NULL, stat_buf.st_size);
        if (ret == -1) {
            printf("sendfile error: %d\n", errno);
        }
        // c->wlength = 0;
        // memset(c->wbuffer, 0, BUFFER_LENGTH);

        c->status = 2;
    } else if (c->status == 2) {

        c->wlength = 0;
        memset(c->wbuffer, 0, BUFFER_LENGTH);
        c->status = 0;
    }



    close(filefd);

#else

    int filefd = open("liulian.png",O_RDONLY);

    struct stat stat_buf;
    fstat(filefd, &stat_buf);

    if (c->status == 0) {
        c->wlength = sprintf(c->wbuffer, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: image/png\r\n"
            "Accept-Ranges: bytes\r\n"
            "Content-Length: %ld\r\n"
            "Data: Tue, 30 Apr 2024 08:00:01 GMT\r\n\r\n",
            stat_buf.st_size);

        c->status = 1;
    } else if (c->status ==1) {

        int ret = sendfile(c->fd, filefd, NULL, stat_buf.st_size);
        if (ret == -1) {
            printf("sendfile error: %d\n", errno);
        }
        // c->wlength = 0;
        // memset(c->wbuffer, 0, BUFFER_LENGTH);

        c->status = 2;
    } else if (c->status == 2) {

        c->wlength = 0;
        memset(c->wbuffer, 0, BUFFER_LENGTH);
        c->status = 0;
    }



    close(filefd);

#endif
    return c->wlength;
}


