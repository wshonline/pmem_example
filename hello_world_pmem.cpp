/* 这是一个PMEM的hello world程序，主要展示如何libpmemobj-cpp工具库进行持久内存C++编程。
 * 除了基本的新建/打开PMEM pool、PMEM对象分配、释放、读写等API的介绍外，
 * 还介绍了pmem::obj::vector<>这个与标准vector用法非常相似的PMEM版vector的用法。
 * 关于libpmemobj-cpp的其它API详细介绍，可参考官方文档https://pmem.io/libpmemobj-cpp/v1.12/doxygen/index.html。
 * 
 * 此程序同时提供了Makefile，可使用符合要求的编译环境直接运行make编译。
 */
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>
#include <libpmemobj++/container/vector.hpp>

using namespace pmem::obj;

#define LAYOUT "Hello World PMEM"
#define HELLO_PMEM_POOL_SIZE ((size_t)(1024 * 1024 * 10))
#define MAX_HISTORY 10

int main(int argc, char *argv[])
{
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

	/* 放置于pmem_pool的root位置的复合数据结构，每次打开pmem_pool(pop)，可以直接调用pop.root()自动定位获取。*/
	struct root
	{
		p<int> count;
		/* p<>模板用来申明存储在pmem的上基础数据结构，以确保之后在transaction中对其的修改能自动被pmdk捕捉并flush进pmem */
		persistent_ptr<compound_type> comp;
		/* persistent_ptr<>模板用来申明存储在pmem上的对象的持久化指针。 */
		pmem::obj::vector<persistent_ptr<compound_type>> history;
		/* 持久化vector模板使用方法与标准vector非常相似，此示例中用此记录comp变量的历史值。 */
	};

	/* 此hello_world_pmem例子需要传入一个运行时参数，即包括pmem_pool文件名在内的全路径，用于创建pmem_pool文件 */
	if (argc < 2)
	{
		std::cerr << "usage: " << argv[0]
				  << " pmem_pool_filename " << std::endl;
		return 1;
	}

	const char *path = argv[1];
	bool create = false;
	pool<root> pop;
	if (access(path, F_OK) == 0)
	{
		/* 如果pmem_pool文件已存在，则直接打开pmem_pool。pool<>中的模板用来定义pmem_pool的root数据类型。*/
		pop = pool<root>::open(path, LAYOUT);
	}
	else
	{
		/* 如果pmem_pool文件不存在，则以HELLO_PMEM_POOL_SIZE的大小创建一个新的pmem_pool。*/
		pop = pool<root>::create(
			path, LAYOUT, HELLO_PMEM_POOL_SIZE, S_IWUSR | S_IRUSR);
		create = true;
	}
	/* 调用.root()直接获取root对象的持久化指针 */
	auto proot = pop.root();

	if (create)
	{
		/* 对新建的pmem_pool，将初始化root对象中的1个基础类型变量count、1个复合类型变量comp、及持久化vector变量history */
		std::cout << "Create new pmem data: " << std::endl;
		/* libpmemobj-cpp利用c++11的Lambda functions，定义transaction，帮助程序员以最省心的方式保证pmem上的数据修改可以持久化，
		 * 并避免memory leak/dangling pointer等错误。
		 */
		transaction::run(pop, [&] {
			/* make_persistent<> 是最基本的用来分配pmem内存对象的方法，与传统的关键字new在dram上的作用类似。
			 * 模板中传入对象的数据类型，并支持传入初始化对象用的参数。
			 */
			proot->comp = make_persistent<compound_type>(1, 2.0);
			/* Transaction中可以直接对p<>申明的基础数据类型进行修改，此修改具有原子性，
			 * 即如果程序在transaction未成功完成前终止（如进程被终止或掉电关机），此修改无效，同样的，
			 * 如果程序在transaction完成后以任何方式终止，此修改可保证被持久化到pmem。
			 */
			proot->count = 1;
		});
		std::cout << "count = " << proot->count << std::endl;
		std::cout << "comp = (" << proot->comp->var_int << ", " << proot->comp->var_double << ")" << std::endl;
		std::cout << "number of comp history = " << proot->history.size() << " (max = " << MAX_HISTORY << ") " << std::endl;
	}
	else
	{
		/* 对已经存在的pmem_pool，将读取root对象中的1个基础类型变量count和1个复合类型变量comp
		 * 输出它们的值后，做一定修改，并将更新的值持久化写入pmem，
		 * 同时还会将旧值加入到comp的历史值vector中，并以旧到新的顺序，输出其所有历史值。
		 */
		std::cout << "Read old pmem data: " << std::endl;
		std::cout << "old count = " << proot->count << std::endl;
		std::cout << "old comp = (" << proot->comp->var_int << ", " << proot->comp->var_double << ")" << std::endl;
		/* 持久化指针persistent_ptr<>可以直接使用"*"来获取其指向的pmem内存对象 */
		compound_type tmp = *(proot->comp);
		/* 持久化指针persistent_ptr<>可以直接使用get()函数来获取pmem内存对象的普通指针 */
		compound_type *ptr_tmp = proot->comp.get();
		/* make_persistent必须在transaction中使用，否则会抛异常，如果只对单独变量分配PMEM并无相关其它修改操作，
		 * 也可以在transaction外直接使用make_persistent_atomic分配pmem内存对象。
		 * make_persistent_atomic也保证在分配pmem对象过程中程序意外终止，不会出现memory leak/dangling pointer等错误。
		 */
		persistent_ptr<compound_type> pptr_tmp;
		make_persistent_atomic<compound_type>(pop, pptr_tmp, tmp.var_int, ptr_tmp->var_double);
		transaction::run(pop, [&] {
			/* 修改pmem对象，也可以在transaction中直接使用"->"调用对象的成员函数，或者访问对象的成员变量，并保证修改的持久化和原子性。*/
			proot->comp->set_var_int(tmp.var_int + 1);
			proot->comp->var_double = tmp.var_double + 0.1;
			proot->count = proot->count + 1;
			/* 持久化vector，pmem::obj::vector<>使用方法和标准的std::vector<>非常相似，此示例将原先的旧数据加入到history中 */
			proot->history.emplace_back(pptr_tmp);
		});
		std::cout << "Update new pmem data: " << std::endl;
		std::cout << "new count = " << proot->count << std::endl;
		std::cout << "new comp = (" << proot->comp->var_int << ", " << proot->comp->var_double << ")" << std::endl;
		std::cout << "number of comp history = " << proot->history.size() << " (max = " << MAX_HISTORY << ") " << std::endl;
		std::cout << "last " << MAX_HISTORY << " comp values (from oldest to lastest) are :" << std::endl;
		size_t pos = 1;
		/* 和标准vector一样，持久化vector也支持迭代器，此示例程序使用它遍历持久化vector输出comp历史值 */
		for (auto it = proot->history.begin(); it != proot->history.end(); it++)
		{
			std::cout << "\t[" << pos++ << "] : ";
			std::cout << "(" << (*it)->var_int << ", " << (*it)->var_double << ")" << std::endl;
		}
		/* 当此时comp的历史值数量达到预设值MAX_HISTORY，我们将丢弃最旧的那个历史值 */
		if (proot->history.size() == MAX_HISTORY)
		{
			/* 使用迭代器获取最旧的历史值 */
			auto it_comp = proot->history.begin();
			transaction::run(pop, [&] {
				/* delete_persistent必须在transaction中调用，它会保证pmem空间被正确释放，同时会自动调用compound_type的析构函数。
				 * 此示例程序使用它释放comp的最旧历史值所占的PMEM内存，*it_comp会返回persistent_ptr<compound_type> */
				delete_persistent<compound_type>(*it_comp);
				/* 同时将最旧的历史值从持久化vector中移除 */
				proot->history.erase(it_comp);
			});
		}
	}
	/* 关闭pmem pool可以确保数据持久化 */
	pop.close();
}
