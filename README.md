# **Hello World, PMem!**

我们在这里准备了一个PMem版的Hello World程序，给大家直观的讲解一下如果通过libpmemobj-cpp函数库进行C++的持久内存编程。
hello_world_pmem是一个使用libpmemobj-cpp在持久内存中存储数据的示例程序，将提供的源文件hello_world_pmem.cpp和Makefile放在目录中运行make即可编译成可执行文件。

## hello_world_pmem简介

hello_world_pmem使用libpmemobj-cpp创建一个持久内存池（pmem pool），并在其root中，存放一个如下结构的持久内存对象：
```cpp
struct root
{
	p<int> count;
	/* p<>模板用来申明存储在pmem的上基础数据结构，以确保之后在transaction中对其的修改能自动被pmdk捕捉并flush进pmem */
	persistent_ptr<compound_type> comp;
	/* persistent_ptr<>模板用来申明存储在pmem上的对象的持久化指针。 */
	pmem::obj::vector<persistent_ptr<compound_type>> history;
	/* 持久化vector模板使用方法与标准vector非常相似，此示例中用此记录comp变量的历史值。 */
};
```
其中compound_type定义如下：
```cpp
/* 一个简单的复合数据类型 */
struct compound_type
{
	compound_type(int val, double dval)
		: var_int(val), var_double(dval)
	{
	}

	void
	set_var_int(int val)
	{
		var_int = val;
	}

	int var_int;
	double var_double;
};
```
hello_world_pmem程序运行时，需要传入一个PMem路径下的文件，用来开辟pmem pool。
首次运行时（即传入的文件不存在），程序将创建一个新的pmem pool，并将struct root中的count和comp初始化并打印出来。
接下来再次运行时（即传入的文件已经存在），程序会将此pmem pool打开，读取并打印上述两个变量的旧值，然后将它们更新，并将更新后的值再次写入pmem并打印。同时，会将他们的旧值添加到一个类型为pmem::obj::vector（与c++标准容器vector非常类似的持久化容器vector）的history变量中，history用来记录这两个变量的最近一定数量的历史值。如果发现存在历史值，会将一定数量的历史值按旧到新的顺序打印出来。

***更详细的描述和API使用方法，请参考hello_world_pmem.cpp的注释部分。***

## hello_world_pmem运行示例

### 编译
```console
$ make
g++ -g -Wall -Werror -std=gnu++11   -c -o hello_world_pmem.o hello_world_pmem.cpp
g++ -o hello_world_pmem  hello_world_pmem.o -lpmem -lpmemobj -pthread
```

### 首次运行
```console
$ ./hello_world_pmem testpool
Create new pmem data: 
count = 1
comp = (1, 2)
number of comp history = 0 (max = 10) 
```

### 再次运行N次
```console
$ ./hello_world_pmem testpool
Read old pmem data: 
old count = 1
old comp = (1, 2)
Update new pmem data: 
new count = 2
new comp = (2, 2.1)
number of comp history = 1 (max = 10) 
last 10 comp values (from oldest to lastest) are :
        [1] : (1, 2)

$ ./hello_world_pmem testpool
Read old pmem data: 
old count = 2
old comp = (2, 2.1)
Update new pmem data: 
new count = 3
new comp = (3, 2.2)
number of comp history = 2 (max = 10) 
last 10 comp values (from oldest to lastest) are :
        [1] : (1, 2)
        [2] : (2, 2.1)

$ ./hello_world_pmem testpool
Read old pmem data: 
old count = 3
old comp = (3, 2.2)
Update new pmem data: 
new count = 4
new comp = (4, 2.3)
number of comp history = 3 (max = 10) 
last 10 comp values (from oldest to lastest) are :
        [1] : (1, 2)
        [2] : (2, 2.1)
        [3] : (3, 2.2)

.
.
.

$ ./hello_world_pmem testpool
Read old pmem data: 
old count = 10
old comp = (10, 2.9)
Update new pmem data: 
new count = 11
new comp = (11, 3)
number of comp history = 10 (max = 10) 
last 10 comp values (from oldest to lastest) are :
        [1] : (1, 2)
        [2] : (2, 2.1)
        [3] : (3, 2.2)
        [4] : (4, 2.3)
        [5] : (5, 2.4)
        [6] : (6, 2.5)
        [7] : (7, 2.6)
        [8] : (8, 2.7)
        [9] : (9, 2.8)
        [10] : (10, 2.9)

# 示例中只保留最近的10个历史值，所以当历史值超过10个后，最旧的那个将会被丢弃，显示如下

$ ./hello_world_pmem testpool
Read old pmem data: 
old count = 11
old comp = (11, 3)
Update new pmem data: 
new count = 12
new comp = (12, 3.1)
number of comp history = 10 (max = 10) 
last 10 comp values (from oldest to lastest) are :
        [1] : (2, 2.1)
        [2] : (3, 2.2)
        [3] : (4, 2.3)
        [4] : (5, 2.4)
        [5] : (6, 2.5)
        [6] : (7, 2.6)
        [7] : (8, 2.7)
        [8] : (9, 2.8)
        [9] : (10, 2.9)
        [10] : (11, 3)

$ ./hello_world_pmem testpool
Read old pmem data: 
old count = 12
old comp = (12, 3.1)
Update new pmem data: 
new count = 13
new comp = (13, 3.2)
number of comp history = 10 (max = 10) 
last 10 comp values (from oldest to lastest) are :
        [1] : (3, 2.2)
        [2] : (4, 2.3)
        [3] : (5, 2.4)
        [4] : (6, 2.5)
        [5] : (7, 2.6)
        [6] : (8, 2.7)
        [7] : (9, 2.8)
        [8] : (10, 2.9)
        [9] : (11, 3)
        [10] : (12, 3.1)
```

# poolset的使用方法
通常情况，当我们创建一个持久内存池（pmem pool）的时候，需要固定持久内存池的大小。对于某些应用，持久内存所需要的大小是动态变化的，
这时候我们就需要poolset的方案来解决这个问题。

poolset需要提供一个set file，来定义poolset的位置，以及最大的持久内存空间。下面是一个poolset的set file的简单示例：
```bash
PMEMPOOLSET
OPTION SINGLEHDR
100G /pmem/poolset
```
其中`/pmem/poolset`为预先创建的目录位置，`100G`为最大的持久内存空间（但真实pool文件会动态分配，而不是直接分配100G的pool文件）。

使用poolset的时候需要注意：
- 当我们调用`pool<>::create`或者`pool<>::open`的时候，传入的`path`参数即为set file，而非真实的持久内存池的位置。
- `pool<>::create`的时候，`size`必须设为0
- 判断持久内存池是否已经存在变得复杂，不过可以通过以下简单方式来避免判断逻辑的复杂性：
    ```c++
    try {
      pop = pool<>::create(<set file>, LAYOUT, 0, S_IWUSR | S_IRUSR);
    } catch (const pmem::pool_error &e) {                                                                                                                                                                        
      pop = pool<Tree>::open(<set file>, LAYOUT);
    }  
    ```
