# Linux C/C++学习笔记

虚拟机用户名lhy，密码123321

## 一. Linux基本操作

```linux
cd ../    # 返回上一级
ls ../    # 查看上一级目录的文件


mkdir lhy.sh    # 创建文件
touch lhy.sh    # 创建文件
vim lhy.sh    # 若有文件：则编辑；若无文件，则创建后再编辑



权限：
ls -l     #查看当前目录下，文件的权限
chmod +x ping.sh    # 给.sh文件可执行权限，有该权限则执行上一条后会显示绿色   
```

## 二. Shell脚本编程

### 1 shell 简介

**为什么要用shell编程呢？** 这里的shell编程通常指shell脚本或Bash，相当于**把Linux中执行的命令写成一条流水线**，**把所有命令自动化执行。** 只要把之前的gcc命令写进.sh文件，加上if判断，就能变成自动化脚本了。

**执行shell脚本的cmd**：“./ping.sh” 或者 如果没有头文件则“/bin/bash ./ping.sh”

### 2 shell 语法

3个示例都用到了for，因此重点关注for用法，示例代码如下：

#### ping测试多个地址

```shell
# 实现功能：测试连接所有虚拟机
#|/bin/bash
头文件
for i in [1...254]; do
    ping -c 2 -i 0.5 192.168.199.$i &>/dev/null
    if [ $? -eq 0 ]; then
        echo "192.168.199.$i is up"
    else 
        echo "192.168.199.$i is down"
    fi
done


# 为了不打印出ping的每一条无效信息，需要修改
    ping -c 2 -i 0.5 192.168.199.$i
    ping -c 2 -i 0.5 192.168.199.$>/dev/null
```

#### 读取目录文件名称

```shell
lhy = "www.nuaa.com"
echo $lhy

for file in $(ls /home/lhy/share/); do
    echo "${file}"
done
```

用法：

for x in items; do

    xxx

done



### 3 特殊符号

| ：管道，将左边命令输出作为右边命令的输入

awk： 文本处理工具，工作模式：读入每一行，按照空白符拆成多个字段，对字段进行处理

awk '{print $1}'： 单引号内容告诉awk要执行什么。1表示当前行第1个字段。

* `print $1` 的意思是：**打印当前行的第 1 列**。

$：命令替换。让sheel先执行括号里的，然后把执行结果替换到当前位置。

```shell
local_ip=$(hostname -I | awk '{print $1}')
```

### 4 任务--shell获取本机IP

```shell
#!/bin/bash
local_ip=$(hostname -I | awk '{print $1}')
echo "Local IP Address: $local_ip"



```





# 编程技术点解析

## 零 状态机实现文件单词统计

### 1 作业-实现单词个数的统计

**需求分析：**

    当前通过fgetc函数从文件中拿到一个字符，然后对字符判断是否等于关键字，如果等于则将状态切换（单词数+1）。如果需要统计每个单词的数量，有下面两种方案：

<mark>暴力解法</mark>

```text
while循环（结束标志为EOF）
判断当前的字符是否是字母。
    如果是字母，则把字母转小写并加入word中
    如果不是字母，则结束当前单词，并把单词加入到dict中。
最后打印数组中的键值对。
```

这样的话，时间复杂度位于O(N^2)和O(n * m)之间，如果想降低时间复杂度，需要实现哈希表存储。





## 一 通讯录项目

### 1 形参指针变量修改

```c
原代码
int person_insert1(struct person *people, struct person *person){
    if (person == NULL) return -1;
    LIST_INSERT(person, people);
    return 0;
}
改后
int person_insert2(struct person **ppeople, struct person *person){
    if (person == NULL) return -1;
    LIST_INSERT(person, *ppeople);
    return 0;
}
调用
struct person *head = null;
struct person lhy;
person_insert2(head, &lhy)
```

使用方法1会造成的问题：

        在main中，把实参传入方法1的*people后，方法1的代码只修改了形参的值，然后people会被销毁，这样main中的实参值仍然是原来的值，数据丢失。

正确的方法（方法2）：

        使用双指针的方法，把输入的形参变为指针的指针

### 2 通讯录项目实现

#### 1️⃣实现思路——分层设计

需要实现一个通讯录，主要逻辑就是实现一个链表，链表中的每个节点是一个二元组，然后能够实现对链表的插入，删除，查找和遍历。但是实现过程需要分层设计：数据层、接口层、业务层。

    分层设计的好处是，当数据层的链表需要换成其他算法时，只要修改数据层即可，接口层，业务层都无需修改。复用性很高。

#### 2️⃣实现代码重写

##### 底层操作层

```c
// 底层数据层entity，构建的是双向列表
#define LIST_INSERT(item, list) do{\
    item -> previous = NULL; \  //先将待插入的pre置空
    item -> next = list; \ // 然后把待插入的指向链表头节点（即指向A节点）
    list = item; \    // 然后把item赋值给list，使得list更新新节点
}while(0)

#define LIST_REMOVE(item, list) do{
    // 如果item有前驱节点，那么让前驱节点跳过待删除节点
    if(item -> previous){item -> previous -> next = item -> next;}
    // 如果item有后继节点，那么让后继节点跳过待删除节点
    if(item -> next){item -> next -> previous = item -> previous;}
    // 如果要删除的是头结点，更新头指针即可
    if(list == item){list = item -> next;}
    // 最后，清空被删除节点的指针
    item->previous = item->next = NULL;
}while(0)


// 定义联系人
struct person{
    char name[NAME_LENGTH];
    char phone_number[PHONE_NUMBER_LENGTH];
    // 结构体内套用自身，形成链式结构 ***📌重点
    struct person *next;
    struct person *pervious;
}
// 定义通讯录
struct contacts{
    struct person *people;
    int count;
}
```

**注意：**

（1）删除操作时，没有释放掉item的内存。但是释放操作没有在宏中进行，原因如下：

        1：违反单一职责原则

        2：无法在外部对变量进行额外处理

        3：在外部调用时，外部的变量会出现野指针(指向已被释放或无效的内存地址)。

##### 接口层

```
// 接口层interface
```





##### 业务层



##### 主函数



## 二 高并发技术方案——锁

##### 互斥锁（Mutex）

线程获取锁失败的时候，操作系统会将其挂起，进入阻塞状态，放入等待队列中，不消耗CPU资源；如果当前锁被释放，则由内核kernel唤醒等待线程。

```
线程想进入临界区：
  1. 尝试获取锁
  2. 如果锁被占用 → 线程进入睡眠（让出CPU）
  3. 等待被唤醒
  4. 获得锁 → 执行临界区代码
  5. 释放锁 → 唤醒等待的线程
```

* **原理**：如果厕所（临界区）有人，你就去**睡觉**（线程休眠），等别人出来叫醒你。

* **优点**：不浪费CPU资源（睡觉不耗电）。

* **缺点**：叫醒（上下文切换）很耗时，大约需要几千个CPU周期。

* **场景**：**复杂操作**。比如你的联系人程序里，`malloc`分配内存、或者删除节点后要`free`，这些操作很慢，用互斥锁最合适。
  
  

##### 自旋锁（Spinlock）

线程获取锁失败时，不休眠，而是循环不断检查锁状态（类似进入while(1)）,直到获取成功。

```
线程想进入临界区：
  1. 尝试获取锁
  2. 如果锁被占用 → 不停循环检查（忙等）
  3. 直到锁被释放
  4. 获得锁 → 执行临界区代码
  5. 释放锁
```

* **原理**：如果微波炉（临界区）被人占用，你就站在旁边**死死盯着**（CPU忙等），每秒问一万次“好了没？”

* **优点**：没有叫醒开销，响应极快（微秒级）。

* **缺点**：盯着的时候啥也干不了，白白耗电（占满CPU）。

* **场景**：**极短操作**。比如仅仅修改一个标志位（`flag = 1`）。在Linux内核中，如果临界区只有几条指令，就用自旋锁。
  
  

##### 原子操作（Atomic Operation）

由CPU硬件保证（汇编语言编写），一条指令要么完全执行，要么完全不执行，中间不会被中断。

```
由硬件保证操作不可分割：
┌─────────────────────────────────────────────┐
│ count++ 实际上分三步：                      │
│   1. 读取 count 到寄存器                    │
│   2. 寄存器 +1                             │
│   3. 写回 count                            │
│                                             │
│ 原子操作：三步一次完成，中间不被中断！     │
└─────────────────────────────────────────────┘
```

* **原理**：不需要“锁”这个机制。CPU硬件保证，**读-改-写**这三步操作**一气呵成**，中间不允许任何人插队。

* **优点**：**最快**！没有锁机制的开销。

* **缺点**：只能处理简单的数学运算（如 `count++`），没法处理复杂的业务逻辑。

* **场景**：**计数器**。比如统计网站访问量，直接用 `atomic_fetch_add`。
  
  

##### CAS（compare and swap）

* **原理**：不锁门，大家直接冲进去改数据。改之前先看一眼旧值，如果旧值没变，就改；如果变了，就**重试**（重新读、重新算、再试一次）。

* **本质**：这是实现**无锁编程**的基石，可以看作是“原子操作”的升级版。

* **场景**：**高性能容器**。比如Java的`ConcurrentHashMap`、C++的`std::atomic`底层大量使用CAS。
  
  

### 为什么要用到锁？

在多线程任务中，多个任务都需要对内存进行访问，而线程执行任务是分为三步完成的：读改写。例如threadA刚执行完读10，CPU再切给threadB执行把count改为改为11，接着切回A，此时A还是旧值10，就会执行改和写把11覆盖为11，这样数据就错乱了。

那么就在任务上加一个锁，一个线程拿到锁进入任务后，会把门关起来，其他的线程要执行这个任务就必须拿到锁，不然无法访问任务，数据错改的问题就被解决了。

当前对于线程访问的问题，一共有三类解决方法，锁，原子操作，CAS。锁分为互斥锁和自旋锁，互斥锁如果线程访问任务不成功，那么会挂起，后续由CPU的kernel唤起，不占用CPU资源。自旋锁如果线程访问不成功，则会循环等待，这个过程会占用CPU资源。原子操作是指，把读改写这三部用汇编写好，让CPU一步执行完，不存在中断。这个过程没有锁的概念，开销很小，但是无法执行复杂任务。CAS指，所有线程都能来执行任务，改之前要看一眼旧值，如果旧值没变就修改，如果变了就重新读改写。

**注意**
1：原子操作在底层（CPU层面）也是需要“锁”的，只不过它用的是**硬件锁（总线锁或缓存行锁）**，而不是软件锁（mutex）。  
它相当于**CPU内部自带的一个极简门锁**，只锁住“读改写”这一条指令执行的瞬间（纳秒级）。所以它的开销比你讲的互斥锁（涉及操作系统调度，微秒级）要小得多。

2：在CAS中可能存在ABA的问题，指的是修改数据结果和旧值一样，但是中间态有改变。这时另外一个线程会默认数据没被修改，可能导致程序逻辑错误。解决方法是在CAS中带上一个版本号。



## 三 线程池

核心技术

### 1 什么是线程队列？

线程队列在被创建后，**是由Linux内核统一管理的**。**在高并发的场景下，线程队列在没有任务时会被阻塞在条件变量上，当有任务时，会被轮流唤醒，去从任务队列中取出任务执行。**

线程的阻塞和唤醒：pthread_cond_wait(), pthread_cond_signal()

在线程销毁中唤醒全部线程: pthread_cond_broadcast(),

程序员负责维护的是任务队列，而线程队列的唤醒和调度是由操作系统的条件变量(pthread_cond)统一管理的。



### 2 什么是线程加锁？

线程加锁的目的是为了保证多线程处理任务时的，任务安全性。<mark>如果给线程加锁的话，那么只有这个 线程能够进行操作任务队列，其他线程不行。（当前线程处于临界区，收到保护的共享资源）在操作完任务之后，在解锁。</mark>

简单来说，把多线程的并行操作变为了串行操作。

### 3 什么是条件变量cond？

条件变量(Condition Variable)， 是一个“线程等待队列”。本质是内核维护的一个等待队列，类似一个线程链表，其中的线程睡眠则入队(pthread_cond_wait)，线程唤醒则出队(pthread_cond_signal)。

```c
// 工作线程 (消费者)：没事就睡觉
pthread_cond_wait(&pool->cond, &pool->mutex);


// 主线程 (生产者)：有任务就按门铃
pthread_cond_signal(&pool->cond);
```

线程池的设计：任务队列和线程队列

线程池的回调函数设计（如何设计回调函数，才能够实现高并发）

线程池的API创建要点（pool->cond作用！！）

    创建线程池的方法

    销毁线程池的方法（堆栈释放）

向线程池推送任务的设计方法



其他任务：

1：在锁的任务中实现了百万次并发，更改为使用CAS实现

2：这里的锁，能不能改为其他锁？有什么区别？能不能用CAS？这里能不能更改线程池的数量？

3：如何查看线程池中的20个线程都被调用了？



## 四 数据库

### 1 MySQL数据库操作

在数据库workbench中操作表，和使用C代码操作表的流程有什么不同

- workbench中连接数据库以及初始化的操作都通过软件直接完成，只要输入sql语句就行。

- 通过代码操作，需要先初始化数据库，然后连接数据库，接着再发送请求。得到的数据库结果是存储在管道内的，需要设计API进行读取。

```sql
# 1. 给admin用户授予全部权限
*.*表示所有库所有表
grant all privileges on *.* to 'admin'@'%';

# 2. 给admin授予mysql数据库的权限
grant select on mysql.* to 'admin'@'%';

# 3. 创建一个新用户
%表示允许从任何IP地址连接
create user 'admin'@'%' identified by '123321';


show databases;
use mysql;
show tables;
mysql -u root -p
```



### 2 项目中的API封装

#### ①数据库封装

现在驱动层封装的很薄，可以进一步把驱动层抽象出来

```c
// 直接使用 MySQL C API，但用宏隔离了连接信息
#define LHY_DB_SERVER_IP   "192.168.137.128"
#define LHY_DB_SERVER_PORT 3306
#define LHY_DB_SERVER_USER "admin"
#define LHY_DB_SERVER_PWD  "123321"
#define LHY_DB_DEFAULTDB   "LHY_DB"

// 直接调用 MySQL API
MYSQL mysql;
mysql_init(&mysql);
mysql_real_connect(&mysql, LHY_DB_SERVER_IP, LHY_DB_SERVER_USER, ...);
```

#### ② SQL语句封装

直接通过#define定义sql语句。包括插入数据，查询数据，定义一个过程，插入图片数据。

```sql
语句中用到了一个过程，过程PROC是在MYSQL workbench中定义的
#define SQL_DELETE_TBL_USER "CALL PROC_DELETE_USER('bruce')"


USE LHY_DB;
DELIMITER $$

CREATE PROCEDURE PROC_DELETE_USER(IN UNAME VARCHAR(32))
BEGIN
    SET SQL_SAFE_UPDATES = 0;
    DELETE FROM TBL_USER WHERE U_NAME = UNAME;
    SET SQL_SAFE_UPDATES = 1;
END$$

CALL PROC_DELETE_USER('LHY');
```



#### ③ 网络封装

当前项目采用的是本地命令行工具，还没有网络层，如果要对外提供服务，需要采用HTTP + JSON的格式。



### 3 项目中遇到的问题

数据库服务器限制root的远程登陆

admin没有权限操作数据库

数据库建模：把所有数据，通过数据库的语言，用数据库的格式，放进数据库。通过表存储起来，然后再用程序往表中插入数据。



```text
客户端    login       node server      select         db server
微信  →  网络连接   →  业务逻辑实现  →   网络连接    →   数据库服务器
qq等                                                     ⬇
                                                       数据库
```



安装MySQL工具

sudo apt-get install libmysqlclient-dev







将图片插入数据库

1：准备好一张图片并且将图片read

2：xxx，xxx，MySQL_write_image

3：mysql_read_image

4：写入磁盘 



准备一个statement，然后把相应参数的类型和statement绑定，然后把参数发送过去，之后再把它插入都数据库服务器里面





作业题：封装一个数据库连接池

![3b67015f-5f94-4267-a4ee-5171f53bc8e1](U:\linux_first\picture\3b67015f-5f94-4267-a4ee-5171f53bc8e1.png)



## 五 DNS协议及UDP编程

### 1 网络层级架构



- **物理层**：bits流

- **数据链路层**：MAC地址，用于在同一个局域网上找到对方

- **网络层**：IP地址，在全球互联网中找到对方

- **传输层**：端口号，数据该交给电脑上的那个程序 (进程)

- **协议层**：传输协议 (数据格式)，规定数据的格式(DNS, FTP, HTTP)
  
  

### 2 各类网络协议对比

<mark>HTTP：网站的内容        ——应用层</mark>

<mark>DNS：把域名转换为IP，查地址  —— 应用层</mark>

<mark>UDP和TCP：运输方式（怎么把数据送出去）——传输层</mark>



### 3 DNS为什么是“树状结构”

DNS的域名空间本质上是一颗 “倒挂的树”。如下图所示，首先在根服务器(.)中查询.com的位置，根返回IP后，去这个IP查询0voice.com在哪，依次查询完整个www.0voice.com整个域名，然后拿到IP地址。

这就是典型的“**<mark>递归查询 + 迭代查询</mark>**”

```text
                       . (根)
                       │
         ┌─────────────┼─────────────┐
         │             │             │
        com           net           org    （顶级域）
         │             │
    ┌────┴────┐   ┌────┴────┐
  baidu   google 0voice  github    （二级域）
    │        │
  www       mail                    （主机名）

域名hostname： www .0voice .com
DNS编码name:  3www 60voice 3com0                       
```

#### 哈夫曼树

一种数据压缩算法，是多叉树，统计出现字符的频率，出现频率越高的越靠近根节点，这样高频字符的编码更短。

**<mark>本质：优先处理最重要的分支</mark>**

#### 常见应用- 各类压缩领域

**1: 文件压缩 (ZIP)**

扫描文件，统计每个字节出现的频率，频率高的用短bit表示，频率低的用长bit表示。

**2: 图片压缩 (JPEG)**

* JPEG 在“量化”之后，会对图像数据做哈夫曼编码。高频细节（人眼不敏感）用长编码，低频信息（人眼敏感）用短编码。

* **效果**：在几乎不损失画质的情况下，把照片体积缩小 5～10 倍。
  
  

### 4 项目中技术点

#### 1：strtok

按照指定的分隔符 delim，把一个字符串拆成多个token。

```c
char *strtok(char *str, const char *delim)

char data[] = "www.0voice.com";
char delim[] = ".";    // 代码中定义的是dellim[2] = "."，这是包含了分隔符

// 第一次调用：传入原字符串
char *token = strtok(data, delim);  // 返回 "www"
while (token != NULL) {
    printf("%s\n", token);
    // 后续调用：传 NULL，继续拆剩下的部分
    token = strtok(NULL, delim);
}
```



#### 2 :  srtncpy

安全复制字符串，最多复制n给字符从src到dest。

```c
char dest[10];
char src[] = "hello";

strncpy(dest, src, sizeof(dest)-1);
dest[sizeof(dest)-1] = '\0';
```

#### 3 : srtdup

复制字符串，并动态分配内存 (内部调用malloc)。

```c
char *origin = "hello";
char *copy = strdup(origin);  // 自动分配内存并复制

printf("%s\n", copy);  // hello
free(copy);            // 需要手动释放！
```



#### 4 : 套接字

套接字Socket是网络通信的门把手。在Linux系统中，socket是一个文件描述符，在使用如下的socket函数创建套接字后，会返回一个sockfd，后续针对这个连接的操作都通过sockfd来识别。

```c
int sockfd = socket(AF_INET, SOCK_DGRAM, 0)
// AFINET表示使用IPV4地址族
// Socket Datagram（数据报套接字）
sockfd >0表示创建成功
```

### 5 传输方式

**UDP的好处，是TCP不具备的**

1: UDP传输速度快（对网络带宽无限制）

- 例如传输大文件的时候（下载）

2: UDP响应速度快

- 用在游戏领域（游戏）
  
  

项目思考：

- 此处使用的是UDP进行数据传输，能改成TCP吗？

- 实现异步DNS

- 什么是DNS协议，协议内容有什么
