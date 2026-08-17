# 一、背景

（1）学完了网络要把所有知识点串起来，就要用到这个项目。知识点如下

0. 一请求一线程

1. select、poll、epoll

2. ntyco

3. dpdk封装TCP/IP

4. IO_Uring

5. iocp

        kv存储主要构建的是key-value的键值对存储，而MySQL中存的是标准的表格，每一条数据都有，是关系型数据库。**而kv是用key去查找value**。

1. redis

2. MongoDB

3. Memcached

        **既然有了这些方案为什么要做kv存储呢？**

1. 把业务做简单，就做几个功能（两个功能），发现自己做也能驾驭

2. 把性能做到极致，对比性能和redis



**（3）kv store怎么用？**

        比如在抖音，有抖音ID，对应用户名，两者有关联关系。或者在图床中，用链接访问png图片，形成了短连接和长连接的映射关系。

        在淘宝的商品链接很长，但是点分享后会有一个很短的字符，你放入浏览器就可以访问这个商品，就是短链接和长连接的映射。

        这是一个基础设施，用起来是无感知的。

        再比如说webserver实现一个问卷调查，访问前端发来问卷，可能做了30分钟，然后再发回去，服务端保存了Sation，问卷有一个cookie，两者对应。

        是一种数据组织的方式。



# 二、拆分功能

1. 独立运行（独立的进程）

2. 通过网络访问（通过node访问，两者间是基于TCP的），会有短连接和长连接的映射关系

<mark>不用公开的库，用自己封装的库来做，用reactor.c</mark>



✅️设计模式

从下面的代码中可以看出，

1. 过滤器模式：可以在来到数据的时候解析，然后分发给特定的接口。

2. 观察者模式：来了事件通知三个接口，丢给每个处理。

```c
#if 0 // echo
    conn_list[fd].wlength = conn_list[fd].rlength;
    memcpy(conn_list[fd].wbuffer, conn_list[fd].rbuffer, conn_list[fd].wlength);
    printf("[%d]recv: %s\n", conn_list[fd].rlength, conn_list[fd].rbuffer);

#elif ENABLE_HTTP
// 接受完数据后做request请求
    http_requset(&conn_list[fd]);

#elif ENABLE_WS
    ws_request(&conn_list[fd]);

#elif ENABLE_KVS

    kvs_request(&conn_list[fd]);
```

能使用reactor网络的框架完成数据的收发，后续的开发功能就和网络没关系了。

1. reactor来做底层的网络框架

2. 用协程ntyco来做：把解析放在server_reader的while循环中，用协程封装的底层非阻塞API来recv

3. io_uring的做法

4. iocp也可以

底层的网络框架是可以跨平台的



当前代码：

    main -> 网络框架里

    协议处理 -> kvstore.c

请注意，主循环与业务相关，所以main应该放入kvstore.c中



作业：在Windows上兼容iocp



通信协议设计：解决tcp分包与粘包的问题

1. recv();

            先接收的就是对方先发送的（保证接收顺序）；接收完的数据中间不会出现丢数据（保证数据必达性）

            因此在数据包前面加上包长

            每次先接收2个字节的数据，然后再接收剩余的数据

```c
连续接收两次
short length = 0;
recv(fd, &length, 2, 0);
recv(fd, buffer, length, 0);
```

2. ET和LT都能做这个功能，但是对方可能一次发多个包，两者分别怎么实现？

3. redis的话，拆分为token分发，比如发GET Teacher

发2\r\n -> 3\r\n -> GET\r\n -> 7\r\n -> Teacher\r\n,那就需要自己实现readline，然后把\r\n去掉。

首先tokens=atoi(buffer)，然后循环tokens次readline



# 三、代码设计

## 1 kv存储+reactor实现

迭代的过程 产生架构

代码都是分层设计的



自己封装malloc这些，使得不直接使用系统调用。



## 2 kv引擎实现

### 2.1 测试用例实现

1. tcp客户端,建立连接
2. 发送协议,
3. 接收服务端返回的数据
4. 预期数据 与 服务端返回数据对比。



测试QPS时，发现效果。当关闭打印信息后，qps大幅提升，说明IO操作是非常耗时的：

```bash
带打印：
lhy@ubuntu:~/share/linux_first/12_1$ ./testcase 192.168.137.128 2000
array_testcase -- > time used: 2604, qps: 3456
不带打印
lhy@ubuntu:~/share/linux_first/12_1$ ./testcase 192.168.137.128 2000
array_testcase -- > time used: 49, qps: 183673
lhy@ubuntu:~/share/linux_first/12_1$ ./testcase 192.168.137.128 2000
array_testcase -- > time used: 51, qps: 176470

```

作业： 客户端如何实现多线程、多进程，来提高qps

### 2.1 引擎实现



```c
引擎层        array    rbtree    (5cmd + 2(create&des))


协议层        协议设计  - SET GET DEL MOD EXIST


网络层        reactor ntyco io_uring
```

编译命令

```bash
gcc -o kvstore kvstore.c reactor.c kvs_array.c
```


