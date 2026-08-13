一 IOuring



io_uring_submit是向ring中推送任务，然后把任务给到worker，代码其实是阻塞在complete queue，也就是io_uring_wait_cqe函数处

![a6bf19de-65db-4035-be6c-a18d578a6c77](file:///U:/linux_first/02/pictures/a6bf19de-65db-4035-be6c-a18d578a6c77.png)

a. 频繁copy过程，mmap

b. 如何做到线程安全

新增了三个系统调用

1 io_uring_setup

2 io_uring_enter

3 io_uring_register



问题：

1 sq的entry与cq的entry有什么关系？是不是一个节点？

共用的是一块内存，共用的是一个节点。

2 io_uring_cq_advance为什么没有把set_event_accept清空

set_event_accept是存入submit中，而io_uring_cq_advance是把cq清空



iouring和epoll的区别--设计理念？

epoll每次设置完，等待io事件触发，不用修改就能有事件触发。而iouring设置完后需要再次设置。

走到循环while中，判断EVENT_READ时，数据已经度出来了。

而EPOLLIN时，是表示数据可读，并没有读出来。

WRITE也是一样的，数据已经写出去才会造成标志位。



叫做reactor 和 proactor的区别？--自行总结三点不一样



iouring的思路要说清楚



reactor.c和uring来进行对比：统计一秒钟的发送数据的次数



起两个tcp客户端，双方各绑定8000，两边同时连接，能不能连接成功。



测试数据：

epoll，100w请求，耗时为7s多

![b4e9bcd3-ab58-4e81-8fbc-3e4d8352be82](file:///U:/linux_first/02/pictures/b4e9bcd3-ab58-4e81-8fbc-3e4d8352be82.png)

qps = 100w/7011 约为13w多



uring测出的是大概5.6s

![d857c662-4d6e-4271-aaa9-bcb22744da7e](file:///U:/linux_first/02/pictures/d857c662-4d6e-4271-aaa9-bcb22744da7e.png)

qps 约为14w多



从50个连接来说，iouring性能更好



epoll 128字节

耗时：9087    qps：110047

epoll 256字节

耗时： 8682   qps：115048

epoll 512字节

耗时：  9359  qps：106849





iouring 128字节

耗时：7570    qps：132100

iouring 256字节

耗时： 7645   qps：130804

iouring 512字节

耗时：  7780  qps：128534



1. qps->64 128 256 512

2. 测并发连接的数量

3. 根据连接数量建立的时间（建链时间）

4. 断链



简历：把简历中的内容都梳理清楚，能聊的都写好



TCP和UDP有哪些区别？

1：TCP基于连接的，UDP基于数据包的

也就是说在TCP上，客户端会有一个连接，服务端也会有一个连接。是一对一的。是顺序的，先发的包先收。

UDP是没有顺序的，会有分包和粘包的问题。收到mtu的影响。

2：分包和粘包的解决方案

对于TCP的分包和粘包的问题，可以在应用层封装，也就是在tcp包前加入2个字节的长度，然后接收时先接收这个头，然后再接收总数据，最后对比一下就可以把整个包接收过来。也可以做个分隔符，在数据中插入分隔符，从而做一个包的切割。而UDP需要为每个包分配id值

3：并发的做法不一样

TCP只要通过epoll的方式，就可以实现。而UDP需要模拟TCP的方式来建立连接。

4：使用场景不一样

UDP的实时性比TCP强，在应用层上，可以做到每个包快速的确认（延迟确认）。在下载时也是用UDP确认（拥塞控制）。两者是或的关系，

在选择UDP时要么是实时性，要么是传输包的大小。而TCP不用考虑这么多。

5：UDP偏向于做短连接（发起一次请求，如DNS，不用建立连接，只要发包和收包就行了），TCP长连接





网络部分

每个概念具体的代码实现是什么样的？会用到什么数据结构？以及准备10个面试题（八股相关的）

作业：

UDP并发如何做？


