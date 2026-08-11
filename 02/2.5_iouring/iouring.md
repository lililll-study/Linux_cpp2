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


