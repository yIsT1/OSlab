#include<iostream>
#include<string>
#include<unistd.h>
#include<pthread.h>
#include<semaphore.h>

using namespace std;

//���е���󳤶�
#define MAXLENGTH 200

//abcd�ĸ�·�ڵĻ�����
pthread_mutex_t a, b, c, d;
//�����������������Ļ�����
pthread_mutex_t mtx_e_in, mtx_s_in, mtx_n_in, mtx_w_in;
pthread_mutex_t mtx_e, mtx_s, mtx_n, mtx_w, mtx, deadl;
//ÿ��������ǰ���������ź�
pthread_cond_t e_go, w_go, s_go, n_go;
//������������̵߳��ź������������������ѽ�����ź�
pthread_cond_t deadlock, dead_solve;
//ÿ�����ڽ���·��ǰԤ�ȼ�����Դֵ
//��ֻ����������������·���ڣ���ô������ζ����ᷢ������
//����������ϣ������·�ڣ��ض��ᷢ��������������������߳�
int deadlock_detect = 4;
typedef enum{east,south,west,north}direc_dl_happen;
direc_dl_happen di;


//�ж��ҷ��Ƿ�������
bool e_coming, w_coming, n_coming, s_coming;

class car_queue {
	//���м�¼�����ı��
	int car_id[MAXLENGTH];
	//ָ����е�ͷβ
	int head;
	int tail;
public:
	//��ǰ�����г�������Ŀ����Ҫ��ֱ�Ӷ�д
	int count;
	//���캯��
	car_queue() {
		head = tail = count = 0;
	}
	//��һ�������뵱ǰ����
	void car_enter(int n);
	//��һ�����Ӷ�����ȡ�������������ı��
	int car_leave();
};

//�Ӷ��������ĸ��������ĳ�������
car_queue from_e, from_w, from_s, from_n;

//�����̵߳��õĺ���
//�ĸ�����ĳ������ȴ�ԭ������˵��ȫһ�£���������ڷ�����
void* manage_e(void*) {
	//������
	pthread_mutex_lock(&mtx_e_in);
	//��·����ȴ������źŻ���,�յ������źź������һ��
	pthread_cond_wait(&e_go, &mtx_e_in);
	pthread_mutex_unlock(&mtx_e_in);

	//��Ǹ÷����г����룬�󷽳��������鵽��ֵΪ�棬��ͣ�����һ��·���ڵȴ�����ͨ��
	e_coming = true;
	//�Ӷ�Ӧ���������ȡ��ͷ��
	int num = from_e.car_leave();
	//�����������·�ڵ������Ϣ
	cout << "car " << num << " from Eest arrives at crossing" << endl;

	//�����Ѻ����ȼ���Լ��Ƿ�������
	pthread_mutex_lock(&mtx);
	deadlock_detect--;
	//����������
	if (!deadlock_detect) {
		pthread_mutex_unlock(&mtx);

		//��ǵ������������ĳ������򣬻������������߳�
		di = east;
		pthread_cond_signal(&deadlock);

		//ͣ��·���⣬�ȴ������������������
		pthread_mutex_lock(&mtx_e);
		pthread_cond_wait(&dead_solve, &mtx_e);
		//������೵�ȴ����ó������������ͨ��ʱ���ᷢ��һ�������ź�
		pthread_cond_wait(&e_go,&mtx_e);
		pthread_mutex_unlock(&mtx_e);
	}
	else 
		pthread_mutex_unlock(&mtx);

	//���ó����ᵼ������������·�ڣ���������ռ��һ��·��
	//��������·��ʱ�����еĳ���������
	from_e.count--;
	pthread_mutex_lock(&b);
	//����·�ںķ�ʱ��
	usleep(100);

	//��ʱ����ҷ��Ƿ���������������ȴ�
	//�ҷ�����ͨ������������һ�������ź�
	//���ҷ������ᵼ���������ҷ������߳̽��ỽ����������߳�
	//��������̻߳ᷢ�������ź�
	//��һ������ֵ����Լ��Ƿ������ȴ�����������ͨ�������ҷ���һ����
	bool waited = false;
	if (n_coming) {
		//�������ȴ�
		waited = true;
		//�ȴ������ź�
		pthread_mutex_lock(&mtx_e);
		pthread_cond_wait(&e_go, &mtx_e);
		pthread_mutex_unlock(&mtx_e);
	}
	//��������źź���ҷ�����������ռ�ڶ���·�ڣ��ͷŵ�һ��·��
	pthread_mutex_lock(&c);
	pthread_mutex_unlock(&b);
	deadlock_detect++;
	//����·�ںķ�ʱ��
	usleep(100);
	
	//����ȴ����ҷ��������ҷ���Ȼ�г����������������źţ����ҳ�ͬʱ�н������໥����
	//�����Ƿ�������������ʱ�ȴ���·������ҳ���Ϊ��һ�µ�
	if (waited && from_n.count)
		pthread_cond_signal(&n_go);
	//�����г��ȴ�����������һ�������źţ����������е���һ�������������ź�
	//�󷽵ȴ��ĳ�������·���ڣ�����������Ƿ���������г�
	if (s_coming)
		pthread_cond_signal(&s_go);
	//�����޳������Լ��Ķ�������һ�������������ź�
	else if (from_e.count)
		pthread_cond_signal(&e_go);
	//���ҷ����������ҷ������յ����Ѿ��ı������ʾ�ҷ�������
	//��Ȼ��������ʱ��δ�ͷŵڶ���·�ڣ�����һ�������Լ�⵽�ҷ���������������֮����·��

	//�ͷŵڶ���·�ڣ��뿪
	cout << "car " << num << " from East leaving crossing" << endl;
	pthread_mutex_unlock(&c);
}

void* manage_s(void*) {
	//������
	pthread_mutex_lock(&mtx_s_in);
	//��·����ȴ������źŻ���,�յ������źź������һ��
	pthread_cond_wait(&s_go, &mtx_s_in);
	pthread_mutex_unlock(&mtx_s_in);

	//��Ǹ÷����г����룬�󷽳��������鵽��ֵΪ�棬��ͣ�����һ��·���ڵȴ�����ͨ��
	s_coming = true;
	//�Ӷ�Ӧ���������ȡ��ͷ��
	int num = from_s.car_leave();
	//�����������·�ڵ������Ϣ
	cout << "car " << num << " from South arrives at crossing" << endl;

	//�����Ѻ����ȼ���Լ��Ƿ�������
	pthread_mutex_lock(&mtx);
	deadlock_detect--;
	//����������
	if (!deadlock_detect) {
		pthread_mutex_unlock(&mtx);

		//��ǵ������������ĳ������򣬻������������߳�
		di = south;
		pthread_cond_signal(&deadlock);

		//ͣ��·���⣬�ȴ������������������
		pthread_mutex_lock(&mtx_s);
		pthread_cond_wait(&dead_solve, &mtx_s);
		//������೵�ȴ����ó������������ͨ��ʱ���ᷢ��һ�������źţ����մ��ź�
		pthread_cond_wait(&s_go, &mtx_s);
		pthread_mutex_unlock(&mtx_s);
	}
	else
		pthread_mutex_unlock(&mtx);

	//���ó����ᵼ������������·�ڣ���������ռ��һ��·��
	//��������·��ʱ�����еĳ���������
	from_s.count--;
	pthread_mutex_lock(&a);
	//����·�ںķ�ʱ��
	usleep(100);

	//��ʱ����ҷ��Ƿ���������������ȴ�
	//�ҷ�����ͨ������������һ�������ź�
	//���ҷ������ᵼ���������ҷ������߳̽��ỽ����������߳�
	//��������̻߳ᷢ�������ź�
	//��һ������ֵ����Լ��Ƿ������ȴ�����������ͨ�������ҷ���һ����
	bool waited = false;
	if (e_coming) {
		//�������ȴ�
		waited = true;
		//�ȴ������ź�
		pthread_mutex_lock(&mtx_s);
		pthread_cond_wait(&s_go, &mtx_s);
		pthread_mutex_unlock(&mtx_s);
	}
	//��������źź���ҷ�����������ռ�ڶ���·�ڣ��ͷŵ�һ��·��
	pthread_mutex_lock(&b);
	pthread_mutex_unlock(&a);
	deadlock_detect++;
	//����·�ںķ�ʱ��
	usleep(100);

	//����ȴ����ҷ��������ҷ���Ȼ�г����������������źţ����ҳ�ͬʱ�н������໥����
	//�����Ƿ�������������ʱ�ȴ���·������ҳ���Ϊ��һ�µ�
	if (waited && from_e.count)
		pthread_cond_signal(&e_go);
	//�����г��ȴ�����������һ�������źţ����������е���һ�������������ź�
	//�󷽵ȴ��ĳ�������·���ڣ�����������Ƿ���������г�
	if (w_coming)
		pthread_cond_signal(&w_go);
	//�����޳������Լ��Ķ�������һ�������������ź�
	else if (from_s.count)
		pthread_cond_signal(&s_go);
	//���ҷ����������ҷ������յ����Ѿ��ı������ʾ�ҷ�������
	//��Ȼ��������ʱ��δ�ͷŵڶ���·�ڣ�����һ�������Լ�⵽�ҷ���������������֮����·��

	//�ͷŵڶ���·�ڣ��뿪
	cout << "car " << num << " from South leaving crossing" << endl;
	pthread_mutex_unlock(&b);
}

void* manage_w(void*) {
	//������
	pthread_mutex_lock(&mtx_w_in);
	//��·����ȴ������źŻ���,�յ������źź������һ��
	pthread_cond_wait(&w_go, &mtx_w_in);
	pthread_mutex_unlock(&mtx_w_in);

	//��Ǹ÷����г����룬�󷽳��������鵽��ֵΪ�棬��ͣ�����һ��·���ڵȴ�����ͨ��
	w_coming = true;
	//�Ӷ�Ӧ���������ȡ��ͷ��
	int num = from_w.car_leave();
	//�����������·�ڵ������Ϣ
	cout << "car " << num << " from West arrives at crossing" << endl;

	//�����Ѻ����ȼ���Լ��Ƿ�������
	pthread_mutex_lock(&mtx);
	deadlock_detect--;
	//����������
	if (!deadlock_detect) {
		pthread_mutex_unlock(&mtx);

		//��ǵ������������ĳ������򣬻������������߳�
		di = west;
		pthread_cond_signal(&deadlock);

		//ͣ��·���⣬�ȴ������������������
		pthread_mutex_lock(&mtx_w);
		pthread_cond_wait(&dead_solve, &mtx_w);
		//������೵�ȴ����ó������������ͨ��ʱ���ᷢ��һ�������źţ����մ��ź�
		pthread_cond_wait(&w_go, &mtx_w);
		pthread_mutex_unlock(&mtx_w);
	}
	else
		pthread_mutex_unlock(&mtx);

	//���ó����ᵼ������������·�ڣ���������ռ��һ��·��
	//��������·��ʱ�����еĳ���������
	from_w.count--;
	pthread_mutex_lock(&d);
	//����·�ںķ�ʱ��
	usleep(100);

	//��ʱ����ҷ��Ƿ���������������ȴ�
	//�ҷ�����ͨ������������һ�������ź�
	//���ҷ������ᵼ���������ҷ������߳̽��ỽ����������߳�
	//��������̻߳ᷢ�������ź�
	//��һ������ֵ����Լ��Ƿ������ȴ�����������ͨ�������ҷ���һ����
	bool waited = false;
	if (s_coming) {
		//�������ȴ�
		waited = true;
		//�ȴ������ź�
		pthread_mutex_lock(&mtx_w);
		pthread_cond_wait(&w_go, &mtx_w);
		pthread_mutex_unlock(&mtx_w);
	}
	//��������źź���ҷ�����������ռ�ڶ���·�ڣ��ͷŵ�һ��·��
	pthread_mutex_lock(&a);
	pthread_mutex_unlock(&d);
	deadlock_detect++;
	//����·�ںķ�ʱ��
	usleep(100);

	//����ȴ����ҷ��������ҷ���Ȼ�г����������������źţ����ҳ�ͬʱ�н������໥����
	//�����Ƿ�������������ʱ�ȴ���·������ҳ���Ϊ��һ�µ�
	if (waited && from_s.count)
		pthread_cond_signal(&s_go);
	//�����г��ȴ�����������һ�������źţ����������е���һ�������������ź�
	//�󷽵ȴ��ĳ�������·���ڣ�����������Ƿ���������г�
	if (n_coming)
		pthread_cond_signal(&n_go);
	//�����޳������Լ��Ķ�������һ�������������ź�
	else if (from_w.count)
		pthread_cond_signal(&w_go);
	//���ҷ����������ҷ������յ����Ѿ��ı������ʾ�ҷ�������
	//��Ȼ��������ʱ��δ�ͷŵڶ���·�ڣ�����һ�������Լ�⵽�ҷ���������������֮����·��

	//�ͷŵڶ���·�ڣ��뿪
	cout << "car " << num << " from West leaving crossing" << endl;
	pthread_mutex_unlock(&a);
}

void* manage_n(void*) {
	//������
	pthread_mutex_lock(&mtx_n_in);
	//��·����ȴ������źŻ���,�յ������źź������һ��
	pthread_cond_wait(&n_go, &mtx_n_in);
	pthread_mutex_unlock(&mtx_n_in);

	//��Ǹ÷����г����룬�󷽳��������鵽��ֵΪ�棬��ͣ�����һ��·���ڵȴ�����ͨ��
	n_coming = true;
	//�Ӷ�Ӧ���������ȡ��ͷ��
	int num = from_n.car_leave();
	//�����������·�ڵ������Ϣ
	cout << "car " << num << " from North arrives at crossing" << endl;

	//�����Ѻ����ȼ���Լ��Ƿ�������
	pthread_mutex_lock(&mtx);
	deadlock_detect--;
	//����������
	if (!deadlock_detect) {
		pthread_mutex_unlock(&mtx);

		//��ǵ������������ĳ������򣬻������������߳�
		di = north;
		pthread_cond_signal(&deadlock);

		//ͣ��·���⣬�ȴ������������������
		pthread_mutex_lock(&mtx_n);
		pthread_cond_wait(&dead_solve, &mtx_n);
		//������೵�ȴ����ó������������ͨ��ʱ���ᷢ��һ�������źţ����մ��ź�
		pthread_cond_wait(&n_go, &mtx_n);
		pthread_mutex_unlock(&mtx_n);
	}
	else
		pthread_mutex_unlock(&mtx);

	//���ó����ᵼ������������·�ڣ���������ռ��һ��·��
	//��������·��ʱ�����еĳ���������
	from_n.count--;
	pthread_mutex_lock(&c);
	//����·�ںķ�ʱ��
	usleep(100);

	//��ʱ����ҷ��Ƿ���������������ȴ�
	//�ҷ�����ͨ������������һ�������ź�
	//���ҷ������ᵼ���������ҷ������߳̽��ỽ����������߳�
	//��������̻߳ᷢ�������ź�
	//��һ������ֵ����Լ��Ƿ������ȴ�����������ͨ�������ҷ���һ����
	bool waited = false;
	if (w_coming) {
		//�������ȴ�
		waited = true;
		//�ȴ������ź�
		pthread_mutex_lock(&mtx_n);
		pthread_cond_wait(&n_go, &mtx_n);
		pthread_mutex_unlock(&mtx_n);
	}
	//��������źź���ҷ�����������ռ�ڶ���·�ڣ��ͷŵ�һ��·��
	pthread_mutex_lock(&d);
	pthread_mutex_unlock(&c);
	deadlock_detect++;
	//����·�ںķ�ʱ��
	usleep(100);

	//����ȴ����ҷ��������ҷ���Ȼ�г����������������źţ����ҳ�ͬʱ�н������໥����
	//�����Ƿ�������������ʱ�ȴ���·������ҳ���Ϊ��һ�µ�
	if (waited && from_w.count)
		pthread_cond_signal(&w_go);
	//�����г��ȴ�����������һ�������źţ����������е���һ�������������ź�
	//�󷽵ȴ��ĳ�������·���ڣ�����������Ƿ���������г�
	if (e_coming)
		pthread_cond_signal(&e_go);
	//�����޳������Լ��Ķ�������һ�������������ź�
	else if (from_n.count)
		pthread_cond_signal(&n_go);
	//���ҷ����������ҷ������յ����Ѿ��ı������ʾ�ҷ�������
	//��Ȼ��������ʱ��δ�ͷŵڶ���·�ڣ�����һ�������Լ�⵽�ҷ���������������֮����·��

	//�ͷŵڶ���·�ڣ��뿪
	cout << "car " << num << " from North leaving crossing" << endl;
	pthread_mutex_unlock(&d);
}


void* solving_deadlock(void*) {
	//ʼ��ִ��
	while (1) {
		//�ȴ������������ʱ������
		pthread_mutex_lock(&deadl);
		pthread_cond_wait(&deadlock, &deadl);
		pthread_mutex_unlock(&deadl);
		//��ʱ��״̬Ϊ��ĳ������׼�������һ��·�ڣ���������������ڵ�һ��·���ڵȴ������ź�
		//��deadlock��Ϣ
		cout << "DEADLOCK: car jam detected, signalling ";
		//��δ����·�ڷ������೵�����������ź�
		switch (di)
		{
		case east: cout << "South to go" << endl; pthread_cond_signal(&s_go); break;
		case south: cout << "West to go" << endl; pthread_cond_signal(&w_go); break;
		case west: cout << "North to go" << endl; pthread_cond_signal(&n_go); break;
		case north: cout << "East to go" << endl; pthread_cond_signal(&e_go); break;
		default:
			break;
		}

		//�ȴ��׸������ĳ��ͷ���Դ����ͬʱ���ỽ����һ���ȴ��ĳ�
		while (!deadlock_detect);
		//�ͷ���Դ����������������ź���δ����·�ڵĳ�����
		pthread_cond_signal(&dead_solve);
	}
}

int main() {
	//��������
	string carlist;
	cin >> carlist;
	
	//����������У���Ϊÿ���������߳�
	//���������׸��������̻߳�ȡ���������ȴ���������
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

	//��������߳�
	pthread_t de_dt;
	pthread_create(&de_dt, 0, solving_deadlock, 0);

	//���Ѹ������׸��߳�
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
