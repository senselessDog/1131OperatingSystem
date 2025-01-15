#include <iostream>
#include <pthread.h>
#include <getopt.h>
#include <string.h>
#include <sched.h>
#include <vector>
#include <chrono>
using namespace std;

// 全域變數
int num_threads;
double time_wait;
vector<string> policies;
vector<int> priorities;
pthread_barrier_t start_barrier;
pthread_barrier_t round_barrier;
vector<pthread_t> threads;

// Busy waiting 函數
void busy_wait(int thread_id, double seconds) {
    printf("Thread %d is starting\n", thread_id);
    auto start = chrono::high_resolution_clock::now();
    while (true) {
        auto current = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed = current - start;
        if (elapsed.count() >= seconds) break;
    }
}

// Worker thread 函數
void *thread_func(void *arg) {
    int thread_id = *(int*)arg;
    // 在線程函數中設置 CPU 親和性
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    // 1. 等待所有執行緒就緒
    pthread_barrier_wait(&start_barrier);
    
    
    // 2. 執行任務
    for (int i = 0; i < 3; i++) {
        // printf("Thread %d is starting\n", thread_id);
        busy_wait(thread_id, time_wait);
        //printf("The loop %d of Thread %d is ending\n", i, thread_id);

	//new barrier
	//pthread_barrier_wait(&round_barrier);
    }
    
    // 3. 結束
    delete (int*)arg;
    return NULL;
}

int main(int argc, char *argv[]) {
    // 1. 解析程式參數
    int opt;
    //printf("Main Thread begin\n");
    while ((opt = getopt(argc, argv, "n:t:s:p:")) != -1) {
        switch (opt) {
            case 'n':
                num_threads = atoi(optarg);
                break;
            case 't':
                time_wait = atof(optarg);
                break;
            case 's': {
                char *token = strtok(optarg, ",");
                while (token != NULL) {
                    policies.push_back(string(token));
                    token = strtok(NULL, ",");
                }
                break;
            }
            case 'p': {
                char *token = strtok(optarg, ",");
                while (token != NULL) {
                    priorities.push_back(atoi(token));
                    token = strtok(NULL, ",");
                }
                break;
            }
        }
    }
    //check parse correct proiorities
    //printf("Thread begin\n");
    //printf("Priorities size: %zu\n", priorities.size());
    //for (int i = 0; i < (int)priorities.size(); i++) {
    //    printf("Number %d priorities: %d\n", i, priorities[i]);
    //}
    // 2. 初始化 barrier
    pthread_barrier_init(&start_barrier, NULL, num_threads+1);
    
    // 3. 設定 CPU 親和性
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);

    // 4. 建立thread
    threads.resize(num_threads);
    for (int i = 0; i < num_threads; i++) {
        // 設定thread attribute
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        
        // 設定排程策略和優先權
        sched_param param;
        if (policies[i] == "FIFO") {
	    //printf("Number %d priorities: %d & belong to FIFO\n", i, priorities[i]);
            pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
            param.sched_priority = priorities[i];
        } else {
            pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
            param.sched_priority = -1;
        }
        pthread_attr_setschedparam(&attr, &param);

	struct sched_param actual_param;
    	int actual_policy;
    	pthread_attr_getschedparam(&attr, &actual_param);
    	pthread_attr_getschedpolicy(&attr, &actual_policy);
    	//printf("Thread %d setup - Policy: %d, Priority: %d\n",
          // i, actual_policy, actual_param.sched_priority);
        
        // 建立執行緒
        int *id = new int(i);
        pthread_create(&threads[i], &attr, thread_func, id);
        pthread_attr_destroy(&attr);
        //check set correct param 
	    sched_param current_param;
    	int current_policy;
    	pthread_getschedparam(threads[i], &current_policy, &current_param);
    	//printf("Thread %d actual - Policy: %d, Priority: %d\n",
           //i, current_policy, current_param.sched_priority);
    }
    pthread_barrier_wait(&start_barrier);
    // 5. 等待所有執行緒結束
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // 6. 清理資源
    pthread_barrier_destroy(&start_barrier);
    
    return 0;
}
