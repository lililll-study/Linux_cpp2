

#define DNS_SERVER_PORT 53
#define DNS_SERVER_IP   "114.114.114.114"





// 定义一个打包的函数
// 发的包是request，但是在已有header和question的基础上，合并到request上
// struct dns_hearder *header, 
// struct dns_question *question, 
// char *request
int dns_build_requestion(struct dns_hearder *header, struct dns_question *question, char *request) {
    
    if (header == NULL || question == NULL || request == NULL) return -1;
    memset(request, 0, rlen);

    // header --> request

    memcpy(request, header, sizeof(struct dns_header));
    int offset = sizeof(struct dns_header);

    // question --> request
    memcpy(request+offset, question->name, question->length);
    offset += question->length;

    memcpy(request+offset, question->qtype, sizeof(question->qtype));
    offset += sizeof(question->qtype);

    memcpy(request+offset, question->qclass, sizeof(question->qclass));
    offset += sizeof(question->qclass);
    return

}


// DNS服务器是基于无连接的，UDP
int dns_client_commit(const char *domain) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    sturct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DNS_SERVER_PORT);
    servaddr.sin_addr.s_addr = inet_addr(DNS_SERVER_IP);

    struct dns_header header = {0};
    dns_create_header(&header);

    struct dns_question question = {0};
    dns_create_question(&question, domain);

    char request[1024] = {0};
    int length = dns_build_requestion(&header, &question, request);

    // request
    // 把数据发送到dns服务器那一侧
    sendto(fd, request, length, 0, servaddr, addr_len);

}








