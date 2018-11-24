#include<iostream>
#include<string>
#include<unistd.h>
#include<pthread.h>
#include<semaphore.h>

using namespace std;

//队列的最大长度
#define MAXLENGTH 200

//abcd四个路口的互斥锁
pthread_mutex_t a, b, c, d;
//用来保护条件变量的互斥锁
pthread_mutex_t mtx_e_in, mtx_s_in, mtx_n_in, mtx_w_in;
pthread_mutex_t mtx_e, mtx_s, mtx_n, mtx_w, mtx, deadl;
//每个方向车辆前进的启动信号
pthread_cond_t e_go, w_go, s_go, n_go;
//唤醒死锁检测线程的信号量，返回死锁问题已解决的信号
pthread_cond_t deadlock, dead_solve;
//每辆车在进入路口前预先减少资源值
//若只有三辆车或以下在路口内，那么无论如何都不会发生死锁
//若第四辆车希望进入路口，必定会发生死锁，唤醒死锁检测线程
int deadlock_detect = 4;
typedef enum{east,south,west,north}direc_dl_happen;
direc_dl_happen di;


//判断右方是否有来车
bool e_coming, w_coming, n_coming, s_coming;

class car_queue {
	//队列记录车辆的编号
	int car_id[MAXLENGTH];
	//指向队列的头尾
	int head;
	int tail;
public:
	//当前队列中车辆的数目，需要被直接读写
	int count;
	//构造函数
	car_queue() {
		head = tail = count = 0;
	}
	//将一辆车加入当前队列
	void car_enter(int n);
	//将一辆车从队列中取出，并返回它的编号
	int car_leave();
};

//从东南西北四个方向来的车辆队列
car_queue from_e, from_w, from_s, from_n;

//车辆线程调用的函数
//四个方向的车辆调度从原理上来说完全一致，区别仅仅在方向上
void* manage_e(void*) {
	//保护用
	pthread_mutex_lock(&mtx_e_in);
	//在路口外等待出发信号唤醒,收到出发信号后进入下一步
	pthread_cond_wait(&e_go, &mtx_e_in);
	pthread_mutex_unlock(&mtx_e_in);

	//标记该方向有车进入，左方车辆如果检查到该值为真，会停在其第一个路口内等待本车通过
	e_coming = true;
	//从对应方向队列中取出头车
	int num = from_e.car_leave();
	//输出车辆到达路口的相关信息
	cout << "car " << num << " from Eest arrives at crossing" << endl;

	//被唤醒后，首先检查自己是否导致死锁
	pthread_mutex_lock(&mtx);
	deadlock_detect--;
	//若发生死锁
	if (!deadlock_detect) {
		pthread_mutex_unlock(&mtx);

		//标记导致死锁发生的车辆方向，唤醒死锁处理线程
		di = east;
		pthread_cond_signal(&deadlock);

		//停在路口外，等待死锁解决后重新启动
		pthread_mutex_lock(&mtx_e);
		pthread_cond_wait(&dead_solve, &mtx_e);
		//由于左侧车等待过该车，死锁解决后通过时将会发出一个启动信号
		pthread_cond_wait(&e_go,&mtx_e);
		pthread_mutex_unlock(&mtx_e);
	}
	else 
		pthread_mutex_unlock(&mtx);

	//若该车不会导致死锁，进入路口，无条件抢占第一个路口
	//真正进入路口时队列中的车辆数减少
	from_e.count--;
	pthread_mutex_lock(&b);
	//进入路口耗费时间
	usleep(100);

	//此时检查右方是否有来车，若有则等待
	//右方车辆通过后会给它发送一个启动信号
	//若右方车辆会导致死锁，右方车辆线程将会唤醒死锁检测线程
	//死锁检测线程会发出启动信号
	//用一个布尔值检测自己是否曾经等待，若有则在通过后唤醒右方下一辆车
	bool waited = false;
	if (n_coming) {
		//设置曾等待
		waited = true;
		//等待启动信号
		pthread_mutex_lock(&mtx_e);
		pthread_cond_wait(&e_go, &mtx_e);
		pthread_mutex_unlock(&mtx_e);
	}
	//获得启动信号后或右方无来车，抢占第二个路口，释放第一个路口
	pthread_mutex_lock(&c);
	pthread_mutex_unlock(&b);
	deadlock_detect++;
	//进入路口耗费时间
	usleep(100);
	
	//如果等待过右方车，且右方仍然有车，给它发送启动信号，左右车同时行进不会相互干扰
	//无论是否发生过死锁，此时等待在路口外的右车行为是一致的
	if (waited && from_n.count)
		pthread_cond_signal(&n_go);
	//若左方有车等待，给它发送一个启动信号，由它给队列的下一辆车发送启动信号
	//左方等待的车辆已在路口内，因此无需检查是否队列中仍有车
	if (s_coming)
		pthread_cond_signal(&s_go);
	//左方若无车，给自己的队列中下一辆车发送启动信号
	else if (from_e.count)
		pthread_cond_signal(&e_go);
	//若右方有来车，右方车先收到后已经改变变量表示右方有来车
	//虽然本方向车暂时还未释放第二个路口，但后一辆车可以检测到右方有来车，不会与之争抢路口

	//释放第二个路口，离开
	cout << "car " << num << " from East leaving crossing" << endl;
	pthread_mutex_unlock(&c);
}

void* manage_s(void*) {
	//保护用
	pthread_mutex_lock(&mtx_s_in);
	//在路口外等待出发信号唤醒,收到出发信号后进入下一步
	pthread_cond_wait(&s_go, &mtx_s_in);
	pthread_mutex_unlock(&mtx_s_in);

	//标记该方向有车进入，左方车辆如果检查到该值为真，会停在其第一个路口内等待本车通过
	s_coming = true;
	//从对应方向队列中取出头车
	int num = from_s.car_leave();
	//输出车辆到达路口的相关信息
	cout << "car " << num << " from South arrives at crossing" << endl;

	//被唤醒后，首先检查自己是否导致死锁
	pthread_mutex_lock(&mtx);
	deadlock_detect--;
	//若发生死锁
	if (!deadlock_detect) {
		pthread_mutex_unlock(&mtx);

		//标记导致死锁发生的车辆方向，唤醒死锁处理线程
		di = south;
		pthread_cond_signal(&deadlock);

		//停在路口外，等待死锁解决后重新启动
		pthread_mutex_lock(&mtx_s);
		pthread_cond_wait(&dead_solve, &mtx_s);
		//由于左侧车等待过该车，死锁解决后通过时将会发出一个启动信号，接收此信号
		pthread_cond_wait(&s_go, &mtx_s);
		pthread_mutex_unlock(&mtx_s);
	}
	else
		pthread_mutex_unlock(&mtx);

	//若该车不会导致死锁，进入路口，无条件抢占第一个路口
	//真正进入路口时队列中的车辆数减少
	from_s.count--;
	pthread_mutex_lock(&a);
	//进入路口耗费时间
	usleep(100);

	//此时检查右方是否有来车，若有则等待
	//右方车辆通过后会给它发送一个启动信号
	//若右方车辆会导致死锁，右方车辆线程将会唤醒死锁检测线程
	//死锁检测线程会发出启动信号
	//用一个布尔值检测自己是否曾经等待，若有则在通过后唤醒右方下一辆车
	bool waited = false;
	if (e_coming) {
		//设置曾等待
		waited = true;
		//等待启动信号
		pthread_mutex_lock(&mtx_s);
		pthread_cond_wait(&s_go, &mtx_s);
		pthread_mutex_unlock(&mtx_s);
	}
	//获得启动信号后或右方无来车，抢占第二个路口，释放第一个路口
	pthread_mutex_lock(&b);
	pthread_mutex_unlock(&a);
	deadlock_detect++;
	//进入路口耗费时间
	usleep(100);

	//如果等待过右方车，且右方仍然有车，给它发送启动信号，左右车同时行进不会相互干扰
	//无论是否发生过死锁，此时等待在路口外的右车行为是一致的
	if (waited && from_e.count)
		pthread_cond_signal(&e_go);
	//若左方有车等待，给它发送一个启动信号，由它给队列的下一辆车发送启动信号
	//左方等待的车辆已在路口内，因此无需检查是否队列中仍有车
	if (w_coming)
		pthread_cond_signal(&w_go);
	//左方若无车，给自己的队列中下一辆车发送启动信号
	else if (from_s.count)
		pthread_cond_signal(&s_go);
	//若右方有来车，右方车先收到后已经改变变量表示右方有来车
	//虽然本方向车暂时还未释放第二个路口，但后一辆车可以检测到右方有来车，不会与之争抢路口

	//释放第二个路口，离开
	cout << "car " << num << " from South leaving crossing" << endl;
	pthread_mutex_unlock(&b);
}

void* manage_w(void*) {
	//保护用
	pthread_mutex_lock(&mtx_w_in);
	//在路口外等待出发信号唤醒,收到出发信号后进入下一步
	pthread_cond_wait(&w_go, &mtx_w_in);
	pthread_mutex_unlock(&mtx_w_in);

	//标记该方向有车进入，左方车辆如果检查到该值为真，会停在其第一个路口内等待本车通过
	w_coming = true;
	//从对应方向队列中取出头车
	int num = from_w.car_leave();
	//输出车辆到达路口的相关信息
	cout << "car " << num << " from West arrives at crossing" << endl;

	//被唤醒后，首先检查自己是否导致死锁
	pthread_mutex_lock(&mtx);
	deadlock_detect--;
	//若发生死锁
	if (!deadlock_detect) {
		pthread_mutex_unlock(&mtx);

		//标记导致死锁发生的车辆方向，唤醒死锁处理线程
		di = west;
		pthread_cond_signal(&deadlock);

		//停在路口外，等待死锁解决后重新启动
		pthread_mutex_lock(&mtx_w);
		pthread_cond_wait(&dead_solve, &mtx_w);
		//由于左侧车等待过该车，死锁解决后通过时将会发出一个启动信号，接收此信号
		pthread_cond_wait(&w_go, &mtx_w);
		pthread_mutex_unlock(&mtx_w);
	}
	else
		pthread_mutex_unlock(&mtx);

	//若该车不会导致死锁，进入路口，无条件抢占第一个路口
	//真正进入路口时队列中的车辆数减少
	from_w.count--;
	pthread_mutex_lock(&d);
	//进入路口耗费时间
	usleep(100);

	//此时检查右方是否有来车，若有则等待
	//右方车辆通过后会给它发送一个启动信号
	//若右方车辆会导致死锁，右方车辆线程将会唤醒死锁检测线程
	//死锁检测线程会发出启动信号
	//用一个布尔值检测自己是否曾经等待，若有则在通过后唤醒右方下一辆车
	bool waited = false;
	if (s_coming) {
		//设置曾等待
		waited = true;
		//等待启动信号
		pthread_mutex_lock(&mtx_w);
		pthread_cond_wait(&w_go, &mtx_w);
		pthread_mutex_unlock(&mtx_w);
	}
	//获得启动信号后或右方无来车，抢占第二个路口，释放第一个路口
	pthread_mutex_lock(&a);
	pthread_mutex_unlock(&d);
	deadlock_detect++;
	//进入路口耗费时间
	usleep(100);

	//如果等待过右方车，且右方仍然有车，给它发送启动信号，左右车同时行进不会相互干扰
	//无论是否发生过死锁，此时等待在路口外的右车行为是一致的
	if (waited && from_s.count)
		pthread_cond_signal(&s_go);
	//若左方有车等待，给它发送一个启动信号，由它给队列的下一辆车发送启动信号
	//左方等待的车辆已在路口内，因此无需检查是否队列中仍有车
	if (n_coming)
		pthread_cond_signal(&n_go);
	//左方若无车，给自己的队列中下一辆车发送启动信号
	else if (from_w.count)
		pthread_cond_signal(&w_go);
	//若右方有来车，右方车先收到后已经改变变量表示右方有来车
	//虽然本方向车暂时还未释放第二个路口，但后一辆车可以检测到右方有来车，不会与之争抢路口

	//释放第二个路口，离开
	cout << "car " << num << " from West leaving crossing" << endl;
	pthread_mutex_unlock(&a);
}

void* manage_n(void*) {
	//保护用
	pthread_mutex_lock(&mtx_n_in);
	//在路口外等待出发信号唤醒,收到出发信号后进入下一步
	pthread_cond_wait(&n_go, &mtx_n_in);
	pthread_mutex_unlock(&mtx_n_in);

	//标记该方向有车进入，左方车辆如果检查到该值为真，会停在其第一个路口内等待本车通过
	n_coming = true;
	//从对应方向队列中取出头车
	int num = from_n.car_leave();
	//输出车辆到达路口的相关信息
	cout << "car " << num << " from North arrives at crossing" << endl;

	//被唤醒后，首先检查自己是否导致死锁
	pthread_mutex_lock(&mtx);
	deadlock_detect--;
	//若发生死锁
	if (!deadlock_detect) {
		pthread_mutex_unlock(&mtx);

		//标记导致死锁发生的车辆方向，唤醒死锁处理线程
		di = north;
		pthread_cond_signal(&deadlock);

		//停在路口外，等待死锁解决后重新启动
		pthread_mutex_lock(&mtx_n);
		pthread_cond_wait(&dead_solve, &mtx_n);
		//由于左侧车等待过该车，死锁解决后通过时将会发出一个启动信号，接收此信号
		pthread_cond_wait(&n_go, &mtx_n);
		pthread_mutex_unlock(&mtx_n);
	}
	else
		pthread_mutex_unlock(&mtx);

	//若该车不会导致死锁，进入路口，无条件抢占第一个路口
	//真正进入路口时队列中的车辆数减少
	from_n.count--;
	pthread_mutex_lock(&c);
	//进入路口耗费时间
	usleep(100);

	//此时检查右方是否有来车，若有则等待
	//右方车辆通过后会给它发送一个启动信号
	//若右方车辆会导致死锁，右方车辆线程将会唤醒死锁检测线程
	//死锁检测线程会发出启动信号
	//用一个布尔值检测自己是否曾经等待，若有则在通过后唤醒右方下一辆车
	bool waited = false;
	if (w_coming) {
		//设置曾等待
		waited = true;
		//等待启动信号
		pthread_mutex_lock(&mtx_n);
		pthread_cond_wait(&n_go, &mtx_n);
		pthread_mutex_unlock(&mtx_n);
	}
	//获得启动信号后或右方无来车，抢占第二个路口，释放第一个路口
	pthread_mutex_lock(&d);
	pthread_mutex_unlock(&c);
	deadlock_detect++;
	//进入路口耗费时间
	usleep(100);

	//如果等待过右方车，且右方仍然有车，给它发送启动信号，左右车同时行进不会相互干扰
	//无论是否发生过死锁，此时等待在路口外的右车行为是一致的
	if (waited && from_w.count)
		pthread_cond_signal(&w_go);
	//若左方有车等待，给它发送一个启动信号，由它给队列的下一辆车发送启动信号
	//左方等待的车辆已在路口内，因此无需检查是否队列中仍有车
	if (e_coming)
		pthread_cond_signal(&e_go);
	//左方若无车，给自己的队列中下一辆车发送启动信号
	else if (from_n.count)
		pthread_cond_signal(&n_go);
	//若右方有来车，右方车先收到后已经改变变量表示右方有来车
	//虽然本方向车暂时还未释放第二个路口，但后一辆车可以检测到右方有来车，不会与之争抢路口

	//释放第二个路口，离开
	cout << "car " << num << " from North leaving crossing" << endl;
	pthread_mutex_unlock(&d);
}


void* solving_deadlock(void*) {
	//始终执行
	while (1) {
		//等待死锁条件达成时被唤醒
		pthread_mutex_lock(&deadl);
		pthread_cond_wait(&deadlock, &deadl);
		pthread_mutex_unlock(&deadl);
		//此时的状态为：某方向车辆准备进入第一个路口，其他三个方向均在第一个路口内等待启动信号
		//报deadlock信息
		cout << "DEADLOCK: car jam detected, signalling ";
		//给未进入路口方向的左侧车辆发出启动信号
		switch (di)
		{
		case east: cout << "South to go" << endl; pthread_cond_signal(&s_go); break;
		case south: cout << "West to go" << endl; pthread_cond_signal(&w_go); break;
		case west: cout << "North to go" << endl; pthread_cond_signal(&n_go); break;
		case north: cout << "East to go" << endl; pthread_cond_signal(&e_go); break;
		default:
			break;
		}

		//等待首个启动的车释放资源，它同时还会唤醒下一辆等待的车
		while (!deadlock_detect);
		//释放资源后死锁解除，发送信号让未进入路口的车启动
		pthread_cond_signal(&dead_solve);
	}
}

int main() {
	//读入输入
	string carlist;
	cin >> carlist;
	
	//将车放入队列，并为每辆车创建线程
	//各个方向首个创建的线程获取互斥锁，等待唤醒命令
	pthread_t thread_list[MAXLENGTH];
	for (int i = 0; i < carlist.length(); i++) {
		switch (carlist[i])
		{
		case 'e': 
			from_e.car_enter(i + 1);
			pthread_create(thread_list + i, 0, manage_e, 0);
			break;
		case 's':
			from_s.car_enter(i + 1);
			pthread_create(thread_list + i, 0, manage_s, 0);
			break;
		case 'w':
			from_w.car_enter(i + 1);
			pthread_create(thread_list + i, 0, manage_w, 0);
			break;
		case 'n':
			from_n.car_enter(i + 1);
			pthread_create(thread_list + i, 0, manage_n, 0);
			break;
		default:
			break;
		}
	}

	//死锁检测线程
	pthread_t de_dt;
	pthread_create(&de_dt, 0, solving_deadlock, 0);

	//唤醒各方向首个线程
	pthread_cond_signal(&w_go);
	pthread_cond_signal(&s_go);
	pthread_cond_signal(&n_go);
	pthread_cond_signal(&e_go);

	for (int i = 0; i < carlist.length(); i++) {
		pthread_join(thread_list[i], nullptr);
	}
}



void car_queue::car_enter(int n)
{
	tail++;
	if (tail >= MAXLENGTH)
		return;
	else
		car_id[tail] = n;
	count++;
}

int car_queue::car_leave()
{
	int n = car_id[head];
	head++;
	return n;
}
